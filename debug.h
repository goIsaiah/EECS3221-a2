#include <stdarg.h>
#include "types.h"

#ifdef DEBUG

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
void debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("\x1B[36m");
    vprintf(format, args);
    printf("\x1B[0m");
    va_end(args);
}

#define DEBUG_PRINTF(...) debug_printf(__VA_ARGS__)

void debug_print_start_message() {
    debug_printf("EECS Assignment 2 Debug Mode\n");
    debug_printf("============================\n");
    debug_printf("\n");
    debug_printf("Messages in blue (this color) are debug ");
    debug_printf("messages and white text is the actual output of ");
    debug_printf("the program.");
    debug_printf("\n\n");
}

#define DEBUG_PRINT_START_MESSAGE() debug_print_start_message()

void debug_print(const char *message) {
    debug_printf("%s", message);
}

#define DEBUG_PRINT(message) debug_print(message);

void debug_print_command(command_t *command) {
    debug_printf(
        "{type: %d, id: %d, time: %d, message: %s}\n",
        command->type,
        command->alarm_id,
        command->time,
        command->message
    );
}

#define DEBUG_PRINT_COMMAND(command) debug_print_command(command)

void debug_print_alarm(alarm_t *alarm) {
    debug_printf(
        "{id: %d, time: %d, message: %s}\n",
        alarm->alarm_id,
        alarm->time,
        alarm->message
    );
}

#define DEBUG_PRINT_ALARM(alarm) debug_print_alarm(alarm)

void debug_print_alarm_list(alarm_t *alarm_list_head) {
    alarm_t *alarm = alarm_list_head;
    debug_printf("[");
    while (alarm != NULL) {
        debug_printf(
            "{id: %d, time: %d, message: %s}",
            alarm->alarm_id,
            alarm->time,
            alarm->message
        );
        alarm = alarm->next;
        if (alarm != NULL) {
            printf(", ");
        }
    }
    debug_printf("]\n");
}
#define DEBUG_PRINT_ALARM_LIST(alarm_list_head) debug_print_alarm_list(alarm_list_head)

void debug_print_thread_list(thread_t thread_list_header){
    thread_t *thread = &thread_list_header;
    debug_printf("[");
    while (thread != NULL){
        debug_printf(
            "{id: %d, alarms: %d, next: %p}",
            thread->thread_id,
            thread->alarms,
            thread->next
        );
        thread = thread->next;
        if(thread != NULL){
            printf(",");
        }
    }
    debug_printf("]\n");

}
#define DEBUG_PRINT_THREAD_LIST(thread_list_header) debug_print_thread_list(thread_list_header)


#else

#define DEBUG_PRINTF(...)

#define DEBUG_PRINT_START_MESSAGE()

#define DEBUG_PRINT(message)

#define DEBUG_PRINT_COMMAND(command)

#define DEBUG_PRINT_ALARM(alarm)

#define DEBUG_PRINT_ALARM_LIST(alarm_list_head)

#define DEBUG_PRINT_THREAD_LIST(thread_list_header)

#endif

