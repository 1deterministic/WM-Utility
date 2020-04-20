#include <unistd.h>
#include <string.h>

#define CLI_NUMERICAL_DEFAULT "null"
#define CLI_TEXT_DEFAULT "null"
#define CLI_PARAMETER_NAME_MAX_LENGHT 32
#define CLI_PARAMETER_VALUE_MAX_LENGHT 256

#define COMMAND_INPUT_MAX_LENGTH 1024
#define COMMAND_OUTPUT_MAX_LENGTH 8192
#define COMMAND_OUTPUT_MAX_LINE_LENGTH 128
#define FILE_OUTPUT_MAX_LENGTH 262144
#define FILE_OUTPUT_MAX_LINE_LENGTH 1024

// runs an arbitrary command and stores its text output
int runCommand(char* command, char* output, int sizeLimit) {
    FILE* file = popen(command, "r");
    if (file == NULL) {
        return 1;
    }

    output[0] = '\0';
    char buffer[COMMAND_OUTPUT_MAX_LINE_LENGTH];
    while(fgets(buffer, sizeof(buffer), file) != NULL) {
        strncat(output, buffer, (sizeLimit - COMMAND_OUTPUT_MAX_LINE_LENGTH > 0) ? COMMAND_OUTPUT_MAX_LINE_LENGTH : sizeLimit - COMMAND_OUTPUT_MAX_LINE_LENGTH);
    }

    pclose(file);
    return 0;
}

// checks if a file exists
int fileExists(char* path) {
    return access(path, F_OK) != -1;
}

// reads all text from a file
int readTextFile(char* path, char* output, int sizeLimit) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    output[0] = '\0';
    char buffer[FILE_OUTPUT_MAX_LINE_LENGTH];
    while(fgets(buffer, sizeof(buffer), file) != NULL && sizeLimit > 0) {
        strncat(output, buffer, (sizeLimit - FILE_OUTPUT_MAX_LINE_LENGTH > 0) ? FILE_OUTPUT_MAX_LINE_LENGTH : sizeLimit - FILE_OUTPUT_MAX_LINE_LENGTH);
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