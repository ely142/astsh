#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_BUFFER_SIZE        1024
#define TOKENS_INITIAL_CAPACITY 16

typedef enum { STATE_IN_QUOTES, STATE_NORMAL } lexer_state_t;

static void append_token(token_t **tokens, int *tokens_capacity, int *tokens_size, token_type_t type, char *value) {
    if (*tokens_size >= *tokens_capacity) {
        *tokens_capacity *= 2;
        token_t *temp = realloc(*tokens, (*tokens_capacity) * sizeof(token_t));
        if (!temp) {
            fprintf(stderr, "[ERROR] Lexer: memory reallocation failed in lexer\n");
            exit(1);
        }
        *tokens = temp;
    }
    (*tokens)[*tokens_size].type = type;
    (*tokens)[*tokens_size].value = value;
    (*tokens_size)++;
}

static void finalize_word_buffer(token_t **tokens, int *tokens_capacity, int *tokens_size, char *buffer, int *idx,
                                 bool *is_quoted) {
    buffer[*idx] = '\0';
    char *temp_word = strdup(buffer);
    if (!temp_word) {
        fprintf(stderr, "[ERROR] Lexer: memory allocation failed for string duplication in lexer\n");
        exit(1);
    }
    append_token(tokens, tokens_capacity, tokens_size, TOKEN_WORD, temp_word);

    *idx = 0;
    *is_quoted = false;
}

static const char *token_type_to_string(token_type_t type) {
    switch (type) {
        case TOKEN_WORD:
            return "WORD";
        case TOKEN_PIPE:
            return "PIPE";
        case TOKEN_REDIR_IN:
            return "REDIR_IN (<)";
        case TOKEN_REDIR_OUT:
            return "REDIR_OUT (>)";
        case TOKEN_AMPERSAND:
            return "AMPERSAND (&)";
        case TOKEN_EOF:
            return "EOF";
        default:
            return "UNKNOWN";
    }
}

token_t *lexer_tokenize(const char *input) {
    int tokens_size = 0;
    int tokens_capacity = TOKENS_INITIAL_CAPACITY;
    token_t *tokens = malloc(tokens_capacity * sizeof(token_t));
    bool is_quoted_token = false;

    if (!tokens) {
        fprintf(stderr, "[ERROR] Lexer: memory allocation failed in lexer\n");
        exit(1);
    }

    char word_buffer[WORD_BUFFER_SIZE];
    int word_idx = 0;

    lexer_state_t state = STATE_NORMAL;

    for (int i = 0; input[i] != '\0'; i++) {
        char ch = input[i];

        switch (state) {
            case STATE_NORMAL:
                if (ch == '"') {
                    state = STATE_IN_QUOTES;
                    is_quoted_token = true;
                } else if (ch == ' ' || ch == '\t' || ch == '\n') {
                    if (word_idx > 0 || is_quoted_token) {
                        finalize_word_buffer(&tokens, &tokens_capacity, &tokens_size, word_buffer, &word_idx,
                                             &is_quoted_token);
                    }
                } else if (ch == '|' || ch == '<' || ch == '>' || ch == '&') {
                    if (word_idx > 0 || is_quoted_token) { // For cases like: "grep> out.txt"
                        finalize_word_buffer(&tokens, &tokens_capacity, &tokens_size, word_buffer, &word_idx,
                                             &is_quoted_token);
                    }
                    if (ch == '|')
                        append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_PIPE, NULL);
                    else if (ch == '<')
                        append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_REDIR_IN, NULL);
                    else if (ch == '>')
                        append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_REDIR_OUT, NULL);
                    else if (ch == '&')
                        append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_AMPERSAND, NULL);
                } else {
                    if (word_idx >= WORD_BUFFER_SIZE - 1) {
                        goto handle_overflow;
                    }
                    word_buffer[word_idx++] = ch;
                }
                break;

            case STATE_IN_QUOTES:
                if (ch == '"') {
                    state = STATE_NORMAL;
                } else {
                    if (word_idx >= WORD_BUFFER_SIZE - 1) {
                        goto handle_overflow;
                    }
                    word_buffer[word_idx++] = ch;
                }
                break;
        }
    }

    if (state == STATE_IN_QUOTES) {
        fprintf(stderr, "[ERROR] Lexer: missing closing quote on input command\n");
        append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_EOF, NULL);
        lexer_free_tokens(tokens);
        return NULL;
    }

    // Cleanup any lingering word at the end of the string not followed by a space afterwards
    if (word_idx > 0 || is_quoted_token) {
        finalize_word_buffer(&tokens, &tokens_capacity, &tokens_size, word_buffer, &word_idx, &is_quoted_token);
    }

    // Add the EOF token so the parser knows when to stop
    append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_EOF, NULL);

    return tokens;

handle_overflow:
    fprintf(stderr, "[ERROR] Lexer: token exceeds maximum length of %d characters\n", WORD_BUFFER_SIZE);
    append_token(&tokens, &tokens_capacity, &tokens_size, TOKEN_EOF, NULL);
    lexer_free_tokens(tokens);
    return NULL;
}

void lexer_free_tokens(token_t *tokens) {
    if (!tokens) {
        return;
    }

    token_t *curr = tokens;
    while (curr->type != TOKEN_EOF) {
        if (curr->value) {
            free(curr->value);
        }
        curr++;
    }
    free(tokens);
}

void lexer_print_tokens(token_t *tokens) {
    if (!tokens)
        return;

    int i = 0;
    for (; tokens[i].type != TOKEN_EOF; i++) {
        printf("Token [%02d] | Type: %-15s", i, token_type_to_string(tokens[i].type));

        if (tokens[i].value != NULL) {
            printf(" | Value: \"%s\"", tokens[i].value);
        }
        printf("\n");
    }

    printf("Token [%02d] | Type: %-15s\n", i, token_type_to_string(TOKEN_EOF));
}