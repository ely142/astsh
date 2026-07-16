#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

int history_size = 0;
history_link_t *history_head;
history_link_t *history_tail;

// the oldest command is in the head, the most recent is at the end of the list
void add_to_history(const char *command) {
    if (history_tail != NULL &&
        (strcmp(history_tail->command, command) == 0)) { // if the need to add command is the same as the most recent
                                                         // one in the history list - no need to add it again
        return;
    }

    history_link_t *new_record = (history_link_t *)malloc(sizeof(history_link_t));

    if (new_record == NULL) {
        fprintf(stderr, "ERROR - failed to allocate memory for new command record in history\n");
        exit(1);
    }

    new_record->command = strdup(command);
    new_record->next = NULL;

    if (history_size == 0) { // this is the first command in history
        history_head = new_record;
        history_tail = new_record;
        history_size++;
    } else { // otherwise
        history_tail->next = new_record;
        history_tail = new_record;

        if (history_size >= HISTLEN) {
            history_link_t *temp = history_head;
            history_head = history_head->next;
            free(temp->command);
            free(temp);
        }

        else {
            history_size++;
        }
    }
}

void print_history() {
    history_link_t *curr = history_head;
    int index = 1;

    while (curr != NULL) {
        printf("%d %s\n", index, curr->command);
        index++;
        curr = curr->next;
    }
}

void free_history() {
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

const char *get_history_command(int index) { //!! - history_size, !n - n
    if (index == history_size) {
        if (strcmp(history_tail->command, "hist") != 0) {
            return history_tail->command;
        } else {
            history_link_t *curr = history_head;
            history_link_t *last_not_history = NULL;

            while (curr != NULL) {
                if (strcmp(curr->command, "hist") != 0) {
                    last_not_history = curr;
                }
                curr = curr->next;
            }

            if (last_not_history != NULL) {
                return last_not_history->command;
            } else {
                fprintf(stderr, "ERROR - failed to find a valid history command\n");
                return NULL;
            }
        }
    }

    else { // index somewhere between 1 to 20
        history_link_t *curr = history_head;
        for (int i = 1; i < index && curr != NULL; i++) {
            curr = curr->next;
        }

        if (curr != NULL && strcmp(curr->command, "hist") != 0) {
            return curr->command;
        } else {
            fprintf(stderr, "ERROR - not a valid command in history\n");
            return NULL;
        }
    }
    return NULL;
}

int get_history_size() {
    return history_size;
}