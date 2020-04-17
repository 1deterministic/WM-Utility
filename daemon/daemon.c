#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "../common.h"

#define STATE_MACHINE_MAX_STATE_COUNT 8
#define PIPE_LOCATION "/run/user/%d/%s" // replace with getuid() and pipeName
#define CLI_ACTION_HELP "--help"
#define CLI_ACTION_SEND "--send"
#define CLI_ACTION_RECEIVE "--receive"
#define CLI_VALUE_PIPE_NAME "--pipe-name"
#define CLI_VALUE_MESSAGE "--message"
#define CLI_VALUE_CONFIG_FILE "--config-file"

typedef struct stateMachine StateMachine;
struct stateMachine {
    char stateName[CLI_ACTION_MAX_LENGHT];
    int keywordCommandCount;
    char stateKeywords[CLI_ACTION_MAX_LENGHT][CLI_ACTION_MAX_LENGHT];
    char stateCommands[CLI_ACTION_MAX_LENGHT][COMMAND_INPUT_MAX_LENGHT];
};

typedef struct threadData ThreadData;
struct threadData {
    char pipeLocation[COMMAND_INPUT_MAX_LENGHT];
    StateMachine* stateMachine;
    int stateCount;
    int currentState;
    pthread_mutex_t currentStateLock;
    int terminate;
    pthread_mutex_t terminateLock;
    char* currentCommand;
    pthread_mutex_t currentCommandLock;
    pthread_mutex_t screenLock;
};

int help(void);
int send(char*, char*);
int receive(char*, char*);

int main(int argc, char** argv) {
    char* target = NULL;
    char action[CLI_ACTION_MAX_LENGHT] = CLI_ACTION_HELP;
    char pipeName[CLI_TEXT_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;
    char message[CLI_TEXT_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;
    char configFile[CLI_TEXT_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;

    // get cli arguments to the right places
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], CLI_ACTION_HELP)) {
            target = NULL;
            strcpy(action, CLI_ACTION_HELP);
            break;
        } else if (!strcmp(argv[i], CLI_ACTION_SEND)) {
            target = NULL;
            strcpy(action, CLI_ACTION_SEND);
        } else if (!strcmp(argv[i], CLI_ACTION_RECEIVE)) {
            target = NULL;
            strcpy(action, CLI_ACTION_RECEIVE);
        } else if (!strcmp(argv[i], CLI_VALUE_PIPE_NAME)) {
            target = pipeName;
        } else if (!strcmp(argv[i], CLI_VALUE_MESSAGE)) {
            target = message;
        } else if (!strcmp(argv[i], CLI_VALUE_CONFIG_FILE)) {
            target = configFile;
        } else {
            if (target != NULL) {
                strcpy(target, argv[i]);
            }
            target = NULL;
        }
    }

    // execute the desired action
    if (!strcmp(action, CLI_ACTION_HELP)) {
        help();
    } else if (!strcmp(action, CLI_ACTION_SEND)) {
        send(pipeName, message);
    } else if (!strcmp(action, CLI_ACTION_RECEIVE)) {
        receive(pipeName, configFile);
    } else {
        help();
    }

    return 0;
}

// shows the help message
int help(void) {
    printf("daemon - CLI tool to ...\n");
    fflush(stdout);

    return 0;
}

int send(char* pipeName, char* message) {
    // validates all received parameters
    if (!strcmp(pipeName, CLI_TEXT_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_PIPE_NAME);
        fflush(stdout);
        return 1;
    }
    if (!strcmp(message, CLI_TEXT_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_MESSAGE);
        fflush(stdout);
        return 1;
    }

    char path[COMMAND_INPUT_MAX_LENGHT];
    sprintf(path, PIPE_LOCATION, getuid(), pipeName);

    // prevent creating a file if the pipe is not created yet
    if (!fileExists(path)) {
        printf("The pipe %s does not exist! Please, start an instance of this daemon in %s mode first.\n", pipeName, CLI_ACTION_RECEIVE);
        fflush(stdout);
        return 1;
    }

    // sends the message to the pipe
    if (writeTextFile(path, message, 1)) {
        printf("Could not write to the pipe %d!\n", pipeName);
        fflush(stdout);
        return 1;
    }

    return 0;
}

