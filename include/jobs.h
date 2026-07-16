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

void add_process(const char *cmd_name, pid_t pid);
void print_process_list();
void free_process_list();
void update_process_list();
void update_process_status(int pid, int status);
int process_signal(const char *signal_name, pid_t pid);

#endif