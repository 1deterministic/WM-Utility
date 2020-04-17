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
    pthread_mutex_t screenLock;
    pthread_mutex_t terminateLock;
    int terminate;
    char pipeLocation[COMMAND_INPUT_MAX_LENGHT];
    pthread_mutex_t currentStateLock;
    int stateCount;
    int currentState;
    StateMachine* stateMachine;
    pthread_mutex_t currentCommandLock;
    char* currentCommand;
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
    int invalidParameters = 0;
    if (!strcmp(pipeName, CLI_TEXT_DEFAULT)) {
        invalidParameters = 1;
        printf("Missing %s parameter!\n", CLI_VALUE_PIPE_NAME);
        fflush(stdout);
    }
    if (!strcmp(message, CLI_TEXT_DEFAULT)) {
        invalidParameters = 1;
        printf("Missing %s parameter!\n", CLI_VALUE_MESSAGE);
        fflush(stdout);
    }

    // stop if any invalid parameter was found
    if (invalidParameters) {
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
        char output[COMMAND_OUTPUT_MAX_LENGHT] = "\0";
        int terminate = 0;

        // sync update the interface
        while (!terminate) {
            // reset contents of output to prevent being concatenated by the next readTextFile
            sprintf(output, "\0");
            // execute the current command
            pthread_mutex_lock(&(threadData->currentCommandLock));
            runCommand(threadData->currentCommand, output);
            pthread_mutex_unlock(&(threadData->currentCommandLock));

            // acquire lock to draw on screen, draw message, then release it
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
        char output[COMMAND_OUTPUT_MAX_LENGHT] = "\0";
        int terminate = 0;

        while (!terminate) {
            // read pipe contents (this will be on hold until something is sent to the pipe)
            sprintf(output, "\0");
            readTextFile(threadData->pipeLocation, output);


            char* newCommand = NULL;
            // get the new command
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
                    // create a copy of the current command (strtok_r will override the original otherwise)
                    char currentCommand[COMMAND_INPUT_MAX_LENGHT];
                    strcpy(currentCommand, newCommand);

                    // split 'state' from 'name' (blank space)
                    char* stateWord = currentCommand;
                    char* stateName = stateWord;
                    stateWord = strtok_r(stateName, " ", &stateName);

                    // search for the new state
                    for (int i = 0; i < threadData->stateCount; i++) {
                        if (!strcmp(stateName, threadData->stateMachine[i].stateName)) {
                            pthread_mutex_lock(&(threadData->currentStateLock));
                            threadData->currentState = i;
                            pthread_mutex_unlock(&(threadData->currentStateLock));

                            // search for the update command 
                            for (int j = 0; j < threadData->stateMachine[i].keywordCommandCount; j++) {
                                if (!strcmp(threadData->stateMachine[i].stateKeywords[j], "update")) {
                                    pthread_mutex_lock(&(threadData->currentCommandLock));
                                    threadData->currentCommand = threadData->stateMachine[i].stateCommands[j];
                                    pthread_mutex_unlock(&(threadData->currentCommandLock));
                                    newCommand = threadData->stateMachine[i].stateCommands[j];
                                }
                            }
                        }
                    }
                }
                
                if (!strcmp(newCommand, "terminate")) {
                    pthread_mutex_lock(&(threadData->terminateLock));
                    threadData->terminate = 1;
                    pthread_mutex_unlock(&(threadData->terminateLock));
                } else {
                    // reset contents of output to prevent being concatenated by the next readTextFile
                    sprintf(output, "\0");
                    // execute the current command
                    pthread_mutex_lock(&(threadData->currentCommandLock));
                    runCommand(newCommand, output);
                    pthread_mutex_unlock(&(threadData->currentCommandLock));

                    // // acquire lock to draw on screen, draw message, then release it
                    // pthread_mutex_lock(&(threadData->screenLock));
                    // printf("%s", output);
                    // fflush(stdout);
                    // pthread_mutex_unlock(&(threadData->screenLock));
                }
            }

            // reset contents of output to prevent being concatenated by the next readTextFile
            sprintf(output, "\0");
            // execute the update command
            pthread_mutex_lock(&(threadData->currentCommandLock));
            runCommand(threadData->currentCommand, output);
            pthread_mutex_unlock(&(threadData->currentCommandLock));

            // acquire lock to draw on screen, draw message, then release it
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
    int invalidParameters = 0;
    if (!strcmp(pipeName, CLI_TEXT_DEFAULT)) {
        invalidParameters = 1;
        printf("Missing %s parameter!\n", CLI_VALUE_PIPE_NAME);
        fflush(stdout);
    }
    if (!strcmp(configFile, CLI_TEXT_DEFAULT)) {
        invalidParameters = 1;
        printf("Missing %s parameter!\n", CLI_VALUE_CONFIG_FILE);
        fflush(stdout);
    }
    if (!fileExists(configFile)) {
        invalidParameters = 1;
        printf("The config file %s does not exist!\n", configFile);
        fflush(stdout);
    }

    // stop if any invalid parameter was found
    if (invalidParameters) {
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

    char* line = output;
    char* nextLine = line;
    while ((line = strtok_r(nextLine, "\n", &nextLine))) {
        char* keyword = line;
        char* command = keyword;
        keyword = strtok_r(command, " ", &command);

        while (!strncmp(" ", keyword, strlen(" "))) {
            keyword = strtok_r(command, " ", &command);
        }

        // ignore a line starting with # or \n
        if (!strncmp("#", keyword, strlen("#")) || !strncmp("\n", keyword, strlen("\n"))) {
            continue;
        }

        // create a new state with the specified name
        if (!strcmp(keyword, "state")) {
            statesCount++;
            strcpy(stateMachine[statesCount - 1].stateName, command);
            stateMachine[statesCount - 1].keywordCommandCount = 0;
        // else is a keyword
        } else {
            // ignore keyword if no state was created
            if (statesCount <= 0) {
                continue;
            }

            // sanity check
            if (!strcmp(keyword, "") || !strcmp(command, "")) {
                continue;
            }

            stateMachine[statesCount - 1].keywordCommandCount++;
            strcpy(stateMachine[statesCount - 1].stateKeywords[stateMachine[statesCount - 1].keywordCommandCount - 1], keyword);
            strcpy(stateMachine[statesCount - 1].stateCommands[stateMachine[statesCount - 1].keywordCommandCount - 1], command);
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

    // create pipe
    char path[COMMAND_INPUT_MAX_LENGHT] = "\0";    
    sprintf(path, PIPE_LOCATION, getuid(), pipeName);

    // prevent using an existing pipe
    if (fileExists(path)) {
        printf("The pipe %s is already being used! Please, use another name.\n", pipeName);
        fflush(stdout);
        return 1;
    }

    sprintf(input, "\0");
    sprintf(input, "mkfifo %s", path);
    runCommand(input, output);

    threadData.terminate = 0;
    threadData.currentState = 0;
    threadData.stateCount = statesCount;
    threadData.currentCommand = stateMachine[0].stateCommands[0];
    threadData.stateMachine = stateMachine;
    strcpy(threadData.pipeLocation, path);

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

    sprintf(input, "\0");
    sprintf(input, "rm %s", path);
    sprintf(output, "\0");
    runCommand(input, output);

    return 0;
}