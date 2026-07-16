#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"
#include "lexer.h"
#include "parser.h"

#define TEST_PASS(name) printf("\033[0;32m[PASS]\033[0m %s\n", name)
#define TEST_FAIL(name) printf("\033[0;31m[FAIL]\033[0m %s\n", name)

// Define the global variable expected by executor.c so the linker succeeds
int debug_mode = 0;

static void assert_file_content(const char *filename, const char *expected) {
    int fd = open(filename, O_RDONLY);
    assert(fd >= 0 && "Executor failed to create the target output file.");

    char buffer[256] = {0};
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    assert(bytes_read >= 0 && "Failed to read output file.");
    assert(strcmp(buffer, expected) == 0 && "Command output did not match expected result.");

    unlink(filename); // Clean up the test file
}

void test_simple_command_redirect() {
    token_t *tokens = lexer_tokenize("echo \"hello executor\" > test_out.txt");
    ast_node_t *root = parser_build_ast(tokens);

    executor_run_ast(root);

    // echo automatically appends a newline
    assert_file_content("test_out.txt", "hello executor\n");

    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_simple_command_redirect");
}

void test_pipe_execution() {
    token_t *tokens = lexer_tokenize("echo \"find me\" | grep \"find\" > test_out.txt");
    ast_node_t *root = parser_build_ast(tokens);

    executor_run_ast(root);

    assert_file_content("test_out.txt", "find me\n");

    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_pipe_execution");
}

void test_input_redirection() {
    int in_fd = open("test_in.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    write(in_fd, "input data", 10);
    close(in_fd);

    token_t *tokens = lexer_tokenize("cat < test_in.txt > test_out.txt");
    ast_node_t *root = parser_build_ast(tokens);

    executor_run_ast(root);

    assert_file_content("test_out.txt", "input data");
    unlink("test_in.txt");

    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_input_redirection");
}

void test_invalid_command_resilience() {
    token_t *tokens = lexer_tokenize("this_command_does_not_exist_123");
    ast_node_t *root = parser_build_ast(tokens);

    // This should print a stderr error but safely return control
    executor_run_ast(root);

    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_invalid_command_resilience");
}

void test_complex_background_pipeline() {
    int in_fd = open("test_in.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    write(in_fd, "all good\nerror: system crash\nnothing here\n", 42);
    close(in_fd);

    token_t *tokens = lexer_tokenize("cat < test_in.txt | grep \"error\" > test_out.txt &");
    ast_node_t *root = parser_build_ast(tokens);

    executor_run_ast(root);

    // Wait for the OS to finish running the background pipeline
    // usleep takes microseconds. 250,000 = 0.25 seconds
    usleep(250000);

    assert_file_content("test_out.txt", "error: system crash\n");
    unlink("test_in.txt");

    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_complex_background_pipeline");
}

int main() {
    printf("==========================\n");
    printf("  RUNNING EXECUTOR TESTS  \n");
    printf("==========================\n");

    test_simple_command_redirect();
    test_pipe_execution();
    test_input_redirection();
    test_invalid_command_resilience();
    test_complex_background_pipeline();

    printf("==========================\n");
    printf("\033[0;32mALL EXECUTOR TESTS PASSED!\033[0m\n");
    printf("==========================\n");

    return 0;
}