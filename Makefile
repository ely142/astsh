CC = gcc
CFLAGS = -g -Wall -Wextra -std=gnu17 -Iinclude
LDFLAGS =

SRC_DIR = src
OBJ_DIR = build
INC_DIR = include

VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

TARGETS = $(OBJ_DIR)/astsh $(OBJ_DIR)/looper

SHELL_OBJS = $(OBJ_DIR)/main.o \
             $(OBJ_DIR)/history.o \
             $(OBJ_DIR)/jobs.o \
			 $(OBJ_DIR)/builtins.o \
             $(OBJ_DIR)/executor.o \
			 $(OBJ_DIR)/lexer.o \
			 $(OBJ_DIR)/parser.o

LOOPER_OBJS = $(OBJ_DIR)/looper.o

all: $(TARGETS) 

$(OBJ_DIR)/astsh: $(SHELL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ 

$(OBJ_DIR)/looper: $(LOOPER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/test_lexer: $(SRC_DIR)/lexer.c tests/test_lexer.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/test_parser: $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c tests/test_parser.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/test_executor: $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c $(SRC_DIR)/executor.c $(SRC_DIR)/jobs.c $(SRC_DIR)/builtins.c tests/test_executor.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ $^

test_lexer: $(OBJ_DIR)/test_lexer
	./$<

test_parser: $(OBJ_DIR)/test_parser
	./$<

test_executor: $(OBJ_DIR)/test_executor
	./$<
    
valgrind_lexer: $(OBJ_DIR)/test_lexer
	$(VALGRIND) ./$<

valgrind_parser: $(OBJ_DIR)/test_parser
	$(VALGRIND) ./$<

valgrind_executor: $(OBJ_DIR)/test_executor
	$(VALGRIND) ./$<

valgrind_shell: $(OBJ_DIR)/astsh
	$(VALGRIND) ./$<
    
.PHONY: all clean test_lexer test_parser test_executor valgrind_lexer valgrind_parser valgrind_executor valgrind_shell

clean:
	rm -rf $(OBJ_DIR)/