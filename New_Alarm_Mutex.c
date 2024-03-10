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
    {Start_Alarm,
     "Start_Alarm\\(([0-9]+)\\):[[:space:]]([0-9]+)[[:space:]](.*)",
     4},
    {Change_Alarm,
     "Change_Alarm\\(([0-9]+)\\):[[:space:]]([0-9]+)[[:space:]](.*)",
     4},
    {Cancel_Alarm,
     "Cancel_Alarm\\(([0-9]+)\\)",
     2},
    {Suspend_Alarm,
     "Suspend_Alarm\\(([0-9]+)\\)",
     2},
    {Reactivate_Alarm,
     "Reactivate_Alarm\\(([0-9]+)\\)",
     2},
    {View_Alarms,
     "View_Alarms",
     1}
};

/**
 * Header of the list of alarms.
 */
alarm_t alarm_header = {0, 0, "", NULL, false, 0, 0};

/**
 * Mutex for the alarm list. Any thread reading or modifying the alarm list must
 * have this mutex locked.
 */
pthread_mutex_t alarm_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Condition variable for the alarm list. This allows threads to wait for
 * updates to the alarm list.
 */
pthread_cond_t alarm_list_cond = PTHREAD_COND_INITIALIZER;

/**
 * Header of the list of threads.
 */
thread_t thread_header = {0,0,0,NULL, 0};

/**
 * Mutex for the thread list. Any thread reading or modifying the thread list
 * must have this mutex locked.
 */
pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * The current event being handled. If the event is NULL, then there is no
 * event being handled. Note that there can only be one event being handled at
 * any given time.
 */
event_t *event = NULL;

/**
 * Mutex for the event. Any thread reading or modifying the event must have
 * this mutex locked.
 */
pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * This method takes a string and checks if it matches any of the
 * command formats. If there is no match, NULL is returned. If there
 * is a match, it parses the string into a command and returns it.
 */
command_t *parse_command(char input[])
{
    regex_t regex; // Holds the regex that we are testing

    int re_status; // Holds the status of regex tests

    regmatch_t matches[4]; // Holds the number of matches. (No command has more
                           // than 3 matches, which we add 1 to because the
                           // string itself counts as a match).

    command_t *command; // Holds the pointer to the command that will be
                        // returned. (This will be malloced, so it must be
                        // freed later).

    int number_of_regexes = 6; // Six regexes for six commands

    char alarm_id_buffer[64]; // Buffer used to hold alarm_id as a string when
                              // it is being converted to an int.

    char time_buffer[64]; // Buffer used to  hold time as a string when it is
                          // being converted to an int.

    /*
     * Loop through the regexes and test each one.
     */
    for (int i = 0; i < number_of_regexes; i++)
    {
        /*
         * Compile the regex
         */
        re_status = regcomp(
            &regex,
            regexes[i].regex_string,
            REG_EXTENDED);

        // Make sure regex compilation succeeded
        if (re_status != 0)
        {
            fprintf(
                stderr,
                "Regex %d \"%s\" did not compile\n",
                i,
                regexes[i].regex_string);
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
            0);

        if (re_status == REG_NOMATCH)
        {
            /*
             * If no match, then free the regex and continue on with
             * the loop.
             */
            regfree(&regex);
            continue;
        }
        else if (re_status != 0)
        {
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
                input);
            regfree(&regex);
            exit(1);
        }
        else
        {
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
            if (command == NULL)
            {
                errno_abort("Malloc failed");
            }

            /*
             * Fill command with data
             */
            command->type = regexes[i].type;
            // Get the alarm_id from the input (if it exists)
            if (regexes[i].expected_matches > 1)
            {
                strncpy(
                    alarm_id_buffer,
                    input + matches[1].rm_so,
                    matches[1].rm_eo - matches[1].rm_so);
                command->alarm_id = atoi(alarm_id_buffer);
            }
            else
            {
                command->alarm_id = 0;
            }
            // Get the time from the input (if it exists)
            if (regexes[i].expected_matches > 2)
            {
                strncpy(
                    time_buffer,
                    input + matches[2].rm_so,
                    matches[2].rm_eo - matches[2].rm_so);
                command->time = atoi(time_buffer);
            }
            else
            {
                command->time = 0;
            }
            // Copy the message from the input (if it exists)
            if (regexes[i].expected_matches > 3)
            {
                strncpy(
                    command->message,
                    input + matches[3].rm_so,
                    matches[3].rm_eo - matches[3].rm_so);
                command->message[matches[3].rm_eo - matches[3].rm_so] = 0;
            }

            return command;
        }
    }

    return NULL;
}

