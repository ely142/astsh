#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int builtins_is_command(const char *cmd);
int builtins_execute(ast_node_t *ast);

#endif
