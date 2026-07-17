#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

// Public entry point: evaluates the AST while shielding the parent shell process
void executor_run_ast(ast_node_t *node);

#endif