#include "line_parser.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

#define TERMINATED -1
#define RUNNING    1
#define SUSPENDED  0

#define HISTLEN 20

typedef struct process {
    cmd_line *cmd;
    pid_t pid;
    int status;
    struct process *next;
} process;

typedef struct HistLink {
    char *command;
    struct HistLink *next;
} HistLink;

int historySize = 0;
HistLink *historyHead;
HistLink *historyTail;

int debug_mode = 0;
process *process_list = NULL;

// lab 2
void prompt();
void execute(cmd_line *pCmdLine);
int processSignal(const char *signalName, pid_t pid);

// lab C
int pipedHandle(cmd_line *pCmdLine);
void addProcess(process **process_list, cmd_line *cmd, pid_t pid);
void printProcessList(process **process_list);
void freeProcessList(process *process_list);
void updateProcessList(process **process_list);
void updateProcessStatus(process *process_list, int pid, int status);
void addToHistory(const char *command);
const char *getHistoryCommand(int index);
void printHistory();
void freeHistory();

int main(int argc, char **argv) {

    char buffer[BUFFER_SIZE];

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) { // properly compare strings in c - stackoverflow.com
            debug_mode = 1;
        }
    }

    while (1) {
        prompt();

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) { // checks for EOF
            printf("\n");
            exit(0);
        }
        buffer[strcspn(buffer, "\n")] =
            '\0'; // removing trailing newline character from fgets() input - stackoverflow.com

        if (strlen(buffer) == 0) { // skips if user just pressed 'enter'
            continue;
        }

        if (strcmp(buffer, "quit") == 0) {
            break;
        }

        if (strcmp(buffer, "hist") == 0) { // history commands handling
            addToHistory(buffer);
            printHistory();
            continue;
        }

        else if (strcmp(buffer, "!!") == 0) {
            if (historyHead == NULL) {
                fprintf(stderr, "ERROR - invalid index of history command\n");
                continue;
            } else {
                const char *command = getHistoryCommand(historySize);
                if (command == NULL) {
                    continue;
                }
                printf("%s\n", command);
                strcpy(buffer, command);
            }
        }

        else if (buffer[0] == '!' && buffer[1] != '\0') {
            int n = atoi(buffer + 1);
            if (n < 1 || n > historySize) {
                fprintf(stderr, "ERROR - invalid index of history command\n");
                continue;
            } else {
                const char *command = getHistoryCommand(n);
                if (command == NULL) {
                    continue;
                }
                printf("%s\n", command);
                strcpy(buffer, command);
            }
        }

        cmd_line *parsedLine = line_parser_parse(buffer);

        if (parsedLine == NULL) {
            fprintf(stderr, "ERROR - failed to parse command\n");
            continue;
        }

        addToHistory(buffer);

        if (strcmp(parsedLine->arguments[0], "cd") == 0) { // cd command handling
            if (parsedLine->arg_count != 2) {
                fprintf(stderr, "ERROR - mismatch arguments count for cd operation\n");
            } else {
                if (chdir(parsedLine->arguments[1]) == -1) { // linux man page for chdir
                    fprintf(stderr, "ERROR - chdir() operation failed\n");
                }
            }
            line_parser_free(parsedLine);
        }

        else if ((strcmp(parsedLine->arguments[0], "halt") == 0) || (strcmp(parsedLine->arguments[0], "wakeup") == 0) ||
                 (strcmp(parsedLine->arguments[0], "ice") == 0)) { // signal commands handling
            if (parsedLine->arg_count != 2) {
                fprintf(stderr, "ERROR - mismatch arguments count for %s signal operation\n", parsedLine->arguments[0]);
            } else {
                int pidToSignal = atoi(parsedLine->arguments[1]);

                if (pidToSignal == 0) { // user based process have pid != 0, therefore indicates atoi error
                    fprintf(stderr, "ERROR - failed to convert pid from string to int for %s signaling process\n",
                            parsedLine->arguments[0]);
                } else {
                    if (processSignal(parsedLine->arguments[0], pidToSignal) == -1) {
                        fprintf(stderr, "ERROR - failed to send the signal %s to the process whose pid is %d\n",
                                parsedLine->arguments[0], pidToSignal);
                    }
                }
            }
            line_parser_free(parsedLine);
        }

        else if (strcmp(parsedLine->arguments[0], "procs") == 0) { // procs command handling
            printProcessList(&process_list);
            line_parser_free(parsedLine);
        }

        else if (parsedLine->next != NULL) { // piped commands handling
            if (pipedHandle(parsedLine) == -1) {
                fprintf(stderr, "ERROR - failed to handle piped command\n");
            }
        }

        else { // execute command handling
            execute(parsedLine);
        }
    }
    freeProcessList(process_list);
    freeHistory();
    return 0;
}