int receive(char* pipeName, char* configFile) {
    void* syncFunction(void* parameters) {
        ThreadData* threadData = (ThreadData*) parameters;
        char input[COMMAND_INPUT_MAX_LENGHT] = "\0";
        char output[COMMAND_OUTPUT_MAX_LENGHT] = "\0";
        int terminate = 0;

        // sync update the interface
        while (!terminate) {
            // reset contents of input to prevent being concatenated by strcpy
            sprintf(input, "\0");
            // pulls the current command
            pthread_mutex_lock(&(threadData->currentCommandLock));
            strcpy(input, threadData->currentCommand);
            pthread_mutex_unlock(&(threadData->currentCommandLock));

            // reset contents of output to prevent being concatenated by readTextFile
            sprintf(output, "\0");
            // runs the command received
            runCommand(input, output);

            // draw command output on the screen
            pthread_mutex_lock(&(threadData->screenLock));
            printf("%s", output);
            fflush(stdout);
            pthread_mutex_unlock(&(threadData->screenLock));

            // wait a second before continuing
            sleep(1);

            // update terminate with data from threadData
            pthread_mutex_lock(&(threadData->terminateLock));
            terminate = threadData->terminate;
            pthread_mutex_unlock(&(threadData->terminateLock));
        }

        return NULL;
    }

    void* asyncFunction(void* parameters) {
        ThreadData* threadData = (ThreadData*) parameters;
        char input[COMMAND_INPUT_MAX_LENGHT] = "\0";
        char output[COMMAND_OUTPUT_MAX_LENGHT] = "\0";
        int terminate = 0;

        while (!terminate) {
            // read pipe contents (this will be on hold until something is sent to the pipe)
            sprintf(output, "\0");
            readTextFile(threadData->pipeLocation, output);

            char* newCommand = NULL;
            // search for the command associated with the keyword received from the pipe in the current state
            for (int i = 0; i < threadData->stateMachine[threadData->currentState].keywordCommandCount; i++) {
                if (!strcmp(output, threadData->stateMachine[threadData->currentState].stateKeywords[i])) {
                    newCommand = threadData->stateMachine[threadData->currentState].stateCommands[i];
                    break;
                }
            }

            // if the keyword received was recognized
            if (newCommand != NULL) {
                // if a state transition is required, do it right away
                if (!strncmp("state", newCommand, strlen("state"))) {
                    // create a copy of the state change command (strtok_r will overwrite the original otherwise)
                    char stateChangeCommand[COMMAND_INPUT_MAX_LENGHT];
                    strcpy(stateChangeCommand, newCommand);

                    // split 'state' from 'name' (blank space)
                    char* stateWord = stateChangeCommand;
                    char* stateName = stateWord;
                    stateWord = strtok_r(stateName, " ", &stateName);

                    // search for the new state only if the state isn't empty and doesn't start with a space character
                    if (strcmp(stateName, "") && strncmp(" ", stateName, strlen(" "))) {
                        // search for the new state by its name
                        for (int i = 0; i < threadData->stateCount; i++) {
                            if (!strcmp(stateName, threadData->stateMachine[i].stateName)) {
                                // update the current state to the new one
                                pthread_mutex_lock(&(threadData->currentStateLock));
                                threadData->currentState = i;
                                pthread_mutex_unlock(&(threadData->currentStateLock));

                                // search for the update command 
                                for (int j = 0; j < threadData->stateMachine[i].keywordCommandCount; j++) {
                                    if (!strcmp(threadData->stateMachine[i].stateKeywords[j], "update")) {
                                        // update the current command to the update command of the new state
                                        pthread_mutex_lock(&(threadData->currentCommandLock));
                                        threadData->currentCommand = threadData->stateMachine[i].stateCommands[j];
                                        pthread_mutex_unlock(&(threadData->currentCommandLock));

                                        // newCommand is storing a state change, this will replace it with the update command of the new state (for it to be executted below)
                                        newCommand = threadData->stateMachine[i].stateCommands[j];
                                        break;
                                    }
                                }

                                break;
                            }
                        }
                        // if no update command was found, the current one will be used
                    }
                }
                // sanity check
                if (!strncmp("state", newCommand, strlen("state"))) {
                    // something went really wrong
                // terminate command received
                } else if (!strcmp(newCommand, "terminate")) {
                    // update the current terminate flag for both threads
                    pthread_mutex_lock(&(threadData->terminateLock));
                    threadData->terminate = 1;
                    pthread_mutex_unlock(&(threadData->terminateLock));
                // any other command
                } else {
                    // reset contents of output to prevent being concatenated by runCommand
                    sprintf(output, "\0");
                    // execute the current command (won't show its output anywhere)
                    runCommand(newCommand, output);
                }
            }

            // reset contents of output to prevent being concatenated by runCommand
            sprintf(input, "\0");
            // pulls the current command
            pthread_mutex_lock(&(threadData->currentCommandLock));
            strcpy(input, threadData->currentCommand);
            pthread_mutex_unlock(&(threadData->currentCommandLock));

            // reset contents of output to prevent being concatenated by runCommand
            sprintf(output, "\0");
            // execute the update command (to reflect immediately any changes that newCommand may have done)
            runCommand(input, output);

            // draw output on screen
            pthread_mutex_lock(&(threadData->screenLock));
            printf("%s", output);
            fflush(stdout);
            pthread_mutex_unlock(&(threadData->screenLock));

            // update terminate with data from threadData
            pthread_mutex_lock(&(threadData->terminateLock));
            terminate = threadData->terminate;
            pthread_mutex_unlock(&(threadData->terminateLock));
        }
        
        return NULL;
    }

    // validates all received parameters
    if (!strcmp(pipeName, CLI_TEXT_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_PIPE_NAME);
        fflush(stdout);
        return 1;
    }
    if (!strcmp(configFile, CLI_TEXT_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_CONFIG_FILE);
        fflush(stdout);
        return 1;
    }
    if (!fileExists(configFile)) {
        printf("The config file %s does not exist!\n", configFile);
        fflush(stdout);
        return 1;
    }

    StateMachine stateMachine[STATE_MACHINE_MAX_STATE_COUNT];
    int statesCount = 0;
    ThreadData threadData;
    pthread_t syncThread;
    pthread_t asyncThread;
    char input[COMMAND_INPUT_MAX_LENGHT] = "\0";
    char output[COMMAND_OUTPUT_MAX_LINE_LENGHT] = "\0";

    // load state machine from config file
    readTextFile(configFile, output);

    // decode config file line by line
    char* line = output;
    char* nextLine = line;
    while ((line = strtok_r(nextLine, "\n", &nextLine))) {
        // split line in keyword and command on the first space character
        char* keyword = line;
        char* command = keyword;
        keyword = strtok_r(command, " ", &command);

        // this will ignore spaces at the start of the line from being considered keywords
        while (!strncmp(" ", keyword, strlen(" "))) {
            keyword = strtok_r(command, " ", &command);
        }

        // ignore a line starting with # or \n (comment or blank line)
        if (!strncmp("#", keyword, strlen("#")) || !strncmp("\n", keyword, strlen("\n"))) {
            continue;
        }

        // keyword is 'state': create a new state with the specified name
        if (!strcmp(keyword, "state")) {
            strcpy(stateMachine[statesCount].stateName, command);
            stateMachine[statesCount].keywordCommandCount = 0;
            statesCount++;
        // else is a keyword
        } else {
            // sanity check
            if (!strcmp(keyword, "") || !strcmp(command, "")) {
                continue;
            }

            // ignore if no state was created yet
            if (statesCount > 0) {
                stateMachine[statesCount - 1].keywordCommandCount++;
                strcpy(stateMachine[statesCount - 1].stateKeywords[stateMachine[statesCount - 1].keywordCommandCount - 1], keyword);
                strcpy(stateMachine[statesCount - 1].stateCommands[stateMachine[statesCount - 1].keywordCommandCount - 1], command);
            }
        }
    }

    // debug
    // for (int i = 0; i < statesCount; i++) {
    //     printf("State %d: name: %s\n", i, stateMachine[i].stateName);
    //     for (int j = 0; j < stateMachine[i].keywordCommandCount; j++) {
    //         printf("'%s' executes '%s'\n", stateMachine[i].stateKeywords[j], stateMachine[i].stateCommands[j]);
    //     }
    // }
    // return 0;

    // resolves the path where the pipe will be created
    char path[COMMAND_INPUT_MAX_LENGHT] = "\0";    
    sprintf(path, PIPE_LOCATION, getuid(), pipeName);

    // prevent using an existing pipe
    if (fileExists(path)) {
        printf("The pipe %s is already being used! Please, use another name.\n", pipeName);
        fflush(stdout);
        return 1;
    }

    // create pipe
    sprintf(input, "\0");
    sprintf(input, "mkfifo %s", path);
    runCommand(input, output);

    threadData.stateMachine = stateMachine;
    strcpy(threadData.pipeLocation, path);
    threadData.terminate = 0;
    threadData.currentState = 0;
    threadData.stateCount = statesCount;
    // search for the update command of the first state
    for (int i = 0; i < stateMachine[threadData.currentState].keywordCommandCount; i++) {
        if (!strcmp(stateMachine[threadData.currentState].stateKeywords[i], "update")) {
            threadData.currentCommand = stateMachine[threadData.currentState].stateCommands[i];
            break;
        }
    }

    // ensures there is an update command for the first state
    if (threadData.currentCommand == NULL) {
        printf("No update command for the first state!\n");
        fflush(stdout);
        return 1;
    }

    // initialize locks
    if (pthread_mutex_init(&(threadData.screenLock), NULL)) {
        return 1;
    }
    if (pthread_mutex_init(&(threadData.terminateLock), NULL)) {
        return 1;
    }
    if (pthread_mutex_init(&(threadData.currentStateLock), NULL)) {
        return 1;
    }
    if (pthread_mutex_init(&(threadData.currentCommandLock), NULL)) {
        return 1;
    }

    // initialize threads
    if (pthread_create(&syncThread, NULL, syncFunction, &threadData)) {
        return 1;
    }
    if (pthread_create(&asyncThread, NULL, asyncFunction, &threadData)) {
        return 1;
    }

    // wait for threads to finish
    pthread_join(asyncThread, NULL);
    pthread_join(syncThread, NULL);

    // destroy locks
    pthread_mutex_destroy(&(threadData.screenLock));
    pthread_mutex_destroy(&(threadData.terminateLock));
    pthread_mutex_destroy(&(threadData.currentStateLock));
    pthread_mutex_destroy(&(threadData.currentCommandLock));

    // remove pipe
    sprintf(input, "\0");
    sprintf(input, "rm %s", path);
    sprintf(output, "\0");
    runCommand(input, output);

    return 0;
}