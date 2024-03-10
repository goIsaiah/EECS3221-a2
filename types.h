#include <stdbool.h>

/**
 * The six possible types of commands that a user can enter.
 */
typedef enum command_type
{
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
typedef struct command_t
{
    command_type type;
    int alarm_id;
    int time;
    char message[128];
} command_t;

/**
 * This is the data type that holds information about parsing a
 * command. It contains the type of the command, the regular
 * expression for the command, the number of matches within the
 * command (that must be parsed out).
 */
typedef struct regex_parser
{
    command_type type;
    const char *regex_string;
    int expected_matches;
} regex_parser;

/**
 * Data type for an alarm.
 *
 *   - `alarm_id` is the ID of the alarm.
 *   - `time` is the time entered by the user.
 *   - `message` is the message entered by the user.
 *   - `next` is the next alarm in the list (since alarms will be
 *     stored as a linked list).
 *   - `status` is the active status of the alarm. If `status` is
 *     true, then the alarm is activated, otherwise the alarm is
 *     suspended.
 *   - `creation_time` is the creation timestamp of the alarm.
 */
typedef struct alarm_t
{
    int alarm_id;
    int time;
    char message[128];
    struct alarm_t *next;
    bool status;
    time_t creation_time;
    time_t expiration_time;
} alarm_t;

/**
 * Data type representing an event that is sent from the main thread
 * to a display thread.
 *
 *   - `type` is the type of the event, directly correlated to a
 *     command entered by a user.
 *   - `alarmId` is the ID of the alarm that is related to the event.
 *     Note that this is not applicable to every event (for example,
 *     the View_Alarms command is not associated with any specific
 *     alarm).
 *   - `alarm` is a pointer to the alarm that is related to the event.
 *     Note that this is not applicable to every event (for example,
 *     the View_Alarms command is not associated with any specific
 *     alarm).
 */
typedef struct event_t
{
    command_type type;
    int alarmId;
    alarm_t *alarm;
} event_t;

/**
 * Data type representing a display thread.
 *
 *  - `thread_id` is our ID that we give to a thread.
 *  - `alarms` is the number of alarms that the thread currently has.
 *  - `thread` is the pthread handle for the thread.
 *  - `next` is the next thread in the list (since threads will be
 *     stored as a linked list).
 *  - `alarm` is the inital alarm that is given to the thread when it
 *     is created.
 */
typedef struct thread_t
{
    int thread_id;
    int alarms;
    pthread_t thread;
    struct thread_t *next;
    alarm_t *alarm;

} thread_t;

