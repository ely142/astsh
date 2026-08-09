#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"
#include "jobs.h"
#include "lexer.h"
#include "parser.h"

#define TEST_PASS(name) printf("\033[0;32m[PASS]\033[0m %s\n", name)
#define TEST_FAIL(name) printf("\033[0;31m[FAIL]\033[0m %s\n", name)

extern process_t *process_list;

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

    jobs_free();
    parser_free_ast(root);
    lexer_free_tokens(tokens);
    TEST_PASS("test_complex_background_pipeline");
}

void test_builtin_cd() {
    char original_dir[4096];
    getcwd(original_dir, sizeof(original_dir));

    token_t *tokens = lexer_tokenize("cd /");
    ast_node_t *root = parser_build_ast(tokens);

    executor_run_ast(root);

    char new_dir[4096];
    getcwd(new_dir, sizeof(new_dir));

    parser_free_ast(root);
    lexer_free_tokens(tokens);

    // Restore state to prevent breaking subsequent tests
    chdir(original_dir);

    assert(strcmp(original_dir, new_dir) != 0 && "cd built-in failed to change directory.");
    assert(strcmp(new_dir, "/") == 0 && "cd built-in did not navigate to the correct target.");

    TEST_PASS("test_builtin_cd");
}

void test_builtin_procs() {
    token_t *bg_tokens = lexer_tokenize("sleep 10 &");
    ast_node_t *bg_root = parser_build_ast(bg_tokens);
    executor_run_ast(bg_root);

    usleep(10000);

    // Reroute stdout in the parent process to capture built-in output
    int saved_stdout = dup(STDOUT_FILENO);
    int out_fd = open("procs_out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);

    token_t *procs_tokens = lexer_tokenize("procs");
    ast_node_t *procs_root = parser_build_ast(procs_tokens);
    executor_run_ast(procs_root);

    // Restore stdout
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    int verify_fd = open("procs_out.txt", O_RDONLY);
    assert(verify_fd >= 0 && "Failed to open captured procs output.");

    char buffer[512] = {0};
    ssize_t bytes_read = read(verify_fd, buffer, sizeof(buffer) - 1);
    close(verify_fd);

    unlink("procs_out.txt");
    parser_free_ast(procs_root);
    lexer_free_tokens(procs_tokens);
    parser_free_ast(bg_root);
    lexer_free_tokens(bg_tokens);
    jobs_free();

    assert(bytes_read > 0 && "Output file is entirely empty.");
    assert(strstr(buffer, "sleep") && "procs output did not contain the 'sleep' job.");

    TEST_PASS("test_builtin_procs");
}

static int check_job_status_via_procs(const char *expected_substring) {
    int saved_stdout = dup(STDOUT_FILENO);
    int out_fd = open("signal_out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);

    token_t *procs_tokens = lexer_tokenize("procs");
    ast_node_t *procs_root = parser_build_ast(procs_tokens);
    executor_run_ast(procs_root);

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    int verify_fd = open("signal_out.txt", O_RDONLY);
    assert(verify_fd >= 0 && "[FATAL] Failed to open signal_out.txt");

    char buffer[512] = {0};
    read(verify_fd, buffer, sizeof(buffer) - 1);
    close(verify_fd);

    unlink("signal_out.txt");
    parser_free_ast(procs_root);
    lexer_free_tokens(procs_tokens);

    // Return 1 if substring found, 0 otherwise
    return strstr(buffer, expected_substring) != NULL;
}

void test_builtin_signals() {
    token_t *bg_tokens = lexer_tokenize("./build/looper &");
    ast_node_t *bg_root = parser_build_ast(bg_tokens);
    executor_run_ast(bg_root);

    usleep(50000);
    int is_running_initially = check_job_status_via_procs("Running");

    // Retrieve the dynamically assigned PID from the jobs manager
    assert(process_list != NULL && "Background process failed to register in jobs list.");
    pid_t target_pid = process_list->pid;
    char cmd_buffer[64];

    snprintf(cmd_buffer, sizeof(cmd_buffer), "halt %d", target_pid);
    token_t *halt_tokens = lexer_tokenize(cmd_buffer);
    ast_node_t *halt_root = parser_build_ast(halt_tokens);
    executor_run_ast(halt_root);

    usleep(50000);
    int is_suspended = check_job_status_via_procs("Suspended");

    snprintf(cmd_buffer, sizeof(cmd_buffer), "wakeup %d", target_pid);
    token_t *wakeup_tokens = lexer_tokenize(cmd_buffer);
    ast_node_t *wakeup_root = parser_build_ast(wakeup_tokens);
    executor_run_ast(wakeup_root);

    usleep(50000);
    int is_running_again = check_job_status_via_procs("Running");

    snprintf(cmd_buffer, sizeof(cmd_buffer), "ice %d", target_pid);
    token_t *ice_tokens = lexer_tokenize(cmd_buffer);
    ast_node_t *ice_root = parser_build_ast(ice_tokens);
    executor_run_ast(ice_root);

    usleep(50000);
    int is_terminated = check_job_status_via_procs("Terminated");

    parser_free_ast(ice_root);
    lexer_free_tokens(ice_tokens);
    parser_free_ast(wakeup_root);
    lexer_free_tokens(wakeup_tokens);
    parser_free_ast(halt_root);
    lexer_free_tokens(halt_tokens);
    parser_free_ast(bg_root);
    lexer_free_tokens(bg_tokens);

    jobs_free();

    assert(is_running_initially && "Background job failed to start as Running.");
    assert(is_suspended && "halt built-in failed to transition job to Suspended.");
    assert(is_running_again && "wakeup built-in failed to transition job to Running.");
    assert(is_terminated && "ice built-in failed to transition job to Terminated.");

    TEST_PASS("test_builtin_signals");
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
    test_builtin_cd();
    test_builtin_procs();
    test_builtin_signals();

    printf("==========================\n");
    printf("\033[0;32mALL EXECUTOR TESTS PASSED!\033[0m\n");
    printf("==========================\n");

    return 0;
}