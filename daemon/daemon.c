#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "../common.h"

#define STATE_MACHINE_MAX_STATE_COUNT 64
#define STATE_MACHINE_MAX_KEYWORD_COMMAND_COUNT 16
#define STATE_MACHINE_MAX_KEYWORD_LENGTH CLI_PARAMETER_NAME_MAX_LENGHT
#define STATE_MACHINE_MAX_COMMAND_LENGTH COMMAND_INPUT_MAX_LENGTH
#define PIPE_LOCATION "/run/user/%d/%s.daemon-pipe" // replace with getuid() and pipeName
#define CLI_ACTION_HELP "--help"
#define CLI_ACTION_SEND "--send"
#define CLI_ACTION_RECEIVE "--receive"
#define CLI_VALUE_PIPE_NAME "--pipe-name"
#define CLI_VALUE_REUSE_PIPE "--reuse-pipe"
#define CLI_VALUE_MESSAGE "--message"
#define CLI_VALUE_CONFIG_FILE "--config-file"

typedef struct stateMachine StateMachine;
struct stateMachine {
    char stateName[STATE_MACHINE_MAX_COMMAND_LENGTH];
    int keywordCommandCount;
    char stateKeywords[STATE_MACHINE_MAX_KEYWORD_COMMAND_COUNT][STATE_MACHINE_MAX_KEYWORD_LENGTH];
    char stateCommands[STATE_MACHINE_MAX_KEYWORD_COMMAND_COUNT][STATE_MACHINE_MAX_COMMAND_LENGTH];
};

