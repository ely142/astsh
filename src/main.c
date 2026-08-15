#define _GNU_SOURCE

#include <errno.h>
#include <linux/limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Core Pipeline
#include "executor.h"
#include "lexer.h"
#include "parser.h"

// Utilities
#include "history.h"
#include "jobs.h"

#define BUFFER_SIZE 2048

int debug_mode = 0;

#define C_RESET  "\033[0m"
#define C_ACCENT "\033[1;30m" // Bold Dark Gray
#define C_USER   "\033[1;36m" // Bold Cyan
#define C_HOST   "\033[1;34m" // Bold Blue
#define C_PATH   "\033[1;33m" // Bold Yellow
#define C_PROMPT "\033[1;32m" // Bold Green

void prompt();

int main(int argc, char **argv) {

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    char buffer[BUFFER_SIZE];

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    // Register asynchronous child reaping
    struct sigaction sa;
    sa.sa_handler = jobs_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        fprintf(stderr, "[ERROR] Shell: failed to bind SIGCHLD handler\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        prompt();

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            if (errno == EINTR) {
                clearerr(stdin);
                printf("\n");
                continue;
            }

            // Strict EOF validation: only exit if the user actually pressed Ctrl+D
            if (feof(stdin)) {
                printf("\n");
                break;
            }

            // Defend against EIO: terminal rejected the read (process group conflict)
            clearerr(stdin);
            continue;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        // Efficiency: skip empty or whitespace-only strings immediately
        int is_empty = 1;
        for (int i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] != ' ' && buffer[i] != '\t') {
                is_empty = 0;
                break;
            }
        }

        if (is_empty) {
            continue;
        }

        if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "exit") == 0) {
            break;
        }

        // 1. RAW STRING INTERCEPTION (History Expansion)

        if (strcmp(buffer, "hist") == 0) {
            history_print();
            continue;
        }

        else if (strcmp(buffer, "!!") == 0) {
            int history_size = history_get_size();

            if (history_size == 0) {
                fprintf(stderr, " [ERROR] History: no commands in history to execute\n");
                continue;
            }

            const char *command = history_get(history_size);

            if (!command) {
                continue;
            }
            printf("%s\n", command);

            strncpy(buffer, command, BUFFER_SIZE - 1);
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        else if (buffer[0] == '!' && buffer[1] != '\0') {
            int n = atoi(buffer + 1);
            int history_size = history_get_size();

            if (n < 1 || n > history_size) {
                fprintf(stderr, "[ERROR] History: index '%d' is out of bounds (1 to %d)\n", n, history_size);
                continue;
            }

            const char *command = history_get(n);

            if (!command) {
                continue;
            }

            printf("%s\n", command);

            strncpy(buffer, command, BUFFER_SIZE - 1);
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        // Contract: only valid, expanded commands reach this point, record them
        history_add(buffer);

        // 2. ORCHESTRATION: Lexer -> Parser

        token_t *tokens = lexer_tokenize(buffer);

        if (!tokens) {
            fprintf(stderr, "[ERROR] Lexer: memory allocation failed during tokenization\n");
            continue;
        }

        ast_node_t *ast = parser_build_ast(tokens);
        lexer_free_tokens(tokens); // Contract: free tokens immediately to prevent leaks

        if (!ast) {
            fprintf(stderr, "[ERROR] Parser: syntax error or failed to build AST\n");
            continue;
        }

        // 3. EXECUTION
        executor_run_ast(ast);

        // 4. CLEANUP
        parser_free_ast(ast); // Contract: parent process always cleans up the AST
    }

    jobs_free();
    history_free();
    return 0;
}

void prompt(void) {
    char cwd[PATH_MAX];
    char host[256];

    if (!getcwd(cwd, PATH_MAX)) {
        strncpy(cwd, "unknown", PATH_MAX);
        cwd[PATH_MAX - 1] = '\0';
    }

    if (gethostname(host, sizeof(host)) != 0) {
        strncpy(host, "local", sizeof(host));
        host[sizeof(host) - 1] = '\0';
    }

    const char *user = getenv("USER");
    if (!user) {
        user = "user";
    }

    // Truncate home directory to '~' for visual brevity
    const char *home = getenv("HOME");
    size_t home_len = home ? strlen(home) : 0;
    char display_cwd[PATH_MAX];

    if (home && home_len > 0 && strncmp(cwd, home, home_len) == 0 && (cwd[home_len] == '/' || cwd[home_len] == '\0')) {
        snprintf(display_cwd, sizeof(display_cwd), "~%s", cwd + home_len);
    } else {
        strncpy(display_cwd, cwd, PATH_MAX);
        display_cwd[PATH_MAX - 1] = '\0';
    }

    // Context Dashboard
    // Format: ╭─[user@host]─[~/current/path]
    printf("%s╭─[%s%s%s@%s%s%s]%s─[%s%s%s]\n", C_ACCENT, C_USER, user, C_ACCENT, C_HOST, host, C_ACCENT, C_ACCENT,
           C_PATH, display_cwd, C_ACCENT);

    // Input Field
    // Format: ╰─❯
    printf("%s╰─%s❯%s ", C_ACCENT, C_PROMPT, C_RESET);

    printf("\033]0;%s@%s: %s\007", user, host, display_cwd);
    fflush(stdout);
}