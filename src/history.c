#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

static int history_size = 0;
static history_link_t *history_head;
static history_link_t *history_tail;

void history_add(const char *command) {
    if (strcmp(command, "hist") == 0 || command[0] == '!') {
        return;
    }

    if (history_tail != NULL && (strcmp(history_tail->command, command) == 0)) {
        return;
    }

    history_link_t *new_record = (history_link_t *)malloc(sizeof(history_link_t));

    if (new_record == NULL) {
        fprintf(stderr, "[ERROR] History: failed to allocate memory for new command record in history\n");
        exit(EXIT_FAILURE);
    }

    new_record->command = strdup(command);
    if (new_record->command == NULL) {
        fprintf(stderr, "[ERROR] History: memory allocation failed for command string\n");
        free(new_record);
        exit(EXIT_FAILURE);
    }

    new_record->next = NULL;

    if (history_size == 0) {
        history_head = new_record;
        history_tail = new_record;
        history_size++;
    } else {
        history_tail->next = new_record;
        history_tail = new_record;

        if (history_size >= HISTLEN) {
            history_link_t *temp = history_head;
            history_head = history_head->next;
            free(temp->command);
            free(temp);
        } else {
            history_size++;
        }
    }
}

void history_print() {
    history_link_t *curr = history_head;
    int index = 1;

    while (curr != NULL) {
        printf("%d %s\n", index, curr->command);
        index++;
        curr = curr->next;
    }
}

void history_free() {
    while (history_head) {
        history_link_t *curr = history_head;
        history_head = history_head->next;
        free(curr->command);
        free(curr);
    }
    history_size = 0;
    history_tail = NULL;
    history_head = NULL;
}

const char *history_get(int index) {
    if (history_size == 0) {
        fprintf(stderr, "[ERROR] History: no commands in history\n");
        return NULL;
    }

    if (index < 1 || index > history_size) {
        fprintf(stderr, "[ERROR] History: event not found\n");
        return NULL;
    }

    history_link_t *curr = history_head;
    for (int i = 1; i < index && curr != NULL; i++) {
        curr = curr->next;
    }

    if (curr != NULL) {
        return curr->command;
    }

    return NULL;
}

int history_get_size() {
    return history_size;
}