void prompt() {
    char cwd_path[PATH_MAX];

    if (getcwd(cwd_path, PATH_MAX) == NULL) {
        fprintf(stderr, "ERROR - failed to get the current working directory\n");
    } else {
        printf("%s #> ", cwd_path);
    }
}

void execute(cmd_line *pCmdLine) {

    pid_t pid;
    int status;

    if (!(pid = fork())) { // child process, pattern taken from lecture 2

        if (debug_mode) {
            fprintf(stderr, "process pid: %d, executing command: %s\n", pid, pCmdLine->arguments[0]);
        }

        if (pCmdLine->input_redirect != NULL) {
            close(STDIN_FILENO);                                  // closing the standard input stream (fd 0)
            if (open(pCmdLine->input_redirect, O_RDONLY) == -1) { // man page open for reading
                fprintf(stderr, "ERROR - failed to redirect input\n");
                _exit(1);
            }
        }

        if (pCmdLine->output_redirect != NULL) {
            close(STDOUT_FILENO); // closing the standard output stream (fd 1)
            if (open(pCmdLine->output_redirect, O_CREAT | O_WRONLY | O_TRUNC, 0644) ==
                -1) { // man page open for writing + create file if doesn't exist
                fprintf(stderr, "ERROR - failed to redirect output\n");
                _exit(1);
            }
        }

        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
            perror("ERROR - execvp() error happened");
            _exit(1);
        }
    }

    else { // parent process
        if (debug_mode) {
            fprintf(stderr, "process pid: %d, executing command: %s\n", pid, pCmdLine->arguments[0]);
        }

        if (pCmdLine->is_blocking) { // parent process will wait for child if & isn't provided with the command
            if (waitpid(pid, &status, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() operation failed for the given pid: %d, status: %d\n", pid, status);
            }
        }
        addProcess(&process_list, pCmdLine, pid);
    }
}

int processSignal(const char *signalName, pid_t pid) {

    if (strcmp(signalName, "halt") == 0) {
        if (kill(pid, SIGTSTP) == 0) {
            printf("Process %d successfully stopped\n", pid);
            updateProcessStatus(process_list, pid, SUSPENDED);
        } else {
            return -1;
        }
    } else if (strcmp(signalName, "wakeup") == 0) {
        if (kill(pid, SIGCONT) == 0) {
            printf("Process %d successfully continued\n", pid);
            updateProcessStatus(process_list, pid, RUNNING);
        } else {
            return -1;
        }
    } else if (strcmp(signalName, "ice") == 0) {
        if (kill(pid, SIGINT) == 0) {
            printf("Process %d successfully terminated\n", pid);
            updateProcessStatus(process_list, pid, TERMINATED);
        } else {
            return -1;
        }
    } else {
        fprintf(stderr, "ERROR - the signal %s is not supported in this program\n", signalName);
        return -1;
    }

    return 0;
}

