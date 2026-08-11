#include "builtins.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int builtins_is_command(const char *cmd) {
    if (!cmd)
        return 0;

    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "procs") == 0 || strcmp(cmd, "halt") == 0 || strcmp(cmd, "wakeup") == 0 ||
        strcmp(cmd, "ice") == 0) {
        return 1;
    }

    return 0;
}

int builtins_execute(ast_node_t *ast) {
    if (!ast || ast->type != NODE_COMMAND) {
        return -1;
    }

    char **argv = ast->data.command.argv;
    if (!argv || !argv[0]) {
        return -1;
    }

    // Helper: calculate argc for the current command node
    int arg_count = 0;
    while (argv[arg_count] != NULL) {
        arg_count++;
    }

    if (strcmp(argv[0], "cd") == 0) {
        if (arg_count != 2) {
            fprintf(stderr, "[ERROR] Built-in: 'cd' requires exactly one argument\n");
            return -1;
        }

        if (chdir(argv[1]) == -1) {
            perror("[ERROR] Built-in: chdir() failed");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "procs") == 0) {
        jobs_print();
        return 0;
    }

    if ((strcmp(argv[0], "halt") == 0) || (strcmp(argv[0], "wakeup") == 0) || (strcmp(argv[0], "ice") == 0)) {

        if (arg_count != 2) {
            fprintf(stderr, "[ERROR] Built-in: '%s' requires a valid PID argument\n", argv[0]);
            return -1;
        }

        // atoi returns 0 on failure, PID 0 is a system process we shouldn't signal anyway
        int pid_to_signal = atoi(argv[1]);

        if (pid_to_signal <= 0) {
            fprintf(stderr, "[ERROR] Built-in: invalid PID '%s' provided to '%s'\n", argv[1], argv[0]);
            return -1;
        }

        if (jobs_execute_signal(argv[0], pid_to_signal) == -1) {
            fprintf(stderr, "[ERROR] Built-in: failed to send '%s' signal to PID %d\n", argv[0], pid_to_signal);
            return -1;
        }
        return 0;
    }

    return -1;
}