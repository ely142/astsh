/**
 * @file line_parser.h
 * @brief Command line parsing module for shell-like applications.
 *
 * Provides structures and functions to parse raw string inputs into
 * a linked list of commands, handling arguments, pipes, and redirection.
 */

#ifndef LINE_PARSER_H
#define LINE_PARSER_H

#include <stdbool.h>

#define LINE_PARSER_MAX_ARGS 256

typedef struct cmd_line cmd_line;

/**
 * @brief Represents a single parsed command or a node in a pipeline.
 * @note This struct owns its memory. The strings contained within must be freed using
 * line_parser_free().
 *
 */
struct cmd_line {
    char *arguments[LINE_PARSER_MAX_ARGS]; /**< Command line arguments (arg 0 is the command).
                                                    NULL-terminated. */
    int arg_count;                         /**< Total number of arguments. */
    char *input_redirect;                  /**< Path for input redirection, NULL if none. */
    char *output_redirect;                 /**< Path for output redirection, NULL if none. */
    bool is_blocking;                      /**< true if foreground (blocking), false if background (&). */
    int pipe_index;                        /**< Pipeline position index (0 for the first command). */
    cmd_line *next;                        /**< Pointer to the next command in the pipeline, NULL if last. */
};

/**
 * @brief Parses a raw input string into a structured command pipeline.
 *
 * Takes a raw user string, splits it by pipes ('|'), handles input/output
 * redirection, and determines if the command should run in the background.
 *
 * @param unparsed_input The raw, unparsed command string from the user.
 * @return cmd_line* Pointer to the head of the parsed linked list, or NULL if the input was
 * empty/invalid.
 */
cmd_line *line_parser_parse(const char *unparsed_input);

/**
 * @brief Safely releases all dynamically allocated memory for a pipeline.
 *
 * Recursively iterates through the linked list and frees all duplicated
 * strings, arguments, and command structures.
 *
 * @param pipeline_head Pointer to the head of the command line chain to free.
 */
void line_parser_free(cmd_line *pipeline_head);

/**
 * @brief Replaces an argument at a specific index with a new string.
 *
 * Safely frees the old argument memory at the given index and duplicates
 * the new string into its place.
 *
 * @param command Pointer to the target command structure.
 * @param target_index The integer index of the argument array to replace.
 * @param new_string The replacement string.
 * @return true if the replacement was successful.
 * @return false if target_index is out of bounds.
 */
bool line_parser_replace_arg(cmd_line *command, int target_index, const char *new_string);

#endif