int pipedHandle(cmd_line *pCmdLine) {
    int fd[2];
    pid_t pidFirst, pidSecond;

    if (pCmdLine->output_redirect != NULL) {
        fprintf(stderr, "ERROR - output redirection on the left side of a piped command isn't allowed\n");
        return -1;
    }

    if (pCmdLine->next->input_redirect != NULL) {
        fprintf(stderr, "ERROR - input redirection on the right side of a piped command isn't allowed\n");
        return -1;
    }

    if (pipe(fd) == -1) {
        perror("ERROR - failed to create a pipe\n");
        return -1;
    }

    if (!(pidFirst = fork())) { // first child process

        if (pCmdLine->input_redirect != NULL) {
            int inputfd = open(pCmdLine->input_redirect, O_RDONLY);
            if (inputfd == -1) {
                fprintf(stderr, "ERROR - failed to redirect input\n");
                _exit(1);
            }

            if (dup2(inputfd, STDIN_FILENO) == -1) {
                fprintf(stderr, "ERROR - dup2 failure\n");
                _exit(1);
            }
            close(inputfd);
        }

        close(STDOUT_FILENO);
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "ERROR - dup2 failure\n");
            _exit(1);
        }
        close(fd[0]);
        close(fd[1]);

        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
            perror("ERROR - execvp() error happened");
            _exit(1);
        }
        exit(1);
    }

    else { // parent process
        addProcess(&process_list, pCmdLine, pidFirst);
        close(fd[1]);

        if (!(pidSecond = fork())) { // second child process
            if (pCmdLine->next->output_redirect != NULL) {
                int outputfd = open(pCmdLine->next->output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (outputfd == -1) {
                    fprintf(stderr, "ERROR - failed to redirect output\n");
                    _exit(1);
                }
                if (dup2(outputfd, STDOUT_FILENO) == -1) {
                    fprintf(stderr, "ERROR - dup2 failure\n");
                    _exit(1);
                }
                close(outputfd);
            }
            close(STDIN_FILENO);
            if (dup2(fd[0], STDIN_FILENO) == -1) {
                fprintf(stderr, "ERROR - dup2 failure\n");
                _exit(1);
            }
            close(fd[0]);
            close(fd[1]);

            if (execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments) == -1) {
                perror("ERROR - execvp() error happened");
                _exit(1);
            }
            exit(1);
        }

        else { // parent process
            addProcess(&process_list, pCmdLine->next, pidSecond);

            close(fd[0]);
            close(fd[1]);

            if (waitpid(pidFirst, NULL, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() failed\n");
            }
            if (waitpid(pidSecond, NULL, 0) == -1) {
                fprintf(stderr, "ERROR - waitpid() failed\n");
            }

            pCmdLine->next = NULL; // ensuring freeCmdLines for the first child process won't erase the next piped
                                   // command process, as it will be handled seperatly in the process manager
        }
    }
    return 1;
}

void addProcess(process **process_list, cmd_line *cmd, pid_t pid) {
    process *newProc = (process *)malloc(sizeof(process));

    if (newProc == NULL) {
        fprintf(stderr, "ERROR - failed to allocate memory for the new process\n");
        exit(1);
    }

    newProc->cmd = cmd;
    newProc->pid = pid;
    newProc->status = RUNNING; // default state
    newProc->next = *process_list;
    *process_list = newProc;
}

