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

typedef struct alarm_t
{
    int alarm_id;
    int time;
    char message[128];
    struct alarm_t *next;
    bool state;
} alarm_t;
