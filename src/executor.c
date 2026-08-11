#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "executor.h"
#include "jobs.h"

extern int debug_mode;

static void execute_process(ast_node_t *node);
static void exec_background(node_background_t *bg);
static void exec_pipe(node_pipe_t *pipe_node);
static void execute_parent_builtin(ast_node_t *root, ast_node_t *core_cmd);

void executor_run_ast(ast_node_t *node) {
    if (!node)
        return;

    // AST Lookahead: peel back redirection nodes to locate the base command
    ast_node_t *core_cmd = node;
    while (core_cmd && core_cmd->type == NODE_REDIRECT) {
        core_cmd = core_cmd->data.redirect.child;
    }

    // Intercept built-ins in the parent process
    if (core_cmd && core_cmd->type == NODE_COMMAND) {
        char **argv = core_cmd->data.command.argv;

        if (argv && argv[0] && builtins_is_command(argv[0])) {
            execute_parent_builtin(node, core_cmd);
            return;
        }
    }

    // Standard execution pipeline (external binaries)
    switch (node->type) {
        // Foreground tasks: fork once to protect the parent shell
        case NODE_COMMAND:
        case NODE_REDIRECT: {
            // Block SIGCHLD so the handler doesn't steal the foreground exit status
            sigset_t mask, prev_mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGCHLD);
            sigprocmask(SIG_BLOCK, &mask, &prev_mask);

            pid_t pid = fork();

            if (pid < 0) {
                fprintf(stderr, "[ERROR] Executor: foreground fork failed - %s\n", strerror(errno));
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                return;
            }

            if (pid == 0) {
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                execute_process(node);
            }

            waitpid(pid, NULL, 0);
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
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
                    signal(SIGINT, SIG_DFL);
                    execvp(argv[0], argv);
                    fprintf(stderr, "[ERROR] Executor: %s - %s\n", argv[0], strerror(errno));
                }
                _exit(EXIT_FAILURE);
            }

            case NODE_REDIRECT: {
                node_redirect_t *redir = &node->data.redirect;

                if (!redir || !redir->child || !redir->file) {
                    fprintf(stderr, "[ERROR] Executor: invalid redirection node execution\n");
                    _exit(EXIT_FAILURE);
                }

                int fd = open(redir->file, redir->open_flags, 0644);

                if (fd < 0) {
                    fprintf(stderr, "[ERROR] Executor: open failed for %s - %s\n", redir->file, strerror(errno));
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, redir->target_fd) == -1) {
                    fprintf(stderr, "[ERROR] Executor: dup2 failure - %s\n", strerror(errno));
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
        fprintf(stderr, "[ERROR] Executor: [PARENT PID %d] process creation failed - %s\n", getpid(), strerror(errno));
        return;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            fprintf(stderr, "[ERROR] Executor: failed to set background process group - %s\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }

        execute_process(bg->child);
        _exit(EXIT_SUCCESS);
    }

    // Defend against race conditions
    setpgid(pid, pid);

    if (debug_mode) {
        printf("[Started background job, PID: %d]\n", pid);
    }

    // Register the job
    const char *cmd_name = "unknown_job";

    if (bg->child->type == NODE_COMMAND && bg->child->data.command.argv[0]) {
        cmd_name = bg->child->data.command.argv[0];
    }

    jobs_add_process(cmd_name, pid);
}

static void exec_pipe(node_pipe_t *pipe_node) {
    if (!pipe_node || !pipe_node->left || !pipe_node->right) {
        return;
    }

    int fd[2];

    if (pipe(fd) < 0) {
        fprintf(stderr, "[ERROR] Executor: pipe creation failed - %s\n", strerror(errno));
        return;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        fprintf(stderr, "[ERROR] Executor: left fork failed - %s\n", strerror(errno));
        close(fd[0]);
        close(fd[1]);
        return;
    }

    if (left_pid == 0) {
        // Route stdout to pipe write-end
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "[ERROR] Executor: left dup2 failure - %s\n", strerror(errno));
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
        fprintf(stderr, "[ERROR] Executor: right fork failed - %s\n", strerror(errno));
        close(fd[0]);
        close(fd[1]);
        // Reap the orphaned left child (killed via SIGPIPE) to prevent a zombie process
        waitpid(left_pid, NULL, 0);
        return;
    }

    if (right_pid == 0) {
        // Route stdin from pipe read-end
        if (dup2(fd[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "[ERROR] Executor: right dup2 failure - %s\n", strerror(errno));
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
        fprintf(stderr, "[ERROR] Executor: waitpid(left) failed for Child PID %d - %s\n", left_pid, strerror(errno));
    }
    if (waitpid(right_pid, NULL, 0) == -1) {
        fprintf(stderr, "[ERROR] Executor: waitpid(right) failed for Child PID %d - %s\n", right_pid, strerror(errno));
    }
}

static void execute_parent_builtin(ast_node_t *root, ast_node_t *core_cmd) {
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);

    if (saved_in < 0 || saved_out < 0 || saved_err < 0) {
        fprintf(stderr, "[ERROR] Executor: failed to backup file descriptors\n");
        return;
    }

    ast_node_t *curr = root;
    while (curr && curr->type == NODE_REDIRECT) {
        node_redirect_t *redir = &curr->data.redirect;

        int fd = open(redir->file, redir->open_flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "[ERROR] Executor: open failed for %s - %s\n", redir->file, strerror(errno));
            goto restore_fds;
        }

        if (dup2(fd, redir->target_fd) == -1) {
            fprintf(stderr, "[ERROR] Executor: dup2 failure - %s\n", strerror(errno));
            close(fd);
            goto restore_fds;
        }
        close(fd);

        curr = redir->child;
    }

    builtins_execute(core_cmd);

restore_fds:
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);

    close(saved_in);
    close(saved_out);
    close(saved_err);
}