#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum { NODE_COMMAND, NODE_PIPE, NODE_REDIRECT, NODE_BACKGROUND } ast_node_type_t;

typedef struct ast_node ast_node_t;

typedef struct {
    char **argv; // Null-terminated array of arguments ["ls", "-l", NULL]
} node_command_t;

typedef struct {
    ast_node_t *left;
    ast_node_t *right;
} node_pipe_t;

typedef struct {
    ast_node_t *child; // The command or pipeline being redirected
    char *file;        // Target filename
    int target_fd;     // 0 for STDIN ('<'), 1 for STDOUT ('>')
    int open_flags;    // System flags for open() (O_CREAT | O_WRONLY | O_TRUNC etc)
} node_redirect_t;

typedef struct {
    ast_node_t *child;
} node_background_t;

struct ast_node {
    ast_node_type_t type;
    union {
        node_command_t command;
        node_pipe_t pipe;
        node_redirect_t redirect;
        node_background_t background;
    } data;
};

ast_node_t *parser_build_ast(token_t *tokens);
void parser_free_ast(ast_node_t *root);
void parser_print_ast(ast_node_t *root, int level);

#endif