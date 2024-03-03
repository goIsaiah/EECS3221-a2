#include <pthread.h>
#include <regex.h>
#include "errors.h"
#include "debug.h"
#include <sys/types.h>
#include <sys/syscall.h>

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

/**
 * Inserts an alarm into the list of alarms.
 *
 * The alarm list mutex MUST BE LOCKED by the caller of this method.
 *
 * If an item in the list already exist with the given alarm's
 * alarm_id, this method returns NULL (and prints an error message to
 * the console). Otherwise, the alarm is added to the list and the
 * alarm is returned.
 */
alarm_t* insert_alarm_into_list(alarm_t *alarm) {
    alarm_t *alarm_node = header.next;
    alarm_t *next_alarm_node;

    // If the list is initially empty, make the given alarm the head
    // of the list.
    if (header.next == NULL) {
        header.next = alarm;
        return alarm;
    }

    alarm_node = &header;
    next_alarm_node = header.next;

    // Find where to insert it by compating alarm_id. The
    // list should always be sorted by alarm_id.
    while (alarm_node != NULL) {
        if (next_alarm_node == NULL) {
            if (alarm_node->alarm_id == alarm->alarm_id) {
                /*
                 * Invalid because two alarms cannot have the
                 * same alaarm_id. In this case, unlock the
                 * mutex and try again.
                 */
                printf("Alarm with same ID exists\n");
                return NULL;
            } else {
                // Insert to end of list
                alarm_node->next = alarm;
                alarm->next = NULL;
                return alarm;
            }
        }

        if (alarm->alarm_id == alarm_node->alarm_id) {
            /*
             * Invalid because two alarms cannot have the
             * same alaarm_id. In this case, unlock the
             * mutex and try again.
             */
            printf("Alarm with same ID exists\n");
            return NULL;
        } else if (alarm->alarm_id > alarm_node->alarm_id
                   && alarm->alarm_id < next_alarm_node->alarm_id) {
            alarm->next = next_alarm_node;
            alarm_node->next = alarm;
            return alarm;
        } else {
            alarm_node = next_alarm_node;
            next_alarm_node = next_alarm_node->next;
        }
    }
}

alarm_t* remove_alarm_from_list(int id) {
    alarm_t *alarm_node = header.next;
    alarm_t *alarm_prev = &header;

    while (alarm_node != NULL) {
        if (alarm_node->alarm_id == id) {
            alarm_prev->next = alarm_node->next;
            break;
        }
        alarm_node = alarm_node->next;
        alarm_prev = alarm_prev->next;
    }
    return alarm_node;
}

int doesAlarmExist(int id) {
    alarm_t *alarm_node = header.next;
    
    while (alarm_node != NULL) {
        if (alarm_node->alarm_id == id) {
            return 1;
        }
        else {
            alarm_node = alarm_node->next;
        }
    }
    return 0;
}

typedef struct thread_t {
    int thread_id;
} thread_t;

typedef struct event_t {
    int type;
    int alarmId;
    alarm_t *alarm;
} event_t;

event_t *event = NULL;

pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *arg) {
    int id = ((thread_t*) arg)->thread_id;
    alarm_t *alarm1 = NULL;
    alarm_t *alarm2 = NULL;
    int status;
    struct timespec t;

    /*
     * Lock the mutex so that this thread can access the alarm list.
     */
    pthread_mutex_lock(&alarm_list_mutex);

    while (1) {
        /*
         * Set timeout to 5 seconds in the future.
         */
        t.tv_sec = time(NULL) + 5;
        t.tv_nsec = 0;

        /*
         * Wait for condition variable to be broadcast. When we get a
         * broadcast, this thread will wake up and will have the mutex
         * locked.
         *
         * Since this is a timed wait, we may also be woken up by the
         * time expiring. In this case, the status returned will be
         * ETIMEDOUT.
         */
        status = pthread_cond_timedwait(
            &alarm_list_cond,
            &alarm_list_mutex,
            &t
        );

        /*
         * In this case, the 5 seconds timed out, so we must print the
         * alarm.
         */
        if (status == ETIMEDOUT) {
            /*
             * If alarm 1 is defined in this thread, print it.
             */
            if (alarm1 != NULL) {
                printf(
                    "Alarm (%d) Printed by Alarm Display Thread %d at %ld: %d %s\n",
                    alarm1->alarm_id,
                    id,
                    time(NULL),
                    alarm1->time,
                    alarm1->message
                );
            }

            /*
             * If alarm 2 is defined in this thread, print it.
             */
            if (alarm2 != NULL) {
                printf(
                    "Alarm (%d) Printed by Alarm Display Thread %d at %ld: %d %s\n",
                    alarm2->alarm_id,
                    id,
                    time(NULL),
                    alarm2->time,
                    alarm2->message
                );
            }

            /*
             * Since the thread was woken up by a timeout, we continue
             * here so that we don't execute any code below. The code
             * below is for event handling, and there was no event.
             */
            continue;
        }

        /*
         * If we reach this part of the code, then the thread was
         * woken up by an event and not a timeout. In this case, lock
         * the event mutex so we can try to handle the event.
         */
        pthread_mutex_lock(&event_mutex);

        /*
         * If event is null, then it was either a false alarm, or the
         * event was already handled by another thread, so we can
         * ignore it.
         */
        if (event == NULL) {
            pthread_mutex_unlock(&event_mutex);
            continue;
        }

        /*
         * Event type of 1 means a new alarm was created.
         */
        if (event->type == 1) {
            /*
             * We need to check if this thread's alarms are NULL to
             * see if we have capacity for the new alarm. If we take
             * the alarm, then we must free the event and set it to
             * NULL to show other threads that this event was already
             * handled.
             */
            if (alarm1 == NULL) {
                /*
                 * If alarm1 is NULL, then take the alarm.
                 */
                alarm1 = event->alarm;
                DEBUG_PRINTF("Thread took alarm %d\n", alarm1->alarm_id);
                free(event);
                event = NULL;
            } else if (alarm2 == NULL) {
                /*
                 * If alarm2 is NULL, we can still take the alarm.
                 */
                alarm2 = event->alarm;
                DEBUG_PRINTF("Thread took alarm %d\n", alarm2->alarm_id);
                free(event);
                event = NULL;
            } else {
                /*
                 * If both alarms are not NULL, we cannot take the
                 * alarm.
                 */
                DEBUG_PRINTF(
                    "Thread at capacity, did not take alarm %d\n",
                    event->alarm->alarm_id
                );
            }

            /*
             * Unlock event mutex so that the main thread can send
             * another event.
             */
            pthread_mutex_unlock(&event_mutex);
        }

        /*
        * Event of type 3 means that an alarm is being cancelled
        */
        else if (event->type == 3) {
            if (alarm1->alarm_id == event->alarmId) {
                printf(
                    "Alarm (%d) Canceled at %ld: %s\n",
                    alarm1->alarm_id,
                    time(NULL),
                    alarm1->message
                );
                alarm1 = NULL;
            }
            else if (alarm2->alarm_id == event->alarmId) {
                printf(
                    "Alarm (%d) Canceled at %ld: %s\n",
                    alarm2->alarm_id,
                    time(NULL),
                    alarm2->message
                );
                alarm2 = NULL;
            }

            free(event);
            event = NULL;
            pthread_mutex_unlock(&event_mutex);
        }

        DEBUG_PRINT_ALARM_LIST(header.next);

    }

    /*
     * Unlock alarm list mutex.
     */
    pthread_mutex_unlock(&alarm_list_mutex);
}

int main(int argc, char *argv[]) {
    char input[128];
    char *return_string;
    command_t *command;
    alarm_t *alarm;
    pthread_t thread;
    int number_of_threads = 1;

    DEBUG_PRINT_START_MESSAGE();

    thread_t thread1 = { 1 };
    pthread_create(&thread, NULL, client_thread, &thread1);

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

            if (command->type == Start_Alarm) {
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
                if (insert_alarm_into_list(alarm) == NULL) {
                    /*
                     * If inserting into alarm returns NULL, then the
                     * alarm was not inserted into this list. In this
                     * case, unlock the alarm list mutex and free the
                     * alarm's memory.
                     */
                    free(alarm);
                    pthread_mutex_unlock(&alarm_list_mutex);
                    continue;
                }

                pthread_mutex_lock(&event_mutex);

                event = malloc(sizeof(event_t));
                event->type = 1;
                event->alarm = alarm;

                pthread_mutex_unlock(&event_mutex);
            }

            else if (command->type == Change_Alarm) {

            }

            else if (command->type == Cancel_Alarm) {
                int cancelId = command->alarm_id; // Get the ID that will be cancellled
                /*
                 * Lock the mutex for the alarm list
                 */
                pthread_mutex_lock(&alarm_list_mutex);

                int AlarmExists = doesAlarmExist(cancelId);
                
                if (AlarmExists == 0) {
                    printf("Not a valid ID.\n");
                }
                else {
                    remove_alarm_from_list(cancelId);

                    pthread_mutex_lock(&event_mutex);

                    event = malloc(sizeof(event_t));
                    event->type = 3;
                    event->alarmId = cancelId;

                    pthread_mutex_unlock(&event_mutex);
                    printf("Cancelled alarm %d\n", cancelId);
                }
            }

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