/**
 * Finds an alarm in the list using a specified ID
 *
 * Alarm list has to be locked by the caller of this method
 *
 * Goes through the linked list, when specified ID is found, pointer
 * to that alarm will be returned.
 *
 * If the specified ID is not found, return NULL.
 */
alarm_t* find_alarm_by_id(int id) 
{
    alarm_t *alarm_node = alarm_header.next;
    
    //Loop through the list
    while(alarm_node != NULL)
    {
        //if ID is found, return pointer to it
        if(alarm_node-> alarm_id == id)
        {
            return alarm_node;
        }
        //if ID is NOT found, go to next node
        else
        {
            alarm_node = alarm_node->next;
        }
    }
    //If the entire list was searched and specified ID was not
    //found, return NULL.
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
alarm_t *insert_alarm_into_list(alarm_t *alarm)
{
    alarm_t *alarm_node = &alarm_header;
    alarm_t *next_alarm_node = alarm_header.next;

    // Find where to insert it by comparing alarm_id. The
    // list should always be sorted by alarm_id.
    while (next_alarm_node != NULL)
    {
        if (alarm->alarm_id == next_alarm_node->alarm_id)
        {
            /*
             * Invalid because two alarms cannot have the
             * same alaarm_id. In this case, unlock the
             * mutex and try again.
             */
            printf("Alarm with same ID exists\n");
            return NULL;
        }
        else if (alarm->alarm_id < next_alarm_node->alarm_id)
        {
            // Insert before next_alarm_node
            alarm_node->next = alarm;
            alarm->next = next_alarm_node;

            return alarm;
        }
        else
        {
            alarm_node = next_alarm_node;
            next_alarm_node = next_alarm_node->next;
        }
    }

    // Insert at the end of the list
    alarm_node->next = alarm;
    alarm->next = NULL;
    return alarm;
}

/**
 * Removes an alarm the list of alarms.
 *
 * The alarm list mutex MUST BE LOCKED by the caller of this method.
 *
 * The linked list of alarms is searched and when the correct ID is found, it
 * edits the linked list to remove that alarm. The node that was removes is
 * then returned.
 */
alarm_t *remove_alarm_from_list(int id)
{
    alarm_t *alarm_node = alarm_header.next;
    alarm_t *alarm_prev = &alarm_header;

    // Keeps on searching the list until it finds the correct ID
    while (alarm_node != NULL)
    {
        if (alarm_node->alarm_id == id)
        {
            alarm_prev->next = alarm_node->next;
            break; // Exit loop since ID has been found
        }
        // If the ID is not found, move to the next node
        alarm_node = alarm_node->next;
        alarm_prev = alarm_prev->next;
    }
    return alarm_node;
}

/**
 * Reactivates an alarm in the alarm list by traversing the list until it finds
 * the alarm with the specified id and setting its status to true (active).
 */
void reactivate_alarm_in_list(int alarm_id) {
    alarm_t *alarm = alarm_header.next;

    /*
     *Checks if the current alarm is the list is NULL
     */
    while(alarm != NULL){
        /*
         * Sets the alarms status to active and prints out the the alarm ID
         * followed by the time the alarm was reactivated and the reactivation
         * message.
         */
        if (alarm->alarm_id == alarm_id){
            alarm->status = true;
            printf(
                "Alarm (%d) Reactivated at %ld: %s\n",
                alarm->alarm_id,
                time(NULL),
                alarm->message
            );
        }
        alarm = alarm->next;
    }
}

/**
 * Checks if an alarm exists in the list of alarms based on a given ID.
 *
 * The alarm list mutex MUST BE LOCKED by the caller of this method.
 *
 * The linked list of alarms is searched and when the correct ID is
 * found, it returns the integer 1.  If the ID is not found and the
 * entire list has been searched, the integer 0 will be returned.
 */
int doesAlarmExist(int id)
{
    alarm_t *alarm_node = alarm_header.next;

    // Loop to iterate through list
    while (alarm_node != NULL)
    {
        // ID is found, return 1
        if (alarm_node->alarm_id == id)
        {
            return 1;
        }
        // ID not found, move to next node
        else
        {
            alarm_node = alarm_node->next;
        }
    }
    // List has been searched and ID does not exist, return 0
    return 0;
}


/**
 * Adds a thread to the thread list.
 *
 * The caller of this function must have the thread list mutex locked when
 * calling this function.
 */
void add_to_thread_list(thread_t *thread){
    thread_t *current_thread = &thread_header;

    while (current_thread != NULL){
        if(current_thread->next == NULL){
            current_thread->next = thread;
            break;
        }
        current_thread = current_thread->next;
    }
}

/**
 * Removes a thread from the thread list.
 *
 * The caller of this function must have the thread list mutex locked when
 * calling this function.
 *
 * The linked list of threads is searched and when the correct ID is found, it
 * edits the linked list to remove that alarm. The thread that was removed is
 * then returned.
 */
thread_t* remove_from_thread_list(thread_t *thread){
    thread_t *thread_node = thread_header.next;
    thread_t *thread_prev = &thread_header;

    // Keeps on searching the list until it finds the correct thread in the list
    while (thread_prev != NULL){
        if (thread_node == thread) {
            thread_prev->next = thread_node->next; // Remove from list.
            break;                                 // Exit loop.
        }
        // If the ID is not found, move to the next node
        thread_node = thread_node->next;
        thread_prev = thread_prev->next;
    }

    return thread_node;
}

/**
 * Checks if there are any threads in the thread list that have space for an
 * alarm. Returns true if all the threads are full (no space left), and false if
 * there is at least one thread with space for an alarm.
 */
bool thread_full_check(){
    thread_t *current_thread = thread_header.next;

    while(current_thread != NULL){
        if(current_thread->alarms != 2){
            return false;
        }
        current_thread = current_thread->next;
    }

    return true;
}

/**
 * DISPLAY THREAD
 * * * * * * * * *
 *
 * This is the function that will be used for display threads. It will loop
 * every 5 seconds, waiting on a condition variable. When the wait times out, it
 * prints the alarms that it holds, unless either of its alarm has expired, in
 * which case it will delete the alarm.
 *
 * The main thread communicates to the display threads through events and a
 * condition variable. When the main thread needs an event to be handled by a
 * display thread, it creates an event, stored in a global variable, and
 * broadcasts a condition variable.
 *
 * If a display thread can handle an event, it will perform actions to handle it
 * and delete the event once it is finished. Other display threads will not
 * handle a deleted event, ensuring the event is only handled once. If a thread
 * cannot handle an event, it will do nothing.
 *
 * Once a display thread has no alarms left (either because they expired or were
 * cancelled) it will remove and free its entry from the thread list and return
 * from this function (which will allow the thread to be recycled by the
 * operating system).
 */
void *client_thread(void *arg)
{
    thread_t *thread = ((thread_t *)arg); // The thread parameter passed to this
                                          // thread.

    alarm_t *alarm1 = NULL;               // The first slot for an alarm.

    alarm_t *alarm2 = NULL;               // The second slot for an alarm.

    int status;                           // Vaariable to hold the status
                                          // returned by timed condition
                                          // variable waits.

    time_t now;                           // Variable to hold the current time.

    struct timespec t;                    // Variable for setting timeout for
                                          // timed condition variable waits.

    DEBUG_PRINTF("Creating thread %d\n", thread->thread_id);

    /*
     * If the alarm passed to this thread is not NULL, then we should
     * take it as our first alarm.
     */
    if(thread->alarm != NULL){
        DEBUG_PRINTF(
            "Thread %d taking alarm %d via thread parameter\n",
            thread->thread_id,
            thread->alarm->alarm_id
        );

        // Take alarm as alarm1
        alarm1 = thread->alarm;

        // Update the number of available alarms for this thread in
        // the thread list. Note that we need the mutex to do this
        // because multiple threads can access the list of threads.
        pthread_mutex_lock(&thread_list_mutex);
        thread->alarms++;
        pthread_mutex_unlock(&thread_list_mutex);
    } else {
        DEBUG_PRINTF("Thread %d was not given an alarm\n", thread->thread_id);
    }

    /*
     * Lock the mutex so that this thread can access the alarm list.
     */
    pthread_mutex_lock(&alarm_list_mutex);

    while (1)
    {
        /*
         * If both alarms are NULL, then we can remove this thread.
         *
         * Note that the thread is given an alarm in the parameter when it is
         * created, so it should not exit immediately after creation.
         */
        if (alarm1 == NULL && alarm2 == NULL) {
            printf(
                "Display Alarm Thread %d Exiting at %ld\n",
                thread->thread_id,
                time(NULL)
            );

            /*
             * Lock thread list mutex, remove thread from list, free thread
             * (because it was malloced by the main thread), unlock thread list
             * mutex, and break so that we exit the main loop and this thread
             * can be destroyed.
             */
            pthread_mutex_lock(&thread_list_mutex);
            remove_from_thread_list(thread);
            free(thread);
            pthread_mutex_unlock(&thread_list_mutex);
            break;
        }

        /*
         * Get current time (in seconds from UNIX epoch).
         */
        now = time(NULL);

        /*
         * Calculate timeout.
         */
        if (alarm1 != NULL && alarm1->expiration_time - now < 5) {
            if (alarm2 == NULL) {
                // Alarm 1 expires first
                t.tv_sec = alarm1->expiration_time;
            } else if (alarm1->expiration_time < alarm2->expiration_time) {
                // Alarm 1 expires first
                t.tv_sec = alarm1->expiration_time;
            } else {
                // Alarm 2 expires first
                t.tv_sec = alarm2->expiration_time;
            }
        } else if (alarm2 != NULL && alarm2->expiration_time - now < 5) {
            if (alarm1 == NULL) {
                // Alarm 2 expires first
                t.tv_sec = alarm2->expiration_time;
            } else if (alarm2->expiration_time < alarm1->expiration_time) {
                // Alarm 2 expires first
                t.tv_sec = alarm2->expiration_time;
            } else {
                // Alarm 1 expires first
                t.tv_sec = alarm1->expiration_time;
            }
        } else {
            // Set timeout to 5 seconds in the future because none of the
            // alarmsare expiring soon.
            t.tv_sec = now + 5;
        }
        /*
         * Add 10 milliseconds (10,000,000 nanoseconds) to the timeout to make
         * sure the expiry is reached. If we don't do this, we get a bug where
         * the alarm prints out many times in quick succession before expiring.
         */
        t.tv_nsec = 10000000;

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
        if (status == ETIMEDOUT)
        {
            if (alarm1 != NULL && alarm1->expiration_time <= time(NULL)) {
                printf(
                    "Display Alarm Thread %d Removed Expired Alarm(%d) at "
                    "%ld: %d %s\n",
                    thread->thread_id,
                    alarm1->alarm_id,
                    time(NULL),
                    alarm1->time,
                    alarm1->message
                );

                /*
                 * If alarm1 has expired, remove it from the list, free the
                 * alarm, remove it from this thread, and update the thread list
                 * to show that this thread has another space left.
                 */
                remove_alarm_from_list(alarm1->alarm_id);
                free(alarm1);
                alarm1 = NULL;
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms--;
                pthread_mutex_unlock(&thread_list_mutex);
            }
            else if (alarm1 != NULL && alarm1->status == true)
            {
                /*
                 * If alarm 1 is defined in this thread, print it.
                 */
                printf(
                    "Alarm (%d) Printed by Alarm Display Thread %d at"
                    "%ld: %d %s\n",
                    alarm1->alarm_id,
                    thread->thread_id,
                    time(NULL),
                    alarm1->time,
                    alarm1->message);
            }

            if (alarm2 != NULL && alarm2->expiration_time <= time(NULL)) {
                printf(
                    "Display Alarm Thread %d Removed Expired Alarm(%d) at "
                    "%ld: %d %s\n",
                    thread->thread_id,
                    alarm2->alarm_id,
                    time(NULL),
                    alarm2->time,
                    alarm2->message
                );

                /*
                 * If alarm2 has expired, remove it from the list, free the
                 * alarm, remove it from this thread, and update the thread list
                 * to show that this thread has another space left.
                 */
                remove_alarm_from_list(alarm2->alarm_id);
                free(alarm2);
                alarm2 = NULL;
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms--;
                pthread_mutex_unlock(&thread_list_mutex);
            }
            else if (alarm2 != NULL && alarm2->status == true)
            {
                /*
                 * If alarm 2 is defined in this thread, print it.
                 */
                printf(
                    "Alarm (%d) Printed by Alarm Display Thread %d at"
                    "%ld: %d %s\n",
                    alarm2->alarm_id,
                    thread->thread_id,
                    time(NULL),
                    alarm2->time,
                    alarm2->message);
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
        if (event == NULL)
        {
            pthread_mutex_unlock(&event_mutex);
            continue;
        }

        /*
         * Event type of 1 means a new alarm was created.
         */
        if (event->type == Start_Alarm)
        {
            /*
             * We need to check if this thread's alarms are NULL to
             * see if we have capacity for the new alarm. If we take
             * the alarm, then we must free the event and set it to
             * NULL to show other threads that this event was already
             * handled.
             */
            if (alarm1 == NULL)
            {
                /*
                 * If alarm1 is NULL, then take the alarm.
                 */
                alarm1 = event->alarm;
                DEBUG_PRINTF("Thread took alarm %d\n", alarm1->alarm_id);
                free(event);
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms++;
                pthread_mutex_unlock(&thread_list_mutex);
                event = NULL;
            }
            else if (alarm2 == NULL)
            {
                /*
                 * If alarm2 is NULL, we can still take the alarm.
                 */
                alarm2 = event->alarm;
                DEBUG_PRINTF("Thread took alarm %d\n", alarm2->alarm_id);
                free(event);
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms++;
                pthread_mutex_unlock(&thread_list_mutex);
                event = NULL;
            }
            else
            {
                /*
                 * If both alarms are not NULL, we cannot take the
                 * alarm.
                 */
                DEBUG_PRINTF(
                    "Thread at capacity, did not take alarm %d\n",
                    event->alarm->alarm_id);
            }

            /*
             * Unlock event mutex so that the main thread can send
             * another event.
             */
            pthread_mutex_unlock(&event_mutex);
        }
        else if (event->type == Suspend_Alarm)
        {
            if (alarm1 != NULL
                && alarm1->alarm_id == event->alarmId
                && alarm1->status == true)
            {
                printf(
                    "Alarm (%d) Suspended at %ld: %s\n",
                    alarm1->alarm_id,
                    time(NULL),
                    alarm1->message);

                alarm1->status = false;
            }
            else if (alarm2 != NULL
                     && alarm2->alarm_id == event->alarmId
                     && alarm2->status == true)
            {
                printf(
                    "Alarm (%d) Suspended at %ld: %s\n",
                    alarm2->alarm_id,
                    time(NULL),
                    alarm2->message);

                alarm2->status = false;
            } else {
                DEBUG_PRINTF(
                    "Suspend_Alarm command for alarm %d was not handled by"
                    "thread &d\n",
                    event->alarmId,
                    thread->thread_id
                );
            }

            /*
             * Free the event structure because it has been handled.
             */
            free(event);
            event = NULL;
            pthread_mutex_unlock(&event_mutex);
        }
        else if (event->type == Cancel_Alarm)
        {
            if (alarm1 != NULL && alarm1->alarm_id == event->alarmId)
            {
                printf(
                    "Display Alarm Thread (%d) Removed Canceled Alarm(%d) at %ld: %s\n",
                    thread->thread_id,
                    alarm1->alarm_id,
                    time(NULL),
                    alarm1->message);

                // Free alarm
                free(alarm1);
                alarm1 = NULL;

                // Update thread list to show that this thread has one less
                // alarm.
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms--;
                pthread_mutex_unlock(&thread_list_mutex);

                // Free event since it is now handled.
                free(event);
                event = NULL;
                pthread_mutex_unlock(&event_mutex);
            }
            else if (alarm2 != NULL && alarm2->alarm_id == event->alarmId)
            {
                printf(
                    "Display Alarm Thread (%d) Removed Canceled Alarm(%d) at %ld: %s\n",
                    thread->thread_id,
                    alarm2->alarm_id,
                    time(NULL),
                    alarm2->message);

                // Free alarm.
                free(alarm2);
                alarm2 = NULL;

                // Update thread list to show that this thread has one less
                // alarm.
                pthread_mutex_lock(&thread_list_mutex);
                thread->alarms--;
                pthread_mutex_unlock(&thread_list_mutex);

                // Free event since it is now handled.
                free(event);
                event = NULL;
                pthread_mutex_unlock(&event_mutex);
            } else {
                DEBUG_PRINTF(
                    "Cancel_Alarm event for alarm %d not handled by"
                    "thread %d\n",
                    event->alarmId,
                    thread->thread_id
                );
            }
        }
        else if (event->type == View_Alarms) {
            printf("Display Thread %d Assigned:\n", thread->thread_id);

            if(alarm1 != NULL) {
                printf(
                    "Alarm(%d): Created at %ld: Assigned at %d %s "
                    "Status %d\n",
                    alarm1->alarm_id,
                    alarm1->creation_time,
                    alarm1->time,
                    alarm1->message,
                    alarm1->status);
            }
            if(alarm2 != NULL) {
                printf(
                    "Alarm(%d): Created at %ld: Assigned at %d %s "
                    "Status %d\n",
                    alarm2->alarm_id,
                    alarm2->creation_time,
                    alarm2->time,
                    alarm2->message,
                    alarm2->status);
            }
            pthread_mutex_unlock(&event_mutex);
        }

        DEBUG_PRINT_ALARM_LIST(alarm_header.next);
    }

    /*
     * Unlock alarm list mutex.
     */
    pthread_mutex_unlock(&alarm_list_mutex);
}

/**
 * MAIN THREAD
 * * * * * * *
 *
 * This is the function for the main thread (and the entry point to the
 * program). It reads commands from user input, parses the command, and performs
 * actions relating to the command.
 *
 * Commands will add, modify, and delete alarms. Each alarm is malloced by this
 * thread. Since each display thread can only hold a maximum of two alarms, this
 * thread is responsible for creating new display threads.
 *
 * When handling commands, the main thread will manipulate the alarm list and
 * create events and broadcast them to the display threads to be handled.
 * Display threads will wait on a condition variable, allowing the main thread
 * to wake up the display threads by broadcasting the condition variable.
 */
int main(int argc, char *argv[])
{
    char input[128];           // Buffer for user input.

    command_t *command;        // Pointer for the currently entered command.

    alarm_t *alarm;            // Pointer for newly created alarms.

    thread_t *next_thread;     // Pointer for newly created threads.

    int thread_id_counter = 0; // Counter for thread IDs. This will be
                               // incremented every time a thread is created so
                               // that each thread has a unique ID.

    DEBUG_PRINT_START_MESSAGE();

    while (1)
    {
        printf("Alarm > ");

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            continue;
        }
        // Replace newline with null terminating character
        input[strcspn(input, "\n")] = 0;
        command = parse_command(input);

        /*
         * If command is NULL, then the command was invalid.
         */
        if (command == NULL)
        {
            printf("Bad command\n");
            continue;
        }
        /*
         * Command was  valid, so we can handle it.
         */
        else {
            DEBUG_PRINT_COMMAND(command);

            /*
             * Lock the mutex for the alarm list, so that no other
             * threads can access the list until we are finished
             * updating it.
             */
            pthread_mutex_lock(&alarm_list_mutex);

            if (command->type == Start_Alarm)
            {
                /*
                 * Allocate space for alarm.
                 */
                alarm = malloc(sizeof(alarm_t));
                if (alarm == NULL)
                {
                    errno_abort("Malloc failed");
                }

                /*
                 * Fill in data for alarm.
                 */
                alarm->alarm_id = command->alarm_id;
                alarm->time = command->time;
                strcpy(alarm->message, command->message);
                alarm->status = true;
                alarm->creation_time = time(NULL);
                alarm->expiration_time = time(NULL) + alarm->time;

                /*
                 * Insert alarm into the list.
                 */
                if (insert_alarm_into_list(alarm) == NULL)
                {
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

                printf(
                    "Alarm %d Inserted Into Alarm List at %ld: %d %s\n",
                    alarm->alarm_id,
                    time(NULL),
                    alarm->time,
                    alarm->message
                );

                DEBUG_PRINTF("threads: ");
                DEBUG_PRINT_THREAD_LIST(thread_header);
                DEBUG_PRINTF("alarms: ");
                DEBUG_PRINT_ALARM_LIST(alarm_header.next);

                /*
                 * If all the threads are full we need to make a new thread for
                 * the new alarm.
                 */
                if (thread_full_check() == true){
                    // Allocate space for a new thread.
                    next_thread = malloc(sizeof(thread_t));
                    if (next_thread == NULL) {
                        errno_abort("Malloc failed");
                    }
                    // Fill in data for the thread
                    next_thread->thread_id = thread_id_counter;
                    next_thread->alarms = 0;
                    next_thread->next = NULL;
                    next_thread->alarm =  alarm;

                    // Increment thread ID counter
                    thread_id_counter++;

                    // Add the thread to the thread list. We need to lock the
                    // thread list mutex first.
                    pthread_mutex_lock(&thread_list_mutex);
                    add_to_thread_list(next_thread);
                    pthread_mutex_unlock(&thread_list_mutex);

                    // Create the new thread.
                    pthread_create(
                        &next_thread->thread,
                        NULL,
                        client_thread,
                        next_thread
                    );

                    DEBUG_PRINT_THREAD_LIST(thread_header);

                    printf(
                        "New Display Alarm Thread %d Created at %ld: %d %s\n",
                        next_thread->thread_id,
                        time(NULL),
                        alarm->time,
                        alarm->message
                    );
                    
                    /*
                     * Unlock the mutex now since we don't want to emit an event
                     * here.
                     */
                    pthread_mutex_unlock(&alarm_list_mutex);
                    continue;
                }
                else{
                    /*
                     * In this case, there is at least one thread with space for
                     * the new alarm. So we just need to send the event without
                     * creating a new thread.
                     */
                    pthread_mutex_lock(&event_mutex);
                    event = malloc(sizeof(event_t));
                    if (event == NULL) {
                        errno_abort("Malloc failed");
                    }
                    event->type = Start_Alarm;
                    event->alarm = alarm;
                    pthread_mutex_unlock(&event_mutex);

                }            
            }
            else if (command->type == Change_Alarm)
            {
                // Check if the alarm exists, if not return error
                // message and unlock the mutex.
                if (!doesAlarmExist(command -> alarm_id))
                {
                    printf(
                        "Alarm of ID %d does not exist.\n",
                        command->alarm_id
                    );
                    pthread_mutex_unlock( & alarm_list_mutex);
                    continue;
                }

                // Go through list and find the existing alarm using the ID
                alarm_t *existing_alarm = find_alarm_by_id(command -> alarm_id);
                
                //Update the existing alarm time and message.                
                existing_alarm -> time = command -> time;
                existing_alarm->expiration_time = existing_alarm->creation_time
                    + command->time;
                strcpy(existing_alarm -> message, command -> message);

                //Return display message showing alarm has changed.
                printf(
                    "Alarm (%d) Changed at %ld: %s\n",
                    command->alarm_id,
                    time(NULL),
                    command->message
                );
            }
            else if (command->type == Cancel_Alarm)
            {
                // Get the ID that will be cancellled
                int cancelId = command->alarm_id;

                int AlarmExists = doesAlarmExist(cancelId);

                if (AlarmExists == 0)
                {
                    printf("Not a valid ID.\n");
                }
                else
                {
                    /*
                     * Remove alarm from list
                     */
                    remove_alarm_from_list(cancelId);

                    /*
                     * Send cancel alarm event to thread.
                     */
                    pthread_mutex_lock(&event_mutex);
                    event = malloc(sizeof(event_t));
                    if (event == NULL) {
                        errno_abort("Malloc failed");
                    }
                    event->type = Cancel_Alarm;
                    event->alarmId = cancelId;
                    pthread_mutex_unlock(&event_mutex);
                }
            }
            else if (command->type == Reactivate_Alarm)
            {
                int AlarmExists = doesAlarmExist(command->alarm_id);
                if (AlarmExists == 0)
                {
                    printf("Not a valid ID.\n");
                }
                else
                {
                    /*
                     * Reactivate the alarm in the list. Note that we don't use
                     * an event because it is simpler to just reactivate it
                     * here. Since the thread owning this alarm shares the
                     * reference to this alarm in the list, it will "notice"
                     * the change in status of the alarm.
                     */
                    reactivate_alarm_in_list(command->alarm_id);
                }
            }
            else if (command->type == Suspend_Alarm)
            {
                // Gets the ID that will be suspended
                int suspendId = command->alarm_id;

                int AlarmExists = doesAlarmExist(suspendId);
                if (AlarmExists == 0)
                {
                    printf("Not a valid ID.\n");
                }
                else
                {
                    /*
                     * Send event to display threads.
                     */
                    pthread_mutex_lock(&event_mutex);
                    // Allocate memory for the event structure
                    event = malloc(sizeof(event_t));
                    if (event == NULL) {
                        errno_abort("Malloc failed");
                    }
                    event->type = Suspend_Alarm;
                    event->alarmId = suspendId;
                    pthread_mutex_unlock(&event_mutex);
                }
            }
            else if (command->type == View_Alarms) {
                printf("View Alarms at %ld: \n", time(NULL));
                pthread_mutex_lock(&event_mutex);
                event = malloc(sizeof(event_t));
                if (event == NULL) {
                    errno_abort("Malloc failed");
                }
                event->type = View_Alarms;
                pthread_mutex_unlock(&event_mutex);
            }

            DEBUG_PRINT_ALARM_LIST(alarm_header.next);

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

