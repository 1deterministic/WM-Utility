#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../common.h"

#define VOLUME_STEP 5
#define VOLUME_MAX 100
#define NUMBER_OF_DEVICES 32
#define STRING_SPLIT_SIZE 16
#define DEVICE_NAME_MAX_LENGTH COMMAND_OUTPUT_MAX_LINE_LENGTH
#define CLI_ACTION_HELP "--help"
#define CLI_ACTION_INFO "--info"
#define CLI_ACTION_GET_VOLUME_POLYBAR "--get-volume-polybar"
#define CLI_ACTION_GET_NAME_POLYBAR "--get-name-polybar"
#define CLI_ACTION_GET_VOLUME "--get-volume"
#define CLI_ACTION_SET_VOLUME "--set-volume"
#define CLI_ACTION_INCREASE_VOLUME "--increase-volume"
#define CLI_ACTION_DECREASE_VOLUME "--decrease-volume"
#define CLI_ACTION_TOGGLE_MUTE "--toggle-mute"
#define CLI_ACTION_CICLE_DEVICES "--cicle-devices"
#define CLI_ACTION_SET_DEVICE "--set-device"
#define CLI_VALUE_VOLUME "--volume"
#define CLI_VALUE_DEVICE "--device"

typedef struct audioDevice AudioDevice;
struct audioDevice {
    int deviceIndex;
    int deviceVolume;
    int deviceIsMuted;
    char deviceName[DEVICE_NAME_MAX_LENGTH];
    int deviceIsDefault;
};

// function prototypes
int getPulseaudioState(AudioDevice*, int*, int*);
int help(void);
int info(AudioDevice*, int);
int getVolumePolybar(AudioDevice*, int, int);
int getNamePolybar(AudioDevice*, int, int);
int getVolume(AudioDevice*, int, int);
int setVolume(AudioDevice*, int, int, char*);
int increaseVolume(AudioDevice*, int, int);
int decreaseVolume(AudioDevice*, int, int);
int toggleMute(AudioDevice*, int, int);
int cicleDevices(AudioDevice*, int, int);
int setDevice(AudioDevice*, int, int, char*);

// main
int main(int argc, char** argv) {
    int audioDevicesCount = 0;
    int defaultDeviceIndex = -1;
    AudioDevice audioDevices[NUMBER_OF_DEVICES];

    char* target = NULL;
    char action[CLI_PARAMETER_NAME_MAX_LENGHT] = CLI_ACTION_HELP;
    char volume[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_NUMERICAL_DEFAULT;
    char device[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_NUMERICAL_DEFAULT;

    // get cli arguments to the right places
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], CLI_ACTION_HELP)) {
            target = NULL;
            strncpy(action, CLI_ACTION_HELP, CLI_PARAMETER_NAME_MAX_LENGHT);
            break;
        } else if (!strcmp(argv[i], CLI_ACTION_INFO)) {
            target = NULL;
            strncpy(action, CLI_ACTION_INFO, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_GET_VOLUME_POLYBAR)) {
            target = NULL;
            strncpy(action, CLI_ACTION_GET_VOLUME_POLYBAR, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_GET_NAME_POLYBAR)) {
            target = NULL;
            strncpy(action, CLI_ACTION_GET_NAME_POLYBAR, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_GET_VOLUME)) {
            target = NULL;
            strncpy(action, CLI_ACTION_GET_VOLUME, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_SET_VOLUME)) {
            target = NULL;
            strncpy(action, CLI_ACTION_SET_VOLUME, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_INCREASE_VOLUME)) {
            target = NULL;
            strncpy(action, CLI_ACTION_INCREASE_VOLUME, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_DECREASE_VOLUME)) {
            target = NULL;
            strncpy(action, CLI_ACTION_DECREASE_VOLUME, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_TOGGLE_MUTE)) {
            target = NULL;
            strncpy(action, CLI_ACTION_TOGGLE_MUTE, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_CICLE_DEVICES)) {
            target = NULL;
            strncpy(action, CLI_ACTION_CICLE_DEVICES, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_SET_DEVICE)) {
            target = NULL;
            strncpy(action, CLI_ACTION_SET_DEVICE, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_VALUE_VOLUME)) {
            target = volume;
        } else if (!strcmp(argv[i], CLI_VALUE_DEVICE)) {
            target = device;
        } else {
            if (target != NULL) {
                strncpy(target, argv[i], CLI_PARAMETER_VALUE_MAX_LENGHT);
            }
            target = NULL;
        }
    }

    // query the pulseaudio state
    getPulseaudioState(audioDevices, &audioDevicesCount, &defaultDeviceIndex);
    
    // execute the desired action
    if (!strcmp(action, CLI_ACTION_HELP)) {
        help();
    } else if (!strcmp(action, CLI_ACTION_INFO)) {
        info(audioDevices, audioDevicesCount);
    } else if (!strcmp(action, CLI_ACTION_GET_VOLUME_POLYBAR)) {
        getVolumePolybar(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_GET_NAME_POLYBAR)) {
        getNamePolybar(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_GET_VOLUME)) {
        getVolume(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_SET_VOLUME)) {
        setVolume(audioDevices, audioDevicesCount, defaultDeviceIndex, volume);
    } else if (!strcmp(action, CLI_ACTION_INCREASE_VOLUME)) {
        increaseVolume(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_DECREASE_VOLUME)) {
        decreaseVolume(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_TOGGLE_MUTE)) {
        toggleMute(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_CICLE_DEVICES)) {
        cicleDevices(audioDevices, audioDevicesCount, defaultDeviceIndex);
    } else if (!strcmp(action, CLI_ACTION_SET_DEVICE)) {
        setDevice(audioDevices, audioDevicesCount, defaultDeviceIndex, device);
    } else {
        help();
    }

    return 0;
}

