#ifndef HISTORY_H
#define HISTORY_H

#define HISTLEN 20

typedef struct history_link {
    char *command;
    struct history_link *next;
} history_link_t;

void add_to_history(const char *command);
const char *get_history_command(int index);
void print_history();
void free_history();
int get_history_size();

#endif