#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "jobs.h"

static process_t *process_list = NULL;

void add_process(const char *cmd_name, pid_t pid) {
    process_t *new_proc = (process_t *)malloc(sizeof(process_t));

    if (!new_proc) {
        fprintf(stderr, "[ERROR] Jobs: memory allocation failed for new process.\n");
        exit(1);
    }

    // Deep copy the string to ensure memory safety
    new_proc->cmd_name = strdup(cmd_name ? cmd_name : "unknown_job");
    new_proc->pid = pid;
    new_proc->status = RUNNING; // Default state
    new_proc->next = process_list;
    process_list = new_proc;
}

void print_process_list() {
    update_process_list();
    process_t *curr = process_list;
    process_t *prev = NULL;

    printf("Index\t\tPID\t\tSTATUS\t\tCommand\n");
    int index = 0;

    while (curr) {
        const char *stat = (curr->status == RUNNING)     ? "Running"
                           : (curr->status == SUSPENDED) ? "Suspended"
                                                         : "Terminated";

        printf("%d\t\t%d\t\t%s\t\t%s\n", index, curr->pid, stat, curr->cmd_name);
        index++;

        // Clean freshly terminated processes from the list
        if (curr->status == TERMINATED) {
            if (!prev) {
                process_list = curr->next;
            } else {
                prev->next = curr->next;
            }

            process_t *next = curr->next;
            free(curr->cmd_name);
            free(curr);
            curr = next;
        }

        else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void free_process_list() {

    while (process_list) {
        process_t *curr = process_list;
        process_list = process_list->next;

        free(curr->cmd_name);
        free(curr);
    }
}

void update_process_list() {
    process_t *curr = process_list;

    while (curr) {
        int status;
        pid_t returned_val = waitpid(curr->pid, &status, WNOHANG);

        if (returned_val > 0) {
            if (WIFSTOPPED(status)) {
                update_process_status(curr->pid, SUSPENDED);
            } else if (WIFCONTINUED(status)) {
                update_process_status(curr->pid, RUNNING);
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                update_process_status(curr->pid, TERMINATED);
            }

        }

        else if (returned_val == -1) {
            // If process is completely gone (ECHILD), mark as terminated
            update_process_status(curr->pid, TERMINATED);
        }

        curr = curr->next;
    }
}

void update_process_status(int pid, int status) {
    process_t *curr = process_list;

    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            break;
        }
        curr = curr->next;
    }
}

int process_signal(const char *signal_name, pid_t pid) {

    if (strcmp(signal_name, "halt") == 0) {
        if (kill(pid, SIGTSTP) == 0) {
            printf("[INFO] Jobs: process %d suspended (SIGTSTP).\n", pid);
            update_process_status(pid, SUSPENDED);
        } else {
            fprintf(stderr, "[ERROR] Jobs: failed to halt PID %d - %s\n", pid, strerror(errno));
            return -1;
        }
    } else if (strcmp(signal_name, "wakeup") == 0) {
        if (kill(pid, SIGCONT) == 0) {
            printf("[INFO] Jobs: process %d resumed (SIGCONT).\n", pid);
            update_process_status(pid, RUNNING);
        } else {
            fprintf(stderr, "[ERROR] Jobs: failed to wakeup PID %d - %s\n", pid, strerror(errno));
            return -1;
        }
    } else if (strcmp(signal_name, "ice") == 0) {
        if (kill(pid, SIGINT) == 0) {
            printf("[INFO] Jobs: process %d terminated (SIGINT).\n", pid);
            update_process_status(pid, TERMINATED);
        } else {
            fprintf(stderr, "[ERROR] Jobs: failed to ice PID %d - %s\n", pid, strerror(errno));
            return -1;
        }
    }

    else {
        fprintf(stderr, "[ERROR] Jobs: unsupported signal '%s'.\n", signal_name);
        return -1;
    }

    return 0;
}