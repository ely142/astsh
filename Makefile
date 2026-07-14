CC = gcc
CFLAGS = -g -Wall -Wextra -std=gnu17 -Iinclude
LDFLAGS =

SRC_DIR = src
OBJ_DIR = build
INC_DIR = include

VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

TARGETS = $(OBJ_DIR)/shell $(OBJ_DIR)/looper

SHELL_OBJS = $(OBJ_DIR)/main.o \
             $(OBJ_DIR)/line_parser.o \
             $(OBJ_DIR)/history.o \
             $(OBJ_DIR)/jobs.o \
             $(OBJ_DIR)/executor.o \
			 $(OBJ_DIR)/lexer.o \
			 $(OBJ_DIR)/parser.o

LOOPER_OBJS = $(OBJ_DIR)/looper.o

all: $(TARGETS) 

test_lexer: src/lexer.c tests/test_lexer.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/test_lexer src/lexer.c tests/test_lexer.c
	./$(OBJ_DIR)/test_lexer

test_parser: src/lexer.c src/parser.c tests/test_parser.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/test_parser src/lexer.c src/parser.c tests/test_parser.c
	./$(OBJ_DIR)/test_parser

test_executor: src/lexer.c src/parser.c src/executor.c tests/test_executor.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/test_executor src/lexer.c src/parser.c src/executor.c tests/test_executor.c
	./$(OBJ_DIR)/test_executor
	
valgrind_lexer: test_lexer
	$(VALGRIND) ./$(OBJ_DIR)/test_lexer

valgrind_parser: test_parser
	$(VALGRIND) ./$(OBJ_DIR)/test_parser

valgrind_executor: test_executor
	$(VALGRIND) ./$(OBJ_DIR)/test_executor
	
$(OBJ_DIR)/shell: $(SHELL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ 

$(OBJ_DIR)/looper: $(LOOPER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(OBJ_DIR)/