void printProcessList(process **process_list) {
    updateProcessList(process_list);
    process *curr = *process_list;
    process *prev = NULL; // head of the list doesn't have a previous node
    const char *stat;
    int index = 0;

    printf("Index\t\tPID\t\tSTATUS\t\tCommand & Arguments\n");
    while (curr != NULL) {
        if (curr->status == RUNNING) {
            stat = "Running";
        } else if (curr->status == SUSPENDED) {
            stat = "Suspended";
        } else {
            stat = "Terminated";
        }

        printf("%d\t\t%d\t\t%s\t\t%s ", index, curr->pid, stat, curr->cmd->arguments[0]);

        for (int i = 1; curr->cmd->arguments[i] != NULL; i++) {
            printf("%s ", curr->cmd->arguments[i]);
        }
        printf("\n");
        index++;

        if (curr->status == TERMINATED) { // cleaning freshly terminated processes from the list
            if (prev == NULL) {
                *process_list = curr->next;
            } else {
                prev->next = curr->next;
            }

            if (curr->cmd != NULL) {
                line_parser_free(curr->cmd);
                curr->cmd = NULL;
            }

            process *next = curr->next;
            free(curr);
            curr = next; // prev stays the same
        }

        else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void freeProcessList(process *process_list) {

    while (process_list != NULL) {
        process *curr = process_list;
        process_list = process_list->next;

        if (curr->cmd != NULL) {
            line_parser_free(curr->cmd);
        }
        free(curr);
    }
}

void updateProcessList(process **process_list) {
    process *curr = *process_list;

    while (curr != NULL) {
        int status;
        pid_t retVal = waitpid(curr->pid, &status, WNOHANG); // waitpid(2) - linux man page

        if (retVal > 0) {
            if (WIFSTOPPED(status)) {
                updateProcessStatus(*process_list, curr->pid, SUSPENDED);
            } else if (WIFCONTINUED(status)) {
                updateProcessStatus(*process_list, curr->pid, RUNNING);
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                updateProcessStatus(*process_list, curr->pid, TERMINATED);
            }

        } else if (retVal == -1) { // process not found -> considered 'terminated'
            updateProcessStatus(*process_list, curr->pid, TERMINATED);
        }
        // else - retVal == 0, process still running, hasn't changed state

        curr = curr->next;
    }
}

void updateProcessStatus(process *process_list, int pid, int status) {
    process *curr = process_list;

    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            break;
        }
        curr = curr->next;
    }
}

// the oldest command is in the head, the most recent is at the end of the list
void addToHistory(const char *command) {
    if (historyTail != NULL &&
        (strcmp(historyTail->command, command) == 0)) { // if the need to add command is the same as the most recent one
                                                        // in the history list - no need to add it again
        return;
    }

    HistLink *newRecord = (HistLink *)malloc(sizeof(HistLink));

    if (newRecord == NULL) {
        fprintf(stderr, "ERROR - failed to allocate memory for new command record in history\n");
        exit(1);
    }

    newRecord->command = strdup(command); // strdup and strdndup functions in c - geeksforgeeks.org
    newRecord->next = NULL;

    if (historySize == 0) { // this is the first command in history
        historyHead = newRecord;
        historyTail = newRecord;
        historySize++;
    } else { // otherwise
        historyTail->next = newRecord;
        historyTail = newRecord;

        if (historySize >= HISTLEN) {
            HistLink *temp = historyHead;
            historyHead = historyHead->next;
            free(temp->command);
            free(temp);
        }

        else {
            historySize++;
        }
    }
}

void printHistory() {
    HistLink *curr = historyHead;
    int index = 1;

    while (curr != NULL) {
        printf("%d %s\n", index, curr->command);
        index++;
        curr = curr->next;
    }
}

void freeHistory() {
    while (historyHead) {
        HistLink *curr = historyHead;
        historyHead = historyHead->next;
        free(curr->command);
        free(curr);
    }
    historySize = 0;
    historyTail = NULL;
    historyHead = NULL;
}

const char *getHistoryCommand(int index) { //!! - historySize, !n - n
    if (index == historySize) {
        if (strcmp(historyTail->command, "hist") != 0) {
            return historyTail->command;
        } else {
            HistLink *curr = historyHead;
            HistLink *lastNotHist = NULL;

            while (curr != NULL) {
                if (strcmp(curr->command, "hist") != 0) {
                    lastNotHist = curr;
                }
                curr = curr->next;
            }

            if (lastNotHist != NULL) {
                return lastNotHist->command;
            } else {
                fprintf(stderr, "ERROR - failed to find a valid history command\n");
                return NULL;
            }
        }
    }

    else { // index somewhere between 1 to 20
        HistLink *curr = historyHead;
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