typedef struct threadData ThreadData;
struct threadData {
    char pipeLocation[COMMAND_INPUT_MAX_LENGTH];
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
int receive(char*, char*, char*);

int main(int argc, char** argv) {
    char* target = NULL;
    char action[CLI_PARAMETER_NAME_MAX_LENGHT] = CLI_ACTION_HELP;
    char pipeName[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;
    char reusePipe[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;
    char message[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;
    char configFile[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_TEXT_DEFAULT;

    // get cli arguments to the right places
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], CLI_ACTION_HELP)) {
            target = NULL;
            strncpy(action, CLI_ACTION_HELP, CLI_PARAMETER_NAME_MAX_LENGHT);
            break;
        } else if (!strcmp(argv[i], CLI_ACTION_SEND)) {
            target = NULL;
            strncpy(action, CLI_ACTION_SEND, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_RECEIVE)) {
            target = NULL;
            strncpy(action, CLI_ACTION_RECEIVE, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_VALUE_PIPE_NAME)) {
            target = pipeName;
        } else if (!strcmp(argv[i], CLI_VALUE_REUSE_PIPE)) {
            target = reusePipe;
        } else if (!strcmp(argv[i], CLI_VALUE_MESSAGE)) {
            target = message;
        } else if (!strcmp(argv[i], CLI_VALUE_CONFIG_FILE)) {
            target = configFile;
        } else {
            if (target != NULL) {
                strncpy(target, argv[i], CLI_PARAMETER_VALUE_MAX_LENGHT);
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
        receive(pipeName, configFile, reusePipe);
    } else {
        help();
    }

    return 0;
}

// shows the help message
int help(void) {
    printf("daemon - CLI tool to make your polybar modules more versatile\n");
    printf("  daemon %s %s pipe-name %s config-file [%s yes/no]: create a receiver that will listen on pipe-name and behave as defined in config-file, reusing pipe-name if specified\n", CLI_ACTION_RECEIVE, CLI_VALUE_PIPE_NAME, CLI_VALUE_CONFIG_FILE, CLI_VALUE_REUSE_PIPE);
    printf("  daemon %s %s pipe-name %s message: sends message to the daemon listening in pipe-name\n", CLI_ACTION_SEND, CLI_VALUE_PIPE_NAME, CLI_VALUE_MESSAGE);
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

    char path[COMMAND_INPUT_MAX_LENGTH];
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

int receive(char* pipeName, char* configFile, char* reusePipe) {
    void* syncFunction(void* parameters) {
        ThreadData* threadData = (ThreadData*) parameters;
        char input[COMMAND_INPUT_MAX_LENGTH];
        char output[COMMAND_OUTPUT_MAX_LENGTH];
        int terminate = 0;

        // sync update the interface
        while (!terminate) {
            // pulls the current command
            pthread_mutex_lock(&(threadData->currentCommandLock));
            strncpy(input, threadData->currentCommand, COMMAND_INPUT_MAX_LENGTH);
            pthread_mutex_unlock(&(threadData->currentCommandLock));

            // runs the command received
            runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

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
        char input[COMMAND_INPUT_MAX_LENGTH];
        char output[COMMAND_OUTPUT_MAX_LENGTH];
        char file[FILE_OUTPUT_MAX_LENGTH];
        int terminate = 0;

        while (!terminate) {
            readTextFile(threadData->pipeLocation, file, FILE_OUTPUT_MAX_LENGTH);

            char* newCommand = NULL;
            // search for the command associated with the keyword received from the pipe in the current state
            for (int i = 0; i < threadData->stateMachine[threadData->currentState].keywordCommandCount; i++) {
                if (!strcmp(file, threadData->stateMachine[threadData->currentState].stateKeywords[i])) {
                    newCommand = threadData->stateMachine[threadData->currentState].stateCommands[i];
                    break;
                }
            }

            // if the keyword received was recognized
            if (newCommand != NULL) {
                // if a state transition is required, do it right away
                if (!strncmp("state", newCommand, strlen("state"))) {
                    // create a copy of the state change command (strtok_r will overwrite the original otherwise)
                    char stateChangeCommand[COMMAND_INPUT_MAX_LENGTH];
                    strncpy(stateChangeCommand, newCommand, COMMAND_INPUT_MAX_LENGTH);
                    // split 'state' from its name
                    for (char* name = NULL, * state = strtok_r(stateChangeCommand, " ", &name); state != NULL; state = strtok_r(NULL, " ", &name)) {
                        if (strcmp(state, "") && strcmp(name, "")) {
                            // printf("'%s' '%s'\n", state, name);
                            // search for the new state by its name
                            for (int i = 0; i < threadData->stateCount; i++) {
                                if (!strcmp(name, threadData->stateMachine[i].stateName)) {
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

                            break;
                        }
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
                    // execute the current command (won't show its output anywhere)
                    runCommand(newCommand, output, COMMAND_OUTPUT_MAX_LENGTH);

                    // pulls the current command
                    pthread_mutex_lock(&(threadData->currentCommandLock));
                    strncpy(input, threadData->currentCommand, COMMAND_INPUT_MAX_LENGTH);
                    pthread_mutex_unlock(&(threadData->currentCommandLock));

                    // execute the update command (to reflect immediately any changes that newCommand may have done)
                    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

                    // draw output on screen
                    pthread_mutex_lock(&(threadData->screenLock));
                    printf("%s", output);
                    fflush(stdout);
                    pthread_mutex_unlock(&(threadData->screenLock));
                }
            }

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
    // reusePipe is optional
    // if (!strcmp(reusePipe, CLI_TEXT_DEFAULT)) {
    //     printf("Missing %s parameter!\n", CLI_VALUE_REUSE_PIPE);
    //     fflush(stdout);
    //     return 1;
    // }

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
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    char file[FILE_OUTPUT_MAX_LENGTH];
    char path[COMMAND_INPUT_MAX_LENGTH];

    // if reusePipe is different than yes, prevent using an existing pipe
    sprintf(path, PIPE_LOCATION, getuid(), pipeName);
    if (strcmp(reusePipe, "yes")) {
        if (fileExists(path)) {
            printf("The pipe %s is already being used! Please, use another name or, if you want to reuse it, add the cli option %s yes.\n", pipeName, CLI_VALUE_REUSE_PIPE);
            fflush(stdout);
            return 1;
        }
    }

    // create pipe
    if (!fileExists(path)) {
        sprintf(input, "mkfifo %s", path);
        runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);
    }

    // load state machine from config file
    readTextFile(configFile, file, FILE_OUTPUT_MAX_LENGTH);
    // for each line of the config file
    for (char* lineRest = NULL, * line = strtok_r(file, "\n", &lineRest); line != NULL; line = strtok_r(NULL, "\n", &lineRest)) {
        printf("'%s'\n", line);
        // break the line in every space character
        for (char* wordRest = NULL, * word = strtok_r(line, " ", &wordRest); word != NULL; word = strtok_r(NULL, "\n", &wordRest)) {
            // if the word is not empty, consider the keyword:command pair found and stop searching
            if (strcmp(word, " ") && strcmp(wordRest, " ") && statesCount < STATE_MACHINE_MAX_STATE_COUNT) {
                // if keyword is state, a new state is created
                if (!strcmp(word, "state")) {
                    strncpy(stateMachine[statesCount].stateName, wordRest, STATE_MACHINE_MAX_COMMAND_LENGTH);
                    stateMachine[statesCount].keywordCommandCount = 0;
                    statesCount++;
                // else is a normal keyword, add a keyword:command pair if there is at least one state created and the number of keyword:command pairs of this state is within the maximum allowed
                } else if (statesCount > 0 && stateMachine[statesCount - 1].keywordCommandCount < STATE_MACHINE_MAX_STATE_COUNT) {
                    strncpy(stateMachine[statesCount - 1].stateKeywords[stateMachine[statesCount - 1].keywordCommandCount], word, STATE_MACHINE_MAX_KEYWORD_LENGTH);
                    strncpy(stateMachine[statesCount - 1].stateCommands[stateMachine[statesCount - 1].keywordCommandCount], wordRest, STATE_MACHINE_MAX_COMMAND_LENGTH);
                    stateMachine[statesCount - 1].keywordCommandCount++;
                }     

                break;
            }
        }
    }
    
    threadData.stateMachine = stateMachine;
    strncpy(threadData.pipeLocation, path, COMMAND_INPUT_MAX_LENGTH);
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
        printf("Could not create screen mutex lock!\n");
        fflush(stdout);
        return 1;
    }
    if (pthread_mutex_init(&(threadData.terminateLock), NULL)) {
        printf("Could not create terminate mutex lock!\n");
        fflush(stdout);
        return 1;
    }
    if (pthread_mutex_init(&(threadData.currentStateLock), NULL)) {
        printf("Could not create current state mutex lock!\n");
        fflush(stdout);
        return 1;
    }
    if (pthread_mutex_init(&(threadData.currentCommandLock), NULL)) {
        printf("Could not create currentCommand mutex lock!\n");
        fflush(stdout);
        return 1;
    }

    // initialize threads
    if (pthread_create(&syncThread, NULL, syncFunction, &threadData)) {
        printf("Could not start sync thread!\n");
        fflush(stdout);
        return 1;
    }
    if (pthread_create(&asyncThread, NULL, asyncFunction, &threadData)) {
        printf("Could not start async thread!\n");
        fflush(stdout);
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
    sprintf(input, "rm %s", path);
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}