#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"
#include "jobs.h"

extern int debug_mode;

static void execute_process(ast_node_t *node);
static void exec_pipe(node_pipe_t *pipe_node);
static void exec_background(node_background_t *bg);

/* =========================================================
 * LEGACY CODE (Temporarily disabled for AST testing)
 * ========================================================= */
#if 0

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

#endif

void executor_run_ast(ast_node_t *node) {
    if (!node)
        return;

    switch (node->type) {
        // Foreground tasks: fork once to protect the parent shell
        case NODE_COMMAND:
        case NODE_REDIRECT: {
            pid_t pid = fork();

            if (pid < 0) {
                fprintf(stderr, "[ERROR] foreground fork failed: %s.\n", strerror(errno));
                return;
            }

            if (pid == 0) {
                execute_process(node);
            }

            waitpid(pid, NULL, 0);
            break;
        }

        case NODE_PIPE:
            exec_pipe(&node->data.pipe);
            break;

        case NODE_BACKGROUND:
            exec_background(&node->data.background);
            break;

        default:
            break;
    }
}

static void execute_process(ast_node_t *node) {
    while (node) {
        switch (node->type) {
            case NODE_COMMAND: {
                char **argv = node->data.command.argv;

                if (argv && argv[0]) {
                    execvp(argv[0], argv);
                    fprintf(stderr, "[ERROR] %s: %s\n", argv[0], strerror(errno));
                }
                _exit(EXIT_FAILURE);
            }

            case NODE_REDIRECT: {
                node_redirect_t *redir = &node->data.redirect;

                if (!redir || !redir->child || !redir->file) {
                    fprintf(stderr, "[ERROR] invalid redirection node execution.\n");
                    _exit(EXIT_FAILURE);
                }

                int fd = open(redir->file, redir->open_flags, 0644);

                if (fd < 0) {
                    fprintf(stderr, "[ERROR] open failed for %s: %s\n", redir->file, strerror(errno));
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, redir->target_fd) == -1) {
                    fprintf(stderr, "[ERROR] dup2 failure: %s\n", strerror(errno));
                    _exit(EXIT_FAILURE);
                }
                close(fd);

                // Tail-call elimination: traverse to the child node and loop
                // instead of recursing to prevent stack frame accumulation
                node = redir->child;
                break;
            }

            case NODE_PIPE:
                exec_pipe(&node->data.pipe);
                _exit(EXIT_SUCCESS);

            case NODE_BACKGROUND:
                exec_background(&node->data.background);
                _exit(EXIT_SUCCESS);

            default:
                _exit(EXIT_FAILURE);
        }
    }
    // Fallback if node becomes NULL
    _exit(EXIT_FAILURE);
}

static void exec_background(node_background_t *bg) {
    if (!bg || !bg->child)
        return;

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "[ERROR] [PARENT PID %d] process creation failed: %s.\n", getpid(), strerror(errno));
        return;
    }

    if (pid == 0) {
        execute_process(bg->child);

        // Prevent the child from returning up the call stack and cloning the shell
        _exit(EXIT_SUCCESS);
    }

    if (debug_mode) {
        printf("[Started background job, PID: %d]\n", pid);
    }
}

static void exec_pipe(node_pipe_t *pipe_node) {
    if (!pipe_node || !pipe_node->left || !pipe_node->right) {
        return;
    }

    int fd[2];

    if (pipe(fd) < 0) {
        fprintf(stderr, "[ERROR] pipe creation failed: %s.\n", strerror(errno));
        return;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        fprintf(stderr, "[ERROR] left fork failed: %s\n", strerror(errno));
        close(fd[0]);
        close(fd[1]);
        return;
    }

    if (left_pid == 0) {
        // Route stdout to pipe write-end
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "[ERROR] left dup2 failure: %s.\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }

        // Close inherited FDs to ensure EOF triggers correctly
        close(fd[0]);
        close(fd[1]);

        execute_process(pipe_node->left);
        _exit(EXIT_SUCCESS);
    }

    pid_t right_pid = fork();

    if (right_pid < 0) {
        fprintf(stderr, "[ERROR] right fork failed: %s\n", strerror(errno));
        close(fd[0]);
        close(fd[1]);
        // Reap the orphaned left child (killed via SIGPIPE) to prevent a zombie process
        waitpid(left_pid, NULL, 0);
        return;
    }

    if (right_pid == 0) {
        // Route stdin from pipe read-end
        if (dup2(fd[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "[ERROR] right dup2 failure: %s.\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        close(fd[0]);
        close(fd[1]);

        execute_process(pipe_node->right);
        _exit(EXIT_SUCCESS);
    }

    // Parent cleanup: close pipe FDs to prevent deadlock
    close(fd[0]);
    close(fd[1]);

    if (waitpid(left_pid, NULL, 0) == -1) {
        fprintf(stderr, "[ERROR] waitpid(left) failed for Child PID %d: %s\n", left_pid, strerror(errno));
    }
    if (waitpid(right_pid, NULL, 0) == -1) {
        fprintf(stderr, "[ERROR] waitpid(right) failed for Child PID %d: %s\n", right_pid, strerror(errno));
    }
}