#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LENGTH 1024
#define MAX_ARGS 100

void expand_variables(char *command) {
    char *start;
    char *end;
    char variable_name[100];
    char *variable_value;

    while ((start = strchr(command, '$')) != NULL) {
        end = start + 1;
        int i = 0;
        while (*end && (*end == '_' || isalnum(*end))) {
            variable_name[i++] = *end++;
        }
        variable_name[i] = '\0';
        variable_value = getenv(variable_name);
        if (variable_value) {
            memmove(start + strlen(variable_value), end, strlen(end) + 1);
            memcpy(start, variable_value, strlen(variable_value));
        } else {
            memmove(start, end, strlen(end) + 1);
        }
    }
}

void run_command(char *command, char **environment_variable) {
    char *args[MAX_ARGS];
    char *token;
    int arg_count = 0;
    int background = 0;

    if (command[strlen(command) - 1] == '&') {
        background = 1;
        command[strlen(command) - 1] = '\0';
    }

    token = strtok(command, " \t");
    while (token != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t");
    }
    args[arg_count] = NULL;

    if (strcmp(args[0], "cd") == 0) {
        if (arg_count > 1) {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        } else {
            chdir(getenv("HOME"));
        }
    } else if (strcmp(args[0], "set") == 0 && arg_count == 3) {
        setenv(args[1], args[2], 1);
    } else if (strcmp(args[0], "unset") == 0 && arg_count == 2) {
        unsetenv(args[1]);
    } else if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; i < arg_count; i++) {
            expand_variables(args[i]);
            printf("%s ", args[i]);
        }
        printf("\n");
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            if (!background) {
                waitpid(pid, NULL, 0);
            }
        } else {
            perror("fork");
        }
    }
}

void run_shell() {
    char command[MAX_LENGTH];
    char *enviroment_variable[] = { NULL };
    while (1) {
        printf("xsh# ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\n")] = 0;
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            break;
        }

        char *pipe_command = strtok(command, "|");
        int number_of_pipes = 0;
        int pipes[2][2];

        while (pipe_command != NULL) {
            if (number_of_pipes > 0) {
                pipe(pipes[number_of_pipes % 2]);
            }

            char *input_file = strchr(pipe_command, '<');
            char *output_file = strchr(pipe_command, '>');

            if (input_file != NULL) {
                *input_file = '\0';
                input_file++;
                input_file = strtok(input_file, " \t");
            }
            if (output_file != NULL) {
                *output_file = '\0';
                output_file++;
                output_file = strtok(output_file, " \t");
            }

            if (number_of_pipes > 0) {
                dup2(pipes[(number_of_pipes - 1) % 2][0], STDIN_FILENO);
            }
            if (output_file != NULL) {
                int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (out_fd == -1) {
                    perror("open");
                    continue;
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            run_command(pipe_command, enviroment_variable);
            pipe_command = strtok(NULL, '|');
            number_of_pipes++;
        }

        if (number_of_pipes > 0) {
            close(pipes[(number_of_pipes - 1) % 2][0]);
            close(pipes[(number_of_pipes - 1) % 2][1]);
        }
    }
}

int main() {
    run_shell();
    return 0;
}