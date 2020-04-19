#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../common.h"

#define CLI_ACTION_HELP "--help"
#define CLI_ACTION_RGB_SHIFT "--rgb-shift"
#define CLI_ACTION_ALPHA_TRANSPARENCY "--alpha-transparency"
#define CLI_VALUE_VALUE "--value"
#define CLI_VALUE_MAXIMUM "--maximum"

// function prototypes
int help(void);
int rgbShift(char*, char*);
int alphaTransparency(char*, char*);

// main
int main(int argc, char** argv) {
    char* target = NULL;
    char action[CLI_PARAMETER_NAME_MAX_LENGHT] = CLI_ACTION_HELP;
    char value[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_NUMERICAL_DEFAULT;
    char maximum[CLI_PARAMETER_VALUE_MAX_LENGHT] = CLI_NUMERICAL_DEFAULT;

    // get cli arguments to the right places
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], CLI_ACTION_HELP)) {
            target = NULL;
            strncpy(action, CLI_ACTION_HELP, CLI_PARAMETER_NAME_MAX_LENGHT);
            break;
        } else if (!strcmp(argv[i], CLI_ACTION_RGB_SHIFT)) {
            target = NULL;
            strncpy(action, CLI_ACTION_RGB_SHIFT, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_ACTION_ALPHA_TRANSPARENCY)) {
            target = NULL;
            strncpy(action, CLI_ACTION_ALPHA_TRANSPARENCY, CLI_PARAMETER_NAME_MAX_LENGHT);
        } else if (!strcmp(argv[i], CLI_VALUE_VALUE)) {
            target = value;
        } else if (!strcmp(argv[i], CLI_VALUE_MAXIMUM)) {
            target = maximum;
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
    } else if (!strcmp(action, CLI_ACTION_RGB_SHIFT)) {
        rgbShift(value, maximum);
    } else if (!strcmp(action, CLI_ACTION_ALPHA_TRANSPARENCY)) {
        alphaTransparency(value, maximum);
    } else {
        help();
    }

    return 0;
}

// shows the help message
int help(void) {
    printf("color - CLI tool to get colors from values\n");
    printf("  color %s: shows this help message\n", CLI_ACTION_HELP);
    printf("  color %s %s V %s M: return a hex color red shifted proportionally to V relative to M\n", CLI_ACTION_RGB_SHIFT, CLI_VALUE_VALUE, CLI_VALUE_MAXIMUM);
    printf("  color %s %s V %s M: return an alpha hex channel proportional to V relative to M\n", CLI_ACTION_ALPHA_TRANSPARENCY, CLI_VALUE_VALUE, CLI_VALUE_MAXIMUM);
    fflush(stdout);

    return 0;
}

// retuns a hex color shifted from blue to red proportional to value relative to maximum
int rgbShift(char* value, char* maximum) {
    // validates all received parameters
    if (!strcmp(value, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }
    if (!strcmp(maximum, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_MAXIMUM);
        fflush(stdout);
        return 1;
    }

    float floatValue = (float) atoi(value);
    float floatMaximum = (float) atoi(maximum);

    // prevent invalid values
    if (floatValue < 0) {
        printf("%s must be greater than or equal to zero!\n", CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }
    if (floatMaximum <= 0) {
        printf("%s must be greater than zero!\n", CLI_VALUE_MAXIMUM);
        fflush(stdout);
        return 1;
    }
    if (floatValue > floatMaximum) {
        printf("%s must be greater than or equal to %s!\n", CLI_VALUE_MAXIMUM, CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }

    int redComponent = (int) 255.0 * ((cos(M_PI * ((floatValue / floatMaximum) - 1.0)) + 1.0) / 2.0);
    int greenComponent = (int) 255.0 * ((cos(2.0 * M_PI * ((floatValue / floatMaximum) - 0.5)) + 1.0) / 2.0);
    int blueComponent = (int) 255.0 * ((cos(M_PI * (floatValue / floatMaximum)) + 1.0) / 2.0);

    printf("#%02x%02x%02x\n", redComponent, greenComponent, blueComponent);
    fflush(stdout);

    return 0;
}

// return an alpha hex channel proportional to value relative to maximum
int alphaTransparency(char* value, char* maximum) {
    // validates all received parameters
    if (!strcmp(value, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }
    if (!strcmp(maximum, CLI_NUMERICAL_DEFAULT)) {
        printf("Missing %s parameter!\n", CLI_VALUE_MAXIMUM);
        fflush(stdout);
        return 1;
    }

    float floatValue = (float) atoi(value);
    float floatMaximum = (float) atoi(maximum);

    // prevent invalid values
    if (floatValue < 0) {
        printf("%s must be greater than or equal to zero!\n", CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }
    if (floatMaximum <= 0) {
        printf("%s must be greater than zero!\n", CLI_VALUE_MAXIMUM);
        fflush(stdout);
        return 1;
    }
    if (floatValue > floatMaximum) {
        printf("%s must be greater than or equal to %s!\n", CLI_VALUE_MAXIMUM, CLI_VALUE_VALUE);
        fflush(stdout);
        return 1;
    }

    int alphaComponent = (int) 255.0 * (floatValue / floatMaximum);

    printf("#%02x\n", alphaComponent);
    fflush(stdout);
    
    return 0;
}