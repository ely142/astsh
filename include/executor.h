#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "line_parser.h"
#include "parser.h"

/* =========================================================
 * LEGACY CODE (Temporarily disabled for AST testing)
 * ========================================================= */
#if 0

void execute(cmd_line *cmd);
int handle_pipe(cmd_line *cmd);

#endif

// Public entry point: evaluates the AST while shielding the parent shell process
void executor_run_ast(ast_node_t *node);

#endif