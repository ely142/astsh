#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "jobs.h"

static process_t *process_list = NULL;

void add_process(cmd_line *cmd, pid_t pid) {
    process_t *new_proc = (process_t *)malloc(sizeof(process_t));

    if (new_proc == NULL) {
        fprintf(stderr, "ERROR - failed to allocate memory for the new process\n");
        exit(1);
    }

    new_proc->cmd = cmd;
    new_proc->pid = pid;
    new_proc->status = RUNNING; // default state
    new_proc->next = process_list;
    process_list = new_proc;
}

void print_process_list() {
    update_process_list();
    process_t *curr = process_list;
    process_t *prev = NULL; // head of the list doesn't have a previous node
    const char *stat;
    int index = 0;

    printf("Index\t\tPID\t\tSTATUS\t\tCommand & Arguments\n");
    while (curr != NULL) {
        if (curr->status == RUNNING) {
            stat = "Running";
        } else if (curr->status == SUSPENDED) {
            stat = "Suspended";
        } else {
            stat = "Terminated";
        }

        printf("%d\t\t%d\t\t%s\t\t%s ", index, curr->pid, stat, curr->cmd->arguments[0]);

        for (int i = 1; curr->cmd->arguments[i] != NULL; i++) {
            printf("%s ", curr->cmd->arguments[i]);
        }
        printf("\n");
        index++;

        if (curr->status == TERMINATED) { // cleaning freshly terminated processes from the list
            if (prev == NULL) {
                process_list = curr->next;
            } else {
                prev->next = curr->next;
            }

            if (curr->cmd != NULL) {
                line_parser_free(curr->cmd);
                curr->cmd = NULL;
            }

            process_t *next = curr->next;
            free(curr);
            curr = next; // prev stays the same
        }

        else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void free_process_list() {

    while (process_list != NULL) {
        process_t *curr = process_list;
        process_list = process_list->next;

        if (curr->cmd != NULL) {
            line_parser_free(curr->cmd);
        }
        free(curr);
    }
}

void update_process_list() {
    process_t *curr = process_list;

    while (curr != NULL) {
        int status;
        pid_t returned_val = waitpid(curr->pid, &status, WNOHANG); // waitpid(2) - linux man page

        if (returned_val > 0) {
            if (WIFSTOPPED(status)) {
                update_process_status(curr->pid, SUSPENDED);
            } else if (WIFCONTINUED(status)) {
                update_process_status(curr->pid, RUNNING);
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                update_process_status(curr->pid, TERMINATED);
            }

        } else if (returned_val == -1) { // process not found -> considered 'terminated'
            update_process_status(curr->pid, TERMINATED);
        }
        // else - returned_val == 0, process still running, hasn't changed state

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
            printf("Process %d successfully stopped\n", pid);
            update_process_status(pid, SUSPENDED);
        } else {
            return -1;
        }
    } else if (strcmp(signal_name, "wakeup") == 0) {
        if (kill(pid, SIGCONT) == 0) {
            printf("Process %d successfully continued\n", pid);
            update_process_status(pid, RUNNING);
        } else {
            return -1;
        }
    } else if (strcmp(signal_name, "ice") == 0) {
        if (kill(pid, SIGINT) == 0) {
            printf("Process %d successfully terminated\n", pid);
            update_process_status(pid, TERMINATED);
        } else {
            return -1;
        }
    } else {
        fprintf(stderr, "ERROR - the signal %s is not supported in this program\n", signal_name);
        return -1;
    }

    return 0;
}