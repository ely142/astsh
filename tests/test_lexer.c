#include "lexer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("\033[0;32m[PASS]\033[0m %s\n", name)
#define TEST_FAIL(name) printf("\033[0;31m[FAIL]\033[0m %s\n", name)

void test_simple_command() {
    token_t *tokens = lexer_tokenize("ls -l");

    assert(tokens != NULL);
    assert(tokens[0].type == TOKEN_WORD);
    assert(strcmp(tokens[0].value, "ls") == 0);

    assert(tokens[1].type == TOKEN_WORD);
    assert(strcmp(tokens[1].value, "-l") == 0);

    assert(tokens[2].type == TOKEN_EOF);

    lexer_print_tokens(tokens);
    lexer_free_tokens(tokens);
    TEST_PASS("test_simple_command");
}

void test_pipeline_and_redirects() {
    token_t *tokens = lexer_tokenize("cat < in.txt | grep out > final.txt");

    assert(tokens[0].type == TOKEN_WORD);      // cat
    assert(tokens[1].type == TOKEN_REDIR_IN);  // <
    assert(tokens[2].type == TOKEN_WORD);      // in.txt
    assert(tokens[3].type == TOKEN_PIPE);      // |
    assert(tokens[4].type == TOKEN_WORD);      // grep
    assert(tokens[5].type == TOKEN_WORD);      // out
    assert(tokens[6].type == TOKEN_REDIR_OUT); // >
    assert(tokens[7].type == TOKEN_WORD);      // final.txt
    assert(tokens[8].type == TOKEN_EOF);

    lexer_print_tokens(tokens);
    lexer_free_tokens(tokens);
    TEST_PASS("test_pipeline_and_redirects");
}

void test_empty_quotes() {
    token_t *tokens = lexer_tokenize("echo \"\"");

    assert(tokens[0].type == TOKEN_WORD);
    assert(strcmp(tokens[0].value, "echo") == 0);

    assert(tokens[1].type == TOKEN_WORD);
    assert(strcmp(tokens[1].value, "") == 0);

    assert(tokens[2].type == TOKEN_EOF);

    lexer_print_tokens(tokens);
    lexer_free_tokens(tokens);
    TEST_PASS("test_empty_quotes");
}

void test_unclosed_quotes() {
    token_t *tokens = lexer_tokenize("echo \"whoops");

    assert(tokens == NULL);

    TEST_PASS("test_unclosed_quotes");
}

int main() {
    printf("========================\n");
    printf("   RUNNING LEXER TESTS  \n");
    printf("========================\n");

    test_simple_command();
    test_pipeline_and_redirects();
    test_empty_quotes();
    test_unclosed_quotes();

    printf("========================\n");
    printf("\033[0;32mALL TESTS PASSED!\033[0m\n");
    printf("========================\n");

    return 0;
}