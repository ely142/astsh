#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "line_parser.h"
#include "parser.h"

void execute(cmd_line *cmd);
int handle_pipe(cmd_line *cmd);

// Walk the AST and executes the nodes recursively
void execute_ast(ast_node_t *node);

#endif