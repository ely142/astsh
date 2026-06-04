#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "executor.h"
#include "history.h"
#include "jobs.h"
#include "line_parser.h"

#define BUFFER_SIZE 2048

int debug_mode = 0;

void prompt();

int main(int argc, char **argv) {

    char buffer[BUFFER_SIZE];

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) { // properly compare strings in c - stackoverflow.com
            debug_mode = 1;
        }
    }

    while (1) {
        prompt();

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) { // checks for EOF
            printf("\n");
            exit(0);
        }
        buffer[strcspn(buffer, "\n")] =
            '\0'; // removing trailing newline character from fgets() input - stackoverflow.com

        if (strlen(buffer) == 0) { // skips if user just pressed 'enter'
            continue;
        }

        if (strcmp(buffer, "quit") == 0) {
            break;
        }

        if (strcmp(buffer, "hist") == 0) { // history commands handling
            add_to_history(buffer);
            print_history();
            continue;
        }

        else if (strcmp(buffer, "!!") == 0) {
            int history_size = get_history_size();
            if (history_size == 0) {
                fprintf(stderr, "ERROR - invalid index of history command\n");
                continue;
            } else {
                const char *command = get_history_command(history_size);
                if (command == NULL) {
                    continue;
                }
                printf("%s\n", command);
                strcpy(buffer, command);
            }
        }

        else if (buffer[0] == '!' && buffer[1] != '\0') {
            int n = atoi(buffer + 1);
            int history_size = get_history_size();
            if (n < 1 || n > history_size) {
                fprintf(stderr, "ERROR - invalid index of history command\n");
                continue;
            } else {
                const char *command = get_history_command(n);
                if (command == NULL) {
                    continue;
                }
                printf("%s\n", command);
                strcpy(buffer, command);
            }
        }

        cmd_line *parsed_line = line_parser_parse(buffer);

        if (parsed_line == NULL) {
            fprintf(stderr, "ERROR - failed to parse command\n");
            continue;
        }

        add_to_history(buffer);

        if (strcmp(parsed_line->arguments[0], "cd") == 0) { // cd command handling
            if (parsed_line->arg_count != 2) {
                fprintf(stderr, "ERROR - mismatch arguments count for cd operation\n");
            } else {
                if (chdir(parsed_line->arguments[1]) == -1) { // linux man page for chdir
                    fprintf(stderr, "ERROR - chdir() operation failed\n");
                }
            }
            line_parser_free(parsed_line);
        }

        else if ((strcmp(parsed_line->arguments[0], "halt") == 0) ||
                 (strcmp(parsed_line->arguments[0], "wakeup") == 0) ||
                 (strcmp(parsed_line->arguments[0], "ice") == 0)) { // signal commands handling
            if (parsed_line->arg_count != 2) {
                fprintf(stderr, "ERROR - mismatch arguments count for %s signal operation\n",
                        parsed_line->arguments[0]);
            } else {
                int pid_to_signal = atoi(parsed_line->arguments[1]);

                if (pid_to_signal == 0) { // user based process have pid != 0, therefore indicates atoi error
                    fprintf(stderr, "ERROR - failed to convert pid from string to int for %s signaling process\n",
                            parsed_line->arguments[0]);
                } else {
                    if (process_signal(parsed_line->arguments[0], pid_to_signal) == -1) {
                        fprintf(stderr, "ERROR - failed to send the signal %s to the process whose pid is %d\n",
                                parsed_line->arguments[0], pid_to_signal);
                    }
                }
            }
            line_parser_free(parsed_line);
        }

        else if (strcmp(parsed_line->arguments[0], "procs") == 0) { // procs command handling
            print_process_list();
            line_parser_free(parsed_line);
        }

        else if (parsed_line->next != NULL) { // piped commands handling
            if (handle_pipe(parsed_line) == -1) {
                fprintf(stderr, "ERROR - failed to handle piped command\n");
            }
        }

        else { // execute command handling
            execute(parsed_line);
        }
    }
    free_process_list();
    free_history();
    return 0;
}

void prompt() {
    char cwd_path[PATH_MAX];

    if (getcwd(cwd_path, PATH_MAX) == NULL) {
        fprintf(stderr, "ERROR - failed to get the current working directory\n");
    } else {
        printf("%s #> ", cwd_path);
    }
}
