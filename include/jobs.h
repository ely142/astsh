#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define TERMINATED -1
#define RUNNING    1
#define SUSPENDED  0

typedef struct process {
    char *cmd_name;
    pid_t pid;
    int status;
    struct process *next;
} process_t;

void jobs_add_process(const char *cmd_name, pid_t pid);
void jobs_print();
void jobs_free();
void jobs_sigchld_handler(int sig);
int jobs_execute_signal(const char *signal_name, pid_t pid);

#endif