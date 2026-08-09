#ifndef HISTORY_H
#define HISTORY_H

#define HISTLEN 20

typedef struct history_link {
    char *command;
    struct history_link *next;
} history_link_t;

void history_add(const char *command);
const char *history_get(int index);
void history_print();
void history_free();
int history_get_size();

#endif