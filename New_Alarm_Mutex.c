#include <pthread.h>
#include <regex.h>
#include "errors.h"
#include "debug.h"

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

alarm_t header = { 0, 0, "", NULL };

pthread_mutex_t alarm_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_list_cond = PTHREAD_COND_INITIALIZER;

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
                strncpy(
                    command->message,
                    input + matches[3].rm_so,
                    matches[3].rm_eo - matches[3].rm_so
                );
                command->message[matches[3].rm_eo - matches[3].rm_so] = 0;
            }

            return command;
        }
    }

    return NULL;
}

void *client_thread(void *arg) {
    /*
     * Lock the mutex so that this thread can access the alarm
     * list.
     */
    pthread_mutex_lock(&alarm_list_mutex);

    while (1) {
        /*
         * Wait for condition variable to be broadcast. When we get a
         * broadcast, this thread will wake up and will have the mutex
         * locked.
         */
        pthread_cond_wait(&alarm_list_cond, &alarm_list_mutex);

        /* alarm_t *alarm = header.next; */
        /* while (alarm != NULL) { */
        /*     DEBUG_PRINT_ALARM(alarm); */
        /*     alarm = alarm->next; */
        /* } */
        DEBUG_PRINT_ALARM_LIST(header.next);

    }
    pthread_mutex_unlock(&alarm_list_mutex);
}

int main(int argc, char *argv[]) {
    char input[128];
    char *return_string;
    command_t *command;
    alarm_t *alarm;
    pthread_t thread;

    DEBUG_PRINT_START_MESSAGE();

    pthread_create(&thread, NULL, client_thread, NULL);

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
            DEBUG_PRINT_COMMAND(command);

            /*
             * Allocate space for alarm.
             */
            alarm = malloc(sizeof(alarm_t));
            if (alarm == NULL) {
                errno_abort("Malloc failed");
            }

            /*
             * Fill in data for alarm.
             */
            alarm->alarm_id = command->alarm_id;
            alarm->time = command->time;
            strcpy(alarm->message, command->message);

            /*
             * Lock the mutex for the alarm list, so that no other
             * threads can access the list until we are finished
             * updating it.
             */
            pthread_mutex_lock(&alarm_list_mutex);

            /*
             * Insert alarm into the list.
             */
            if (header.next == NULL) {
                header.next = alarm;
            } else {
                alarm_t *alarm_node = header.next;
                // Find where to insert it by compating alarm_id. The
                // list should always be sorted by alarm_id.
                while (alarm_node != NULL) {
                    if (alarm->alarm_id > alarm_node->alarm_id) {
                        alarm->next = alarm_node->next;
                        alarm_node->next = alarm;
                        break;
                    }
                    alarm_node = alarm_node->next;
                }
            }

            DEBUG_PRINT_COMMAND(command);

            /*
             * We are done updating the list, so notify the other
             * threads by broadcasting, then unlock the mutex so that
             * the other threads can lock it.
             */
            pthread_cond_broadcast(&alarm_list_cond);
            pthread_mutex_unlock(&alarm_list_mutex);

            free(command);
        }
    }

    return 0;
}