// executes pacmd list-sinks and redirects its output to some char array
int getPulseaudioState(AudioDevice* audioDevices, int* audioDevicesCount, int* defaultDeviceIndex) {
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    runCommand("pacmd list-sinks", output, COMMAND_OUTPUT_MAX_LENGTH);

    // for each line of the command output
    for (char* lineRest = NULL, * line = strtok_r(output, "\n", &lineRest); line != NULL; line = strtok_r(NULL, "\n", &lineRest)) {
        // we will need to split the line, this array will have pointers to where the text is
        char* values[STRING_SPLIT_SIZE];
        int valuesCount = 0;

        // a line startnig with '  * index:' gives the index of the default device
        if (!strncmp("  * index:", line, strlen("  * index:"))) {
            for (char* valueRest = NULL, * value = strtok_r(line, " ", &valueRest); value != NULL && valuesCount < STRING_SPLIT_SIZE; value = strtok_r(NULL, " ", &valueRest)) {
                values[valuesCount] = value;
                valuesCount++;
            }
            
            // create a new audio device and store its index as the default one
            audioDevices[*audioDevicesCount].deviceIndex = atoi(values[2]);
            audioDevices[*audioDevicesCount].deviceIsDefault = 1;
            *defaultDeviceIndex = *audioDevicesCount;
            (*audioDevicesCount)++;
        }

        // a line startnig with '  * index:' gives the index of some non-default device
        else if (!strncmp("    index:", line, strlen("    index:"))) {
            for (char* valueRest = NULL, * value = strtok_r(line, " ", &valueRest); value != NULL && valuesCount < STRING_SPLIT_SIZE; value = strtok_r(NULL, " ", &valueRest)) {
                values[valuesCount] = value;
                valuesCount++;
            }

            // create a new audio device
            audioDevices[*audioDevicesCount].deviceIndex = atoi(values[1]);
            audioDevices[*audioDevicesCount].deviceIsDefault = 0;
            (*audioDevicesCount)++;
        }

        // a line starting with '	volume:' gives the volume of the device being read
        else if (!strncmp("	volume:", line, strlen("	volume:"))) {
            // continue only if an audio device was already found
            if (*audioDevicesCount > 0) {
                for (char* valueRest = NULL, * value = strtok_r(line, " ", &valueRest); value != NULL && valuesCount < STRING_SPLIT_SIZE; value = strtok_r(NULL, " ", &valueRest)) {
                    if (valuesCount == 4 || valuesCount == 11) {
                        // remove percentage symbol
                        value = strtok_r(value, "%", &value);
                    }

                    values[valuesCount] = value;
                    valuesCount++;
                }

                // sets the volume of the current device to max of left and right volumes
                int leftVolume = atoi(values[4]);
                int rightVolume = atoi(values[11]);
                audioDevices[*audioDevicesCount - 1].deviceVolume = (leftVolume > rightVolume) ? leftVolume : rightVolume;
            }
        }

        // a line starting with '		device.product.name =' gives the name of the device being read
        else if (!strncmp("		device.product.name =", line, strlen("		device.product.name ="))) {
            // continue only if an audio device was already found
            if (*audioDevicesCount > 0) {
                for (char* valueRest = NULL, * value = strtok_r(line, "=", &valueRest); value != NULL && valuesCount < STRING_SPLIT_SIZE; value = strtok_r(NULL, "=", &valueRest)) {
                    if (valuesCount == 1) {
                        for (char* noQuotesRest = NULL, * noQuotes = strtok_r(value, "\"", &noQuotesRest); noQuotes != NULL; noQuotes = strtok_r(NULL, "\"", &noQuotesRest)) {
                            if (strcmp(noQuotes, " ")) {
                                value = noQuotes;
                                break;
                            }
                        }
                    }

                    values[valuesCount] = value;
                    valuesCount++;
                }

                // sets the name of the current device
                strncpy(audioDevices[*audioDevicesCount - 1].deviceName, values[1], DEVICE_NAME_MAX_LENGTH);
            }
        }

        // a line starting with '	muted:' gives the muted state of the device being read
        else if (!strncmp("	muted:", line, strlen("	muted:"))) {
            // continue only if an audio device was already found
            if (*audioDevicesCount > 0) {
                for (char* valueRest = NULL, * value = strtok_r(line, " ", &valueRest); value != NULL && valuesCount < STRING_SPLIT_SIZE; value = strtok_r(NULL, " ", &valueRest)) {
                    values[valuesCount] = value;
                    valuesCount++;
                }
            }

            // sets the muted state of the current device
            audioDevices[*audioDevicesCount - 1].deviceIsMuted = !strcmp(values[1], "yes");
        }

        // debug
        // for (int i = 0; i < valuesCount; i++) {
        //     printf("[%d] '%s' ", i, values[i]);
        //     printf("\n");
        // }
    }

    return 0;
}

