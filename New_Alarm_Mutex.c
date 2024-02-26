#include <regex.h>
#include "errors.h"
#include <stdarg.h>

/**
 * Prints a debug message to the console.
 *
 * This will only work if the program is compiled with the debug flag.
 * To do that, use the -DDEBUG flag when compiling:
 *
 *   gcc <input_file> -DDEBUG
 *
 * When compiling for production, do not compile with the debug flag,
 * or else the console output will be unnecessarily cluttered.
 */
void debug_print(const char* message) {
#ifdef DEBUG
    printf("%s", message);
#endif
}

/**
 * Prints a formatted debug message to the console. The message can be
 * formatted as if you were calling printf.
 *
 * This will only work if the program is compiled with the debug flag.
 * To do that, use the -DDEBUG flag when compiling:
 *
 *   gcc <input_file> -DDEBUG
 *
 * When compiling for production, do not compile with the debug flag,
 * or else the console output will be unnecessarily cluttered.
 */
void debug_printf(char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
}

/**
 * The six possible types of commands that a user can enter.
 */
typedef enum command_type {
    Start_Alarm,
    Change_Alarm,
    Cancel_Alarm,
    Suspend_Alarm,
    Reactivate_Alarm,
    View_Alarms
} command_type;

/**
 * Data structure representing a command entered by a user. Includes
 * the type of the command, the alarm_id (if applicable), the time
 * (if applicable), and the message (if applicable).
 */
typedef struct command_t {
    command_type type;
    int          alarm_id;
    int          time;
    char         message[128];
} command_t;

/**
 * This method takes a string and checks if it matches any of the
 * command formats. If there is no match, NULL is returned. If there
 * is a match, it parses the string into a command and returns it.
 */
command_t* parse_command(char input[]) {
    regex_t regex; // Holds the regex that we are testing

    int reti;      // Holds the status of regex tests

    // This is the data type that holds information about parsing a
    // command. It contains the type of the command, the regular
    // expression for the command, the number of matches within the
    // command (that must be parsed out).
    typedef struct regex_parser {
        command_type type;
        const char *regex_string;
        int expected_matches;
    } regex_parser;

    regmatch_t matches[3];     // Holds the number of matches.
                               // (No command has more than 3
                               // matches).

    command_t *command;        // Holds the pointer to the command
                               // that will be returned.
                               // (This will be malloced, so it must
                               // be freed later).

    int number_of_regexes = 6; // Six regexes for six commands


    /*
     * These are the regexes for the commands that we must look for.
     */
    regex_parser regexes[] = {
        {
            Start_Alarm,
            "Start_Alarm\\(([0-9]+)\\):[[:space:]]([0-9])+[[:space:]](.*)",
            3
        },
        {
            Change_Alarm,
            "Change_Alarm\\(([0-9]+)\\):[[:space:]]([0-9])+[[:space:]](.*)",
            3
        },
        {
            Cancel_Alarm,
            "Cancel_Alarm\\(([0-9]+)\\)",
            1
        },
        {
            Suspend_Alarm,
            "Suspend_Alarm\\(([0-9]+)\\)",
            1
        },
        {
            Reactivate_Alarm,
            "Reactivate_Alarm\\(([0-9]+)\\)",
            1
        },
        {
            View_Alarms,
            "View_Alarms",
            0
        }
    };

    /*
     * Loop through the regexes and test each one.
     */
    for (int i = 0; i < number_of_regexes; i++) {
        /*
         * Compile the regex
         */
        reti = regcomp(
            &regex,
            regexes[i].regex_string,
            REG_EXTENDED
        );

        // Make sure compilation succeeded
        if (reti != 0) {
            fprintf(
                stderr,
                "Regex %d \"%s\" did not compile\n",
                i,
                regexes[i].regex_string
            );
            exit(1);
        }

        /*
         * Test input string against regex
         */
        reti = regexec(
            &regex,
            input,
            regexes[i].expected_matches,
            matches,
            0
        );

        if (reti == REG_NOMATCH) {
            /*
             * If no match, then free the regex and continue on with
             * the loop.
             */
            regfree(&regex);
            continue;
        } else if (reti != 0) {
            /*
             * If regex test returned nonzero value, then it was an
             * error. In this case, free the regex and stop the
             * program.
             */
            fprintf(
                stderr,
                "Regex %d \"%s\" did not work with input \"%s\"\n",
                i,
                regexes[i].regex_string,
                input
            );
            regfree(&regex);
            exit(1);
        } else {
            /*
             * In this case (regex returned 0), we know the input has
             * matched a command and we can now parse it.
             */

            /* // Get the alarm_id from the input (if it exists) */
            /* alarm_id = (regexes[i].expected_matches > 0) */
            /*     ? atoi(&input[matches[0].rm_so]) */
            /*     : 0; */
            /* // Get the time from the input (if it exists) */
            /* time = (regexes[i].expected_matches > 1) */
            /*     ? atoi(&input[matches[1].rm_so]) */
            /*     : 0; */

            /* // Copy the message from the input (if it exists) */
            /* if (regexes[i].expected_matches > 2) { */
            /*     strcpy(message, &input[matches[2].rm_so]); */
            /* } */

            for (int j=0; j < regexes[i].expected_matches; j++) {
                debug_printf(
                    "Match: %.*s\n",
                    matches[j].rm_eo-matches[j].rm_so,
                    &input[matches[j].rm_so]
                );
            }

            // Free the regex
            regfree(&regex);

            /*
             * Allocate command (IT MUST BE FREED LATER)
             */
            command = malloc(sizeof(command_t));
            if (command == NULL) {
                errno_abort("Malloc failed");
            }

            /*
             * Fill command with data
             */
            command->type = regexes[i].type;
            // Get the alarm_id from the input (if it exists)
            if (regexes[i].expected_matches > 0) {
                command->alarm_id = atoi(&input[matches[0].rm_so]);
            } else {
                command->alarm_id = 0;
            }
            // Get the time from the input (if it exists)
            if (regexes[i].expected_matches > 1) {
                command->time = atoi(&input[matches[1].rm_so]);
            } else {
                command->time = 0;
            }
            // Copy the message from the input (if it exists)
            if (regexes[i].expected_matches > 2) {
                strcpy(command->message, &input[matches[2].rm_so]);
            }

            return command;
        }
        return NULL;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    char input[128];
    char *return_string;
    command_t *command;

    while (1) {
        printf("Alarm > ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            continue;
        }
        // Replace newline with null terminating character
        input[strcspn(input, "\n")] = 0;
        command = parse_command(input);
        if (command == NULL) {
            printf("Bad command\n");
            continue;
        } else {
            debug_printf(
                "{type: %s, id: %d, time: %d, message: %s}\n",
                command->type,
                command->alarm_id,
                command->time,
                command->message
            );

            free(command);
        }
    }

    return 0;
}

