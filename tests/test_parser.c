#include "lexer.h"
#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("\033[0;32m[PASS]\033[0m %s\n", name)
#define TEST_FAIL(name) printf("\033[0;31m[FAIL]\033[0m %s\n", name)

void test_simple_command() {
    token_t *tokens = lexer_tokenize("ls -l -a");
    assert(tokens != NULL);

    ast_node_t *root = parser_build_ast(tokens);
    assert(root != NULL);

    assert(root->type == NODE_COMMAND);

    assert(strcmp(root->data.command.argv[0], "ls") == 0);
    assert(strcmp(root->data.command.argv[1], "-l") == 0);
    assert(strcmp(root->data.command.argv[2], "-a") == 0);
    assert(root->data.command.argv[3] == NULL);

    parser_print_ast(root, 0);
    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_simple_command");
}

void test_pipeline() {
    token_t *tokens = lexer_tokenize("cat in.txt | grep hello");
    ast_node_t *root = parser_build_ast(tokens);
    assert(root != NULL);

    assert(root->type == NODE_PIPE);

    ast_node_t *left = root->data.pipe.left;
    assert(left != NULL && left->type == NODE_COMMAND);
    assert(strcmp(left->data.command.argv[0], "cat") == 0);
    assert(strcmp(left->data.command.argv[1], "in.txt") == 0);
    assert(left->data.command.argv[2] == NULL);

    ast_node_t *right = root->data.pipe.right;
    assert(right != NULL && right->type == NODE_COMMAND);
    assert(strcmp(right->data.command.argv[0], "grep") == 0);
    assert(strcmp(right->data.command.argv[1], "hello") == 0);
    assert(right->data.command.argv[2] == NULL);

    parser_print_ast(root, 0);
    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_pipeline");
}

void test_redirection_wrapper() {
    token_t *tokens = lexer_tokenize("echo test > out.txt");
    ast_node_t *root = parser_build_ast(tokens);
    assert(root != NULL);

    assert(root->type == NODE_REDIRECT);
    assert(strcmp(root->data.redirect.file, "out.txt") == 0);
    assert(root->data.redirect.target_fd == 1);

    ast_node_t *child = root->data.redirect.child;
    assert(child != NULL && child->type == NODE_COMMAND);
    assert(strcmp(child->data.command.argv[0], "echo") == 0);
    assert(strcmp(child->data.command.argv[1], "test") == 0);
    assert(child->data.command.argv[2] == NULL);

    parser_print_ast(root, 0);
    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_redirection_wrapper");
}

void test_syntax_errors() {
    // Test 1: Missing filename after redirection
    token_t *tokens1 = lexer_tokenize("ls >");
    ast_node_t *root1 = parser_build_ast(tokens1);
    assert(root1 == NULL);
    lexer_free_tokens(tokens1);

    // Test 2: Double pipe / missing command
    token_t *tokens2 = lexer_tokenize("ls | | grep");
    ast_node_t *root2 = parser_build_ast(tokens2);
    assert(root2 == NULL);
    lexer_free_tokens(tokens2);

    TEST_PASS("test_syntax_errors");
}

void test_background_job() {
    token_t *tokens = lexer_tokenize("sleep 10 &");
    ast_node_t *root = parser_build_ast(tokens);
    assert(root != NULL);

    assert(root->type == NODE_BACKGROUND);

    ast_node_t *child = root->data.background.child;
    assert(child != NULL && child->type == NODE_COMMAND);
    assert(strcmp(child->data.command.argv[0], "sleep") == 0);
    assert(strcmp(child->data.command.argv[1], "10") == 0);
    assert(child->data.command.argv[2] == NULL);

    parser_print_ast(root, 0);
    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_background_job");
}

void test_complex_background_pipeline() {
    token_t *tokens = lexer_tokenize("cat file.txt | grep \"hello\" > output.txt &");
    ast_node_t *root = parser_build_ast(tokens);
    assert(root != NULL);

    assert(root->type == NODE_BACKGROUND);

    ast_node_t *pipe = root->data.background.child;
    assert(pipe != NULL && pipe->type == NODE_PIPE);

    ast_node_t *left = pipe->data.pipe.left;
    assert(left != NULL && left->type == NODE_COMMAND);
    assert(strcmp(left->data.command.argv[0], "cat") == 0);
    assert(strcmp(left->data.command.argv[1], "file.txt") == 0);
    assert(left->data.command.argv[2] == NULL);

    ast_node_t *right_redir = pipe->data.pipe.right;
    assert(right_redir != NULL && right_redir->type == NODE_REDIRECT);
    assert(strcmp(right_redir->data.redirect.file, "output.txt") == 0);
    assert(right_redir->data.redirect.target_fd == 1);

    ast_node_t *right_cmd = right_redir->data.redirect.child;
    assert(right_cmd != NULL && right_cmd->type == NODE_COMMAND);
    assert(strcmp(right_cmd->data.command.argv[0], "grep") == 0);
    assert(strcmp(right_cmd->data.command.argv[1], "hello") == 0);
    assert(right_cmd->data.command.argv[2] == NULL);

    parser_print_ast(root, 0);
    parser_free_ast(root);
    lexer_free_tokens(tokens);

    TEST_PASS("test_complex_background_pipeline");
}

int main() {
    printf("========================\n");
    printf("   RUNNING PARSER TESTS \n");
    printf("========================\n");

    test_simple_command();
    test_pipeline();
    test_redirection_wrapper();
    test_syntax_errors();
    test_background_job();
    test_complex_background_pipeline();

    printf("========================\n");
    printf("\033[0;32mALL AST TESTS PASSED!\033[0m\n");
    printf("========================\n");

    return 0;
}