// shows the help message
int help(void) {
    printf("audio - CLI tool to control pulseaudio via simplified inputs\n");
    printf("  audio %s: shows this help message\n", CLI_ACTION_HELP);
    printf("  audio %s: shows the complete pulseaudio state\n", CLI_ACTION_INFO);
    printf("  audio %s: returns the volume formatted to use on polybar (requires ./color)\n", CLI_ACTION_GET_VOLUME_POLYBAR);
    printf("  audio %s: returns the name of the default audio device formatted to use on polybar (requires ./color)\n", CLI_ACTION_GET_NAME_POLYBAR);
    printf("  audio %s: returns the volume\n", CLI_ACTION_GET_VOLUME);
    printf("  audio %s %s V: sets the volume to V (%d >= V >= 0)\n", CLI_ACTION_SET_VOLUME, CLI_VALUE_VOLUME, VOLUME_MAX);
    printf("  audio %s: increases the volume by %d until %d\n", CLI_ACTION_INCREASE_VOLUME, VOLUME_STEP, VOLUME_MAX);
    printf("  audio %s: decreases the volume by %d until 0\n", CLI_ACTION_DECREASE_VOLUME, VOLUME_STEP);
    printf("  audio %s: mutes/unmutes\n", CLI_ACTION_TOGGLE_MUTE);
    printf("  audio %s: set the next device as the default\n", CLI_ACTION_CICLE_DEVICES);
    printf("  audio %s %s D: sets the default device to D\n", CLI_ACTION_SET_DEVICE, CLI_VALUE_DEVICE);
    fflush(stdout);

    return 0;
}

// shows the complete pulseaudio state
int info(AudioDevice* audioDevices, int audioDevicesCount) {
    for (int i = 0; i < audioDevicesCount; i++) {
        printf("Audio device %2.2d: Name:%-64.64s Volume:%3.3d Muted:%1.1d Default:%1.1d\n", audioDevices[i].deviceIndex, audioDevices[i].deviceName, audioDevices[i].deviceVolume, audioDevices[i].deviceIsMuted, audioDevices[i].deviceIsDefault);
    }
    fflush(stdout);

    return 0;
}

// returns the volume of the default device formatted to show on polybar
int getVolumePolybar(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    char underlineColor[CLI_PARAMETER_VALUE_MAX_LENGHT] = "#555555";
    if (!audioDevices[defaultDeviceIndex].deviceIsMuted) {
        // runs the command and store its output
        char input[COMMAND_INPUT_MAX_LENGTH];
        char output[COMMAND_OUTPUT_MAX_LENGTH];
        sprintf(input, "color --rgb-shift --value %d --maximum 100", audioDevices[defaultDeviceIndex].deviceVolume);
        runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

        // removes the new line character from the end of the command output
        for (char* lineRest = NULL, * line = strtok_r(output, "\n", &lineRest); line != NULL; line = strtok_r(NULL, "\n", &lineRest)) {
            if (strcmp(line, " ")) {
                strncpy(underlineColor, line, CLI_PARAMETER_VALUE_MAX_LENGHT);
                break;
            }
        }
    }

    printf("%%{u%s}%d\n", underlineColor, audioDevices[defaultDeviceIndex].deviceVolume);
    fflush(stdout);

    return 0;
}

