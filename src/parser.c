#include "parser.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_BUFFER_SIZE 1024

static ast_node_t *parse_job(token_t **curr);
static ast_node_t *parse_pipeline(token_t **curr);
static ast_node_t *parse_command(token_t **curr);

static ast_node_t *create_node(ast_node_type_t type) {
    ast_node_t *new_node = calloc(1, sizeof(ast_node_t));
    if (!new_node) {
        fprintf(stderr, "[ERROR] memory allocation failed in parser.\n");
        exit(1);
    }
    new_node->type = type;
    return new_node;
}

void parser_free_ast(ast_node_t *root) {
    if (!root)
        return;

    switch (root->type) {
        case NODE_COMMAND:
            if (root->data.command.argv) {
                for (int i = 0; root->data.command.argv[i] != NULL; i++) {
                    free(root->data.command.argv[i]);
                }
                free(root->data.command.argv);
            }
            break;

        case NODE_PIPE:
            parser_free_ast(root->data.pipe.left);
            parser_free_ast(root->data.pipe.right);
            break;

        case NODE_REDIRECT:
            free(root->data.redirect.file);
            parser_free_ast(root->data.redirect.child);
            break;

        case NODE_BACKGROUND:
            parser_free_ast(root->data.background.child);
            break;
    }
    free(root);
}

ast_node_t *parser_build_ast(token_t *tokens) {
    if (!tokens || tokens[0].type == TOKEN_EOF) {
        return NULL;
    }

    token_t *curr = tokens;
    ast_node_t *root = parse_job(&curr);

    if (root != NULL && curr->type != TOKEN_EOF) {
        fprintf(stderr, "[ERROR] unexpected token '%s'.\n", curr->value ? curr->value : "metachar");
        parser_free_ast(root);
        return NULL;
    }
    return root;
}

static ast_node_t *parse_job(token_t **curr) {
    ast_node_t *child = parse_pipeline(curr);
    if (!child) {
        return NULL;
    }
    if ((*curr)->type == TOKEN_AMPERSAND) {
        (*curr)++;
        ast_node_t *bg_node = create_node(NODE_BACKGROUND);
        bg_node->data.background.child = child;
        return bg_node;
    }
    return child;
}

static ast_node_t *parse_pipeline(token_t **curr) {
    ast_node_t *left = parse_command(curr);
    if (!left) {
        return NULL;
    }
    if ((*curr)->type == TOKEN_PIPE) {
        (*curr)++;

        ast_node_t *right = parse_pipeline(curr);
        if (!right) {
            parser_free_ast(left);
            return NULL;
        }
        ast_node_t *pipe_node = create_node(NODE_PIPE);
        pipe_node->data.pipe.left = left;
        pipe_node->data.pipe.right = right;
        return pipe_node;
    }
    return left; // If no pipe found, return the single command
}

static ast_node_t *parse_command(token_t **curr) {
    ast_node_t *cmd_node = create_node(NODE_COMMAND);

    int argv_capacity = 8;
    int argc = 0;
    cmd_node->data.command.argv = calloc(argv_capacity, sizeof(char *));
    if (!cmd_node->data.command.argv) {
        fprintf(stderr, "[ERROR] memory allocation failed in parser.\n");
        exit(1);
    }

    ast_node_t *root = cmd_node;

    while ((*curr)->type == TOKEN_WORD || (*curr)->type == TOKEN_REDIR_IN || (*curr)->type == TOKEN_REDIR_OUT) {
        if ((*curr)->type == TOKEN_WORD) {
            if (argc >= argv_capacity - 1) {
                argv_capacity *= 2;
                char **temp_argv = realloc(cmd_node->data.command.argv, argv_capacity * sizeof(char *));
                if (!temp_argv) {
                    fprintf(stderr, "[ERROR] memory reallocation failed in parser.\n");
                    exit(1);
                }
                cmd_node->data.command.argv = temp_argv;
            }
            char *temp_word = strdup((*curr)->value);
            if (!temp_word) {
                fprintf(stderr, "[ERROR] memory allocation failed for string duplication in parser.\n");
                exit(1);
            }
            cmd_node->data.command.argv[argc++] = temp_word;
            (*curr)++;

        } else if ((*curr)->type == TOKEN_REDIR_IN || (*curr)->type == TOKEN_REDIR_OUT) {
            token_type_t redir_type = (*curr)->type;
            (*curr)++;

            if ((*curr)->type != TOKEN_WORD) {
                fprintf(stderr, "[ERROR] expected filename after redirection.\n");
                // Cap the array before freeing so parser_free_ast knows where to stop
                cmd_node->data.command.argv[argc] = NULL;
                parser_free_ast(root);
                return NULL;
            }

            ast_node_t *redir_node = create_node(NODE_REDIRECT);
            redir_node->data.redirect.child = root;
            char *temp_word = strdup((*curr)->value);
            if (!temp_word) {
                fprintf(stderr, "[ERROR] memory allocation failed for string duplication in parser.\n");
                exit(1);
            }
            redir_node->data.redirect.file = temp_word;

            if (redir_type == TOKEN_REDIR_IN) {
                redir_node->data.redirect.target_fd = 0; // STDIN
                redir_node->data.redirect.open_flags = O_RDONLY;
            } else {
                redir_node->data.redirect.target_fd = 1; // STDOUT
                redir_node->data.redirect.open_flags = O_WRONLY | O_CREAT | O_TRUNC;
            }

            root = redir_node;
            (*curr)++;
        }
    }

    // Cap the array to hide any garbage left by realloc
    cmd_node->data.command.argv[argc] = NULL;

    // For cases like: "| grep" - syntax error, "> test.txt" - isn't an error
    if (argc == 0 && root == cmd_node) {
        fprintf(stderr, "[ERROR] invalid command syntax.\n");
        parser_free_ast(root);
        return NULL;
    }

    return root;
}

void parser_print_ast(ast_node_t *root, int level) {
    if (!root) {
        return;
    }

    for (int i = 0; i < level; i++) {
        printf("  |   ");
    }

    if (level > 0) {
        printf("+-- ");
    }

    switch (root->type) {
        case NODE_COMMAND:
            printf("[COMMAND] ");
            if (root->data.command.argv) {
                for (int i = 0; root->data.command.argv[i] != NULL; i++) {
                    printf("\"%s\" ", root->data.command.argv[i]);
                }
            }
            printf("\n");
            break;

        case NODE_PIPE:
            printf("[PIPE]\n");
            parser_print_ast(root->data.pipe.left, level + 1);
            parser_print_ast(root->data.pipe.right, level + 1);
            break;

        case NODE_REDIRECT:
            printf("[REDIRECT] fd: %d -> file: \"%s\"\n", root->data.redirect.target_fd, root->data.redirect.file);
            parser_print_ast(root->data.redirect.child, level + 1);
            break;

        case NODE_BACKGROUND:
            printf("[BACKGROUND] (&)\n");
            parser_print_ast(root->data.background.child, level + 1);
            break;

        default:
            printf("[UNKNOWN NODE]\n");
            break;
    }
}