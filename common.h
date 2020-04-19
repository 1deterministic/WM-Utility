#include <unistd.h>
#include <string.h>

#define CLI_NUMERICAL_DEFAULT "null"
#define CLI_TEXT_DEFAULT "null"
#define CLI_ACTION_MAX_LENGTH 32
#define CLI_NUMERICAL_VALUE_MAX_LENGTH 8
#define CLI_TEXT_VALUE_MAX_LENGTH 256
#define COMMAND_INPUT_MAX_LENGTH 1024
#define COMMAND_OUTPUT_MAX_LENGTH 262144 // up to 256Kb
#define COMMAND_OUTPUT_MAX_LINE_LENGTH 128
#define FILE_OUTPUT_MAX_LINE_LENGTH 1024

#define CLI_PARAMETER_NAME_MAX_LENGHT 32
#define CLI_PARAMETER_VALUE_MAX_LENGHT 256

// runs an arbitrary command and stores its text output
int runCommand(char* command, char* output) {
    FILE* file = popen(command, "r");
    if (file == NULL) {
        return 1;
    }

    char buffer[COMMAND_OUTPUT_MAX_LINE_LENGTH];
    while(fgets(buffer, sizeof(buffer), file) != NULL) {
        strcat(output, buffer);
    }

    pclose(file);
    return 0;
}

// checks if a file exists
int fileExists(char* path) {
    return access(path, F_OK) != -1;
}

// reads all text from a file
int readTextFile(char* path, char* output) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    char buffer[FILE_OUTPUT_MAX_LINE_LENGTH];
    while(fgets(buffer, sizeof(buffer), file) != NULL) {
        strcat(output, buffer);
    }

    fclose(file);
    return 0;
}

// writes a string to a file
int writeTextFile(char* path, char* input, int overwrite) {
    FILE* file = fopen(path, (overwrite) ? "w" : "a");
    if (file == NULL) {
        return 1;
    }

    int status = fputs(input, file);
    fclose(file);

    return status == EOF;
}