// returns the name of the default device formatted to show on polybar
int getNamePolybar(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    char underlineColor[CLI_PARAMETER_VALUE_MAX_LENGHT] = "#555555";
    if (!audioDevices[defaultDeviceIndex].deviceIsMuted) {
        // runs the command and store its output
        char input[COMMAND_INPUT_MAX_LENGTH];
        char output[COMMAND_OUTPUT_MAX_LENGTH];
        sprintf(input, "color --rgb-shift --value %d --maximum 100", audioDevices[defaultDeviceIndex].deviceVolume);
        runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

        // removes the new line character from the end of the command output
        for (char* lineRest = NULL, * line = strtok_r(output, "\n", &lineRest); line != NULL; line = strtok_r(NULL, "\n", &lineRest)) {
            if (strcmp(line, " ")) {
                strncpy(underlineColor, line, CLI_PARAMETER_VALUE_MAX_LENGHT);
                break;
            }
        }
    }

    printf("%%{u%s}%s\n", underlineColor, audioDevices[defaultDeviceIndex].deviceName);
    fflush(stdout);

    return 0;
}

// returns the volume of the default device
int getVolume(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    printf("%d\n", audioDevices[defaultDeviceIndex].deviceVolume);
    fflush(stdout);

    return 0;
}

// sets the volume of the default device
int setVolume(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex, char* volume) {
    // validates all received parameters
    if (!strcmp(volume, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_VOLUME);
        fflush(stdout);
        return 1;
    }

    // converts the text parameter to integer and enforces min and max values
    int integerVolume = atoi(volume);
    integerVolume = (integerVolume <= VOLUME_MAX) ? integerVolume : VOLUME_MAX;
    integerVolume = (integerVolume >= 0) ? integerVolume : 0;

    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pactl set-sink-volume %d %d%%", audioDevices[defaultDeviceIndex].deviceIndex, integerVolume);    
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}

int increaseVolume(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    // finds what the new volume will be enforcing min and max values
    int newVolume = audioDevices[defaultDeviceIndex].deviceVolume + VOLUME_STEP;
    newVolume = (newVolume <= VOLUME_MAX) ? newVolume : VOLUME_MAX;
    newVolume = (newVolume >= 0) ? newVolume : 0;

    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pactl set-sink-volume %d %d%%", audioDevices[defaultDeviceIndex].deviceIndex, newVolume);
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}

int decreaseVolume(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    // finds what the new volume will be enforcing min and max values
    int newVolume = audioDevices[defaultDeviceIndex].deviceVolume - VOLUME_STEP;
    newVolume = (newVolume <= VOLUME_MAX) ? newVolume : VOLUME_MAX;
    newVolume = (newVolume >= 0) ? newVolume : 0;

    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pactl set-sink-volume %d %d%%", audioDevices[defaultDeviceIndex].deviceIndex, newVolume);    
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}

int toggleMute(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pactl set-sink-mute %d %d%%", audioDevices[defaultDeviceIndex].deviceIndex, !(audioDevices[defaultDeviceIndex].deviceIsMuted));
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}

int cicleDevices(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex) {
    // finds the index of the next device on the array returning to the start if the end is reached
    int nextDefaultDeviceIndex = (defaultDeviceIndex + 1) % audioDevicesCount;

    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pactl set-default-sink %d", audioDevices[nextDefaultDeviceIndex].deviceIndex);
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}

// sets the new default device
int setDevice(AudioDevice* audioDevices, int audioDevicesCount, int defaultDeviceIndex, char* device) {
    // validates all received parameters
    if (!strcmp(device, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_DEVICE);
        fflush(stdout);
        return 1;
    }

    // converts the text parameter to integer and enforces min and max values
    int integerDevice = atoi(device);
    integerDevice = (integerDevice < audioDevicesCount) ? integerDevice : audioDevicesCount - 1;
    integerDevice = (integerDevice >= 0) ? integerDevice : 0;

    // runs the command and store its output
    char input[COMMAND_INPUT_MAX_LENGTH];
    char output[COMMAND_OUTPUT_MAX_LENGTH];
    sprintf(input, "pacmd set-default-sink %d", audioDevices[integerDevice].deviceIndex);    
    runCommand(input, output, COMMAND_OUTPUT_MAX_LENGTH);

    return 0;
}