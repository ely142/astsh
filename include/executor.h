#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "line_parser.h"

void execute(cmd_line *cmd);
int handle_pipe(cmd_line *cmd);

#endif