#include <linux/limits.h>
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

void prompt();

int main(int argc, char **argv) {

    signal(SIGINT, SIG_IGN);

    char buffer[BUFFER_SIZE];

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    while (1) {
        prompt();

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            printf("\n");
            break; // Contract: EOF (Ctrl+D) triggers a graceful exit
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        // Efficiency: Skip empty or whitespace-only strings immediately
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
            add_to_history(buffer);
            print_history();
            continue;
        }

        else if (strcmp(buffer, "!!") == 0) {
            int history_size = get_history_size();

            if (history_size == 0) {
                fprintf(stderr, " [ERROR] History: no commands in history to execute.\n");
                continue;
            }

            const char *command = get_history_command(history_size);

            if (!command) {
                continue;
            }
            printf("%s\n", command);

            strncpy(buffer, command, BUFFER_SIZE - 1);
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        else if (buffer[0] == '!' && buffer[1] != '\0') {
            int n = atoi(buffer + 1);
            int history_size = get_history_size();

            if (n < 1 || n > history_size) {
                fprintf(stderr, "[ERROR] History: index '%d' is out of bounds (1 to %d).\n", n, history_size);
                continue;
            }

            const char *command = get_history_command(n);

            if (!command) {
                continue;
            }

            printf("%s\n", command);

            strncpy(buffer, command, BUFFER_SIZE - 1);
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        // Contract: only valid, expanded commands reach this point, record them
        add_to_history(buffer);

        // 2. ORCHESTRATION: Lexer -> Parser

        token_t *tokens = lexer_tokenize(buffer);

        if (!tokens) {
            fprintf(stderr, "[ERROR] Lexer: memory allocation failed during tokenization.\n");
            continue;
        }

        ast_node_t *ast = parser_build_ast(tokens);
        lexer_free_tokens(tokens); // Contract: free tokens immediately to prevent leaks

        if (!ast) {
            fprintf(stderr, "[ERROR] Parser: syntax error or failed to build AST.\n");
            continue;
        }

        // 3. EXECUTION
        executor_run_ast(ast);

        // 4. CLEANUP
        parser_free_ast(ast); // Contract: parent process always cleans up the AST
    }

    free_process_list();
    free_history();
    return 0;
}

void prompt() {
    char cwd_path[PATH_MAX];

    if (!getcwd(cwd_path, PATH_MAX)) {
        fprintf(stderr, "[ERROR] failed to get the current working directory.\n");
    } else {
        printf("%s #> ", cwd_path);
    }
}
