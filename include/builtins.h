#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int builtin_is_command(const char *cmd);
int builtin_execute(ast_node_t *ast);

#endif
