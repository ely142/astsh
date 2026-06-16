#include "parser.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ast_node_t *parse_job(token_t **curr);
static ast_node_t *parse_pipeline(token_t **curr);
static ast_node_t *parse_command(token_t **curr);

static ast_node_t *create_node(ast_node_type_t type) {
    ast_node_t *new_node = calloc(1, sizeof(ast_node_t));
    if (!new_node) {
        fprintf(stderr, "[ERROR] memory allocation failed in parser.");
        exit(1);
    }
    new_node->type = type;
    return new_node;
}

ast_node_t *parser_build_ast(token_t *tokens) {
    if (!tokens || tokens[0].type == TOKEN_EOF) {
        return NULL;
    }

    token_t *curr = tokens;
    ast_node_t *root = parse_job(&curr);

    if (root != NULL && curr->type != TOKEN_EOF) {
        fprintf(stderr, "[ERROR] unexpected token '%s'\n", curr->value ? curr->value : "metachar");
        // Insert free function here later
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
            return NULL;
        }
        ast_node_t *pipe_node = create_node(NODE_PIPE);
        pipe_node->data.pipe.left = left;
        pipe_node->data.pipe.right = right;
        return pipe_node;
    }
    return left; // If no pipe found, return the single command
}

static ast_node_t *parse_command(token_t **curr) {}