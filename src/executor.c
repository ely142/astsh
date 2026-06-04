#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"
#include "jobs.h"

extern int debug_mode;

void execute(cmd_line *cmd) {

    pid_t pid;
    int status;

    if (!(pid = fork())) { // child process, pattern taken from lecture 2

        if (debug_mode) {
            fprintf(stderr, "process pid: %d, executing command: %s\n", pid, cmd->arguments[0]);
        }

        if (cmd->input_redirect != NULL) {
            close(STDIN_FILENO);                             // closing the standard input stream (fd 0)
            if (open(cmd->input_redirect, O_RDONLY) == -1) { // man page open for reading
                fprintf(stderr, "ERROR - failed to redirect input\n");
                _exit(1);
            }
        }

        if (cmd->output_redirect != NULL) {
            close(STDOUT_FILENO); // closing the standard output stream (fd 1)
            if (open(cmd->output_redirect, O_CREAT | O_WRONLY | O_TRUNC, 0644) ==
                -1) { // man page open for writing + create file if doesn't exist
                fprintf(stderr, "ERROR - failed to redirect output\n");
                _exit(1);
            }
        }

        if (execvp(cmd->arguments[0], cmd->arguments) == -1) {
            perror("ERROR - execvp() error happened");
            _exit(1);
        }
    }

    else { // parent process
        if (debug_mode) {
            fprintf(stderr, "process pid: %d, executing command: %s\n", pid, cmd->arguments[0]);
        }

        if (cmd->is_blocking) { // parent process will wait for child if & isn't provided with the command
            if (waitpid(pid, &status, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() operation failed for the given pid: %d, status: %d\n", pid, status);
            }
        }
        add_process(cmd, pid);
    }
}

int handle_pipe(cmd_line *cmd) {
    int fd[2];
    pid_t pid_first, pid_second;

    if (cmd->output_redirect != NULL) {
        fprintf(stderr, "ERROR - output redirection on the left side of a piped command isn't allowed\n");
        return -1;
    }

    if (cmd->next->input_redirect != NULL) {
        fprintf(stderr, "ERROR - input redirection on the right side of a piped command isn't allowed\n");
        return -1;
    }

    if (pipe(fd) == -1) {
        perror("ERROR - failed to create a pipe\n");
        return -1;
    }

    if (!(pid_first = fork())) { // first child process

        if (cmd->input_redirect != NULL) {
            int input_fd = open(cmd->input_redirect, O_RDONLY);
            if (input_fd == -1) {
                fprintf(stderr, "ERROR - failed to redirect input\n");
                _exit(1);
            }

            if (dup2(input_fd, STDIN_FILENO) == -1) {
                fprintf(stderr, "ERROR - dup2 failure\n");
                _exit(1);
            }
            close(input_fd);
        }

        close(STDOUT_FILENO);
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "ERROR - dup2 failure\n");
            _exit(1);
        }
        close(fd[0]);
        close(fd[1]);

        if (execvp(cmd->arguments[0], cmd->arguments) == -1) {
            perror("ERROR - execvp() error happened");
            _exit(1);
        }
        exit(1);
    }

    else { // parent process
        add_process(cmd, pid_first);
        close(fd[1]);

        if (!(pid_second = fork())) { // second child process
            if (cmd->next->output_redirect != NULL) {
                int output_fd = open(cmd->next->output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    fprintf(stderr, "ERROR - failed to redirect output\n");
                    _exit(1);
                }
                if (dup2(output_fd, STDOUT_FILENO) == -1) {
                    fprintf(stderr, "ERROR - dup2 failure\n");
                    _exit(1);
                }
                close(output_fd);
            }
            close(STDIN_FILENO);
            if (dup2(fd[0], STDIN_FILENO) == -1) {
                fprintf(stderr, "ERROR - dup2 failure\n");
                _exit(1);
            }
            close(fd[0]);
            close(fd[1]);

            if (execvp(cmd->next->arguments[0], cmd->next->arguments) == -1) {
                perror("ERROR - execvp() error happened");
                _exit(1);
            }
            exit(1);
        }

        else { // parent process
            add_process(cmd->next, pid_second);

            close(fd[0]);
            close(fd[1]);

            if (waitpid(pid_first, NULL, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() failed\n");
            }
            if (waitpid(pid_second, NULL, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() failed\n");
            }

            cmd->next = NULL; // ensuring freeCmdLines for the first child process won't erase the next piped
                              // command process, as it will be handled seperatly in the process manager
        }
    }
    return 1;
}
