#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "jobs.h"

process_t *process_list = NULL;

static void update_process_status(int pid, int status) {
    process_t *curr = process_list;

    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            break;
        }
        curr = curr->next;
    }
}

void jobs_add_process(const char *cmd_name, pid_t pid) {
    // Block SIGCHLD to prevent list traversal race conditions
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

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

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void jobs_print() {
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

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

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void jobs_free() {
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    while (process_list) {
        process_t *curr = process_list;
        process_list = process_list->next;

        if (curr->status != TERMINATED) {
            kill(curr->pid, SIGTERM);
            kill(curr->pid, SIGCONT); // Wake-and-kill for suspended processes
        }

        free(curr->cmd_name);
        free(curr);
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void jobs_sigchld_handler(int sig) {
    // Silence compiler warnings for mandatory POSIX signature parameters
    (void)sig;

    int saved_errno = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFSTOPPED(status)) {
            update_process_status(pid, SUSPENDED);
        } else if (WIFCONTINUED(status)) {
            update_process_status(pid, RUNNING);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            update_process_status(pid, TERMINATED);
        }
    }

    errno = saved_errno;
}

int jobs_execute_signal(const char *signal_name, pid_t pid) {

    if (strcmp(signal_name, "halt") == 0) {
        if (kill(pid, SIGTSTP) == 0) {
            printf("[INFO] Jobs: process %d suspended (SIGTSTP).\n", pid);
        } else {
            fprintf(stderr, "[ERROR] Jobs: failed to halt PID %d - %s\n", pid, strerror(errno));
            return -1;
        }
    } else if (strcmp(signal_name, "wakeup") == 0) {
        if (kill(pid, SIGCONT) == 0) {
            printf("[INFO] Jobs: process %d resumed (SIGCONT).\n", pid);
        } else {
            fprintf(stderr, "[ERROR] Jobs: failed to wakeup PID %d - %s\n", pid, strerror(errno));
            return -1;
        }
    } else if (strcmp(signal_name, "ice") == 0) {
        if (kill(pid, SIGTERM) == 0) {
            kill(pid, SIGCONT);
            printf("[INFO] Jobs: process %d terminated (SIGTERM).\n", pid);
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