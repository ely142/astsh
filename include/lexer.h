#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_WORD,      // "ls", "-l", "my folder"
    TOKEN_PIPE,      // |
    TOKEN_REDIR_IN,  // <
    TOKEN_REDIR_OUT, // >
    TOKEN_AMPERSAND, // &
    TOKEN_EOF        // Array's end indicator
} token_type_t;

typedef struct {
    token_type_t type;
    char *value; // NULL for pipes/ampersands
} token_t;

token_t *lexer_tokenize(const char *input); // Returns a dynamically sized array of tokens
void lexer_free_tokens(token_t *tokens);
void lexer_print_tokens(token_t *tokens);

#endif