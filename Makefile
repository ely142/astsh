CC = gcc
CFLAGS = -g -Wall -Wextra -std=gnu17 -fsanitize=address -Iinclude
LDFLAGS = -fsanitize=address

SRC_DIR = src
OBJ_DIR = build
INC_DIR = include

TARGETS = $(OBJ_DIR)/shell $(OBJ_DIR)/looper

all: $(TARGETS) 

$(OBJ_DIR)/shell: $(OBJ_DIR)/shell.o $(OBJ_DIR)/line_parser.o
	$(CC) $(LDFLAGS) -o $@ $^ 

$(OBJ_DIR)/looper: $(OBJ_DIR)/looper.o
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(OBJ_DIR)/