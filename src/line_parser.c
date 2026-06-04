#include "line_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isEmpty(const char *str) {
    if (!str) {
        return true;
    }

    while (*str) {
        if (!isspace((unsigned char)*(str++))) {
            return false;
        }
    }

    return true;
}

static char *clone_first_word(char *str) {
    char *start = NULL;
    char *end = NULL;
    char *word;

    while (!end) {
        switch (*str) {
            case '>':
            case '<':
            case 0:
                end = str - 1;
                break;
            case ' ':
                if (start)
                    end = str - 1;
                break;
            default:
                if (!start)
                    start = str;
                break;
        }
        str++;
    }

    if (start == NULL)
        return NULL;

    word = (char *)malloc(end - start + 2);
    strncpy(word, start, ((int)(end - start) + 1));
    word[(int)((end - start) + 1)] = 0;

    return word;
}

static void extract_redirections(char *line, cmd_line *cmd) {
    // TODO: This function currently uses destructive parsing (*s++ = 0).
    // causes a "Lost Argument" bug if flags are typed AFTER the redirection
    // 'cat < in.txt -l' drops '-l'
    // needs to be refactored to shift the string memory left instead of null-terminating.
    char *s = line;

    while ((s = strpbrk(s, "<>"))) {
        if (*s == '<') {
            free(cmd->input_redirect);
            cmd->input_redirect = clone_first_word(s + 1);

            if (!cmd->input_redirect) {
                fprintf(stderr, "Syntax error: expected file after '<'\n");
            }
        } else {
            free(cmd->output_redirect);
            cmd->output_redirect = clone_first_word(s + 1);

            if (!cmd->output_redirect) {
                fprintf(stderr, "Syntax error: expected file after '>'\n");
            }
        }

        *s++ = '\0';
    }
}

static cmd_line *parse_single_command(const char *raw_cmd) {
    if (isEmpty(raw_cmd)) {
        return NULL;
    }

    cmd_line *cmd = calloc(1, sizeof(cmd_line));
    if (!cmd) {
        perror("Failed to allocate command structure.");
        return NULL;
    }

    char *line = strdup(raw_cmd);
    if (!line) {
        perror("Failed to duplicate command string.");
        free(cmd);
        return NULL;
    }

    extract_redirections(line, cmd);

    char *token = strtok(line, " ");

    while (token && cmd->arg_count < LINE_PARSER_MAX_ARGS - 1) {
        cmd->arguments[cmd->arg_count] = strdup(token);

        if (!cmd->arguments[cmd->arg_count]) {
            perror("Failed to duplicate argument token.");
            line_parser_free(cmd);
            free(line);
            return NULL;
        }

        cmd->arg_count++;
        token = strtok(NULL, " ");
    }

    free(line);
    return cmd;
}

static cmd_line *parse_pipeline_nodes(char *line) {
    if (isEmpty(line)) {
        return NULL;
    }

    char *pipe_ptr;
    cmd_line *current_cmd;

    pipe_ptr = strchr(line, '|');
    if (pipe_ptr)
        *pipe_ptr = '\0';

    current_cmd = parse_single_command(line);
    if (!current_cmd)
        return NULL;

    if (pipe_ptr)
        current_cmd->next = parse_pipeline_nodes(pipe_ptr + 1);

    return current_cmd;
}

cmd_line *line_parser_parse(const char *unparsed_input) { // Destructive parsing method
    if (isEmpty(unparsed_input)) {
        return NULL;
    }

    char *line, *ampersand;

    line = strdup(unparsed_input);
    if (!line) {
        perror("Memory allocation failed during parsing.");
        return NULL;
    }

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    ampersand = strchr(line, '&');
    if (ampersand) {
        *(ampersand) = '\0';
    }

    cmd_line *head = parse_pipeline_nodes(line);

    if (head) {
        cmd_line *current = head;
        int idx = 0;

        while (current) {
            current->pipe_index = idx++;

            if (!current->next) {
                current->is_blocking = (ampersand == NULL);
            } else {
                current->is_blocking = true;
            }
            current = current->next;
        }
    }

    free(line);
    return head;
}

void line_parser_free(cmd_line *pipeline_head) {
    if (!pipeline_head)
        return;

    free(pipeline_head->input_redirect);
    free(pipeline_head->output_redirect);
    for (int i = 0; i < pipeline_head->arg_count; ++i)
        free(pipeline_head->arguments[i]);

    if (pipeline_head->next)
        line_parser_free(pipeline_head->next);

    free(pipeline_head);
}

bool line_parser_replace_arg(cmd_line *command, int target_index, const char *new_string) {
    if (target_index >= command->arg_count)
        return false;

    char *new_arg = strdup(new_string);

    if (!new_arg) {
        perror("Failed to allocate memory for new argument.");
        return false;
    }

    free(command->arguments[target_index]);
    command->arguments[target_index] = new_arg;

    return true;
}