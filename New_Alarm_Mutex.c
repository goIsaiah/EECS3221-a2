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
 * This is the data type that holds information about parsing a
 * command. It contains the type of the command, the regular
 * expression for the command, the number of matches within the
 * command (that must be parsed out).
 */
typedef struct regex_parser {
    command_type type;
    const char *regex_string;
    int expected_matches;
} regex_parser;

/**
 * These are the regexes for the commands that we must look for.
 */
regex_parser regexes[] = {
    {
        Start_Alarm,
        "Start_Alarm\\(([0-9]+)\\):[[:space:]]([0-9]+)[[:space:]](.*)",
        4
    },
    {
        Change_Alarm,
        "Change_Alarm\\(([0-9]+)\\):[[:space:]]([0-9]+)[[:space:]](.*)",
        4
    },
    {
        Cancel_Alarm,
        "Cancel_Alarm\\(([0-9]+)\\)",
        2
    },
    {
        Suspend_Alarm,
        "Suspend_Alarm\\(([0-9]+)\\)",
        2
    },
    {
        Reactivate_Alarm,
        "Reactivate_Alarm\\(([0-9]+)\\)",
        2
    },
    {
        View_Alarms,
        "View_Alarms",
        1
    }
};

/**
 * This method takes a string and checks if it matches any of the
 * command formats. If there is no match, NULL is returned. If there
 * is a match, it parses the string into a command and returns it.
 */
command_t* parse_command(char input[]) {
    regex_t regex;             // Holds the regex that we are testing

    int re_status;             // Holds the status of regex tests

    regmatch_t matches[4];     // Holds the number of matches.
                               // (No command has more than 3
                               // matches, which we add 1 to because
                               // the string itself counts as a
                               // match).

    command_t *command;        // Holds the pointer to the command
                               // that will be returned.
                               // (This will be malloced, so it must
                               // be freed later).

    int number_of_regexes = 6; // Six regexes for six commands

    char alarm_id_buffer[64];  // Buffer used to hold alarm_id as a
                               // string when it is being converted
                               // to an int.

    char time_buffer[64];      // Buffer used to  hold time as a
                               // string when it is being converted
                               // to an int.

    /*
     * Loop through the regexes and test each one.
     */
    for (int i = 0; i < number_of_regexes; i++) {
        /*
         * Compile the regex
         */
        re_status = regcomp(
            &regex,
            regexes[i].regex_string,
            REG_EXTENDED
        );

        // Make sure regex compilation succeeded
        if (re_status != 0) {
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
        re_status = regexec(
            &regex,
            input,
            regexes[i].expected_matches,
            matches,
            0
        );

        if (re_status == REG_NOMATCH) {
            /*
             * If no match, then free the regex and continue on with
             * the loop.
             */
            regfree(&regex);
            continue;
        } else if (re_status != 0) {
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
            if (regexes[i].expected_matches > 1) {
                strncpy(
                    alarm_id_buffer,
                    input + matches[1].rm_so,
                    matches[1].rm_eo - matches[1].rm_so
                );
                command->alarm_id = atoi(alarm_id_buffer);
            } else {
                command->alarm_id = 0;
            }
            // Get the time from the input (if it exists)
            if (regexes[i].expected_matches > 2) {
                strncpy(
                    time_buffer,
                    input + matches[2].rm_so,
                    matches[2].rm_eo - matches[2].rm_so
                );
                command->time = atoi(time_buffer);
            } else {
                command->time = 0;
            }
            // Copy the message from the input (if it exists)
            if (regexes[i].expected_matches > 3) {
                printf("\n-%s-\n", command->message);
                strncpy(
                    command->message,
                    input + matches[3].rm_so,
                    matches[3].rm_eo - matches[3].rm_so + 1
                );
                printf("\n-%s-\n", command->message);
            }

            return command;
        }
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
            printf(
                "{type: %d, id: %d, time: %d, message: %s}\n",
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

