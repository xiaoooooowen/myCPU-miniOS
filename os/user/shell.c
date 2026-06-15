#include "user.h"

#define LINE_MAX 256
#define ARG_MAX 16

struct command {
    char *argv[ARG_MAX + 1];
    char storage[ARG_MAX + 2][128];
    int stored;
    int argc;
    char *input;
    char *output;
    int append;
    int background;
};

static char shell_line[LINE_MAX];
static struct command shell_command;

static int read_line(char *line, int capacity) {
    int length = 0;
    for (;;) {
        char character;
        if (read(0, &character, 1) <= 0)
            return -1;
        if (character == '\r' || character == '\n') {
            write(1, "\n", 1);
            line[length] = '\0';
            return length;
        }
        if (character == 8 || character == 127) {
            if (length > 0) {
                length--;
                write(1, "\b \b", 3);
            }
            continue;
        }
        if (length + 1 < capacity) {
            line[length++] = character;
            write(1, &character, 1);
        }
    }
}

static int is_space(char character) {
    return character == ' ' || character == '\t';
}

static int next_word(char **cursor_pointer, char *destination, int capacity) {
    char *cursor = *cursor_pointer;
    int length = 0;
    char quote = 0;
    while (*cursor != '\0') {
        char character = *cursor;
        if (quote == 0 &&
            (is_space(character) || character == '<' ||
             character == '>' || character == '&'))
            break;
        cursor++;
        if (quote != 0) {
            if (character == quote) {
                quote = 0;
                continue;
            }
        } else if (character == '\'' || character == '"') {
            quote = character;
            continue;
        }
        if (character == '\\' && *cursor != '\0')
            character = *cursor++;
        if (length + 1 >= capacity)
            return -1;
        destination[length++] = character;
    }
    if (quote != 0 || length == 0)
        return -1;
    destination[length] = '\0';
    *cursor_pointer = cursor;
    return 0;
}

static int parse_command(char *line, struct command *command) {
    memset(command, 0, sizeof(*command));
    char *cursor = line;
    while (*cursor != '\0') {
        while (is_space(*cursor))
            cursor++;
        if (*cursor == '\0')
            break;
        if (*cursor == '&') {
            cursor++;
            command->background = 1;
            while (is_space(*cursor))
                cursor++;
            if (*cursor != '\0')
                return -1;
            break;
        }
        if (*cursor == '<' || *cursor == '>') {
            int operator = *cursor++ == '<' ? 1 : 2;
            if (operator == 2 && *cursor == '>') {
                operator = 3;
                cursor++;
            }
            while (is_space(*cursor))
                cursor++;
            if (command->stored >= ARG_MAX + 2 ||
                next_word(&cursor, command->storage[command->stored],
                          sizeof(command->storage[0])) < 0)
                return -1;
            char *path = command->storage[command->stored++];
            if (operator == 1) {
                if (command->input != 0)
                    return -1;
                command->input = path;
            } else {
                if (command->output != 0)
                    return -1;
                command->output = path;
                command->append = operator == 3;
            }
            continue;
        }
        if (command->argc >= ARG_MAX || command->stored >= ARG_MAX + 2)
            return -1;
        char *token = command->storage[command->stored++];
        if (next_word(&cursor, token, sizeof(command->storage[0])) < 0)
            return -1;
        command->argv[command->argc++] = token;
    }
    command->argv[command->argc] = 0;
    return command->argc == 0 ? 1 : 0;
}

static void build_environment(char *envp[], char pwd_entry[260]) {
    strcpy(pwd_entry, "PWD=");
    getcwd(pwd_entry + 4, 256);
    envp[0] = "PATH=/bin:/tests";
    envp[1] = "HOME=/";
    envp[2] = pwd_entry;
    envp[3] = 0;
}

static int apply_redirections(const struct command *command) {
    if (command->input != 0) {
        int fd = open(command->input, O_RDONLY);
        if (fd < 0 || dup2(fd, 0) < 0)
            return -1;
        close(fd);
    }
    if (command->output != 0) {
        int flags = O_CREATE | O_WRONLY |
                    (command->append ? O_APPEND : O_TRUNC);
        int fd = open(command->output, flags);
        if (fd < 0 || dup2(fd, 1) < 0)
            return -1;
        close(fd);
    }
    return 0;
}

static int try_exec(char **argv, char **envp) {
    if (argv[0][0] == '/' || argv[0][0] == '.') {
        execve(argv[0], argv, envp);
        return -1;
    }
    char path[256];
    strcpy(path, "/bin/");
    strcat(path, argv[0]);
    execve(path, argv, envp);
    strcpy(path, "/tests/");
    strcat(path, argv[0]);
    execve(path, argv, envp);
    return -1;
}

static int wait_for(int pid) {
    int status = 0;
    int result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -2);
    return result < 0 ? 1 : status;
}

static void print_help(void) {
    puts("builtins: help cd exit exec run");
    puts("external: ls cat echo pwd ps kill env mkdir rm touch write");
    puts("syntax: COMMAND [ARGS] [< FILE] [> FILE|>> FILE] [&]");
}

int main(int argc, char **argv, char **initial_envp) {
    (void)argc;
    (void)argv;
    (void)initial_envp;
    for (;;) {
        while (waitpid(-1, 0, WNOHANG) > 0)
            ;
        write(1, "minios> ", 8);
        if (read_line(shell_line, sizeof(shell_line)) < 0)
            break;
        int parsed = parse_command(shell_line, &shell_command);
        if (parsed > 0)
            continue;
        if (parsed < 0) {
            puts("shell: parse error");
            continue;
        }
        if (strcmp(shell_command.argv[0], "help") == 0) {
            print_help();
            continue;
        }
        if (strcmp(shell_command.argv[0], "cd") == 0) {
            if (shell_command.argc != 2 ||
                chdir(shell_command.argv[1]) < 0)
                puts("cd: failed");
            continue;
        }
        if (strcmp(shell_command.argv[0], "exit") == 0)
            return shell_command.argc > 1 ?
                atoi(shell_command.argv[1]) : 0;

        int direct_exec = strcmp(shell_command.argv[0], "exec") == 0;
        int run_alias = strcmp(shell_command.argv[0], "run") == 0;
        char **program_argv =
            shell_command.argv + (direct_exec || run_alias);
        if ((direct_exec || run_alias) && program_argv[0] == 0) {
            puts("shell: missing program");
            continue;
        }
        char pwd_entry[260];
        char *envp[4];
        build_environment(envp, pwd_entry);
        if (direct_exec) {
            int saved_input = dup2(0, 14);
            int saved_output = dup2(1, 15);
            if (saved_input >= 0)
                set_cloexec(14, 1);
            if (saved_output >= 0)
                set_cloexec(15, 1);
            if (saved_input < 0 || saved_output < 0 ||
                apply_redirections(&shell_command) < 0 ||
                try_exec(program_argv, envp) < 0) {
                if (saved_input >= 0) {
                    dup2(14, 0);
                    close(14);
                }
                if (saved_output >= 0) {
                    dup2(15, 1);
                    close(15);
                }
                puts("exec: program not found");
            }
            continue;
        }
        int pid = fork();
        if (pid < 0) {
            puts("shell: fork failed");
            continue;
        }
        if (pid == 0) {
            if (apply_redirections(&shell_command) < 0) {
                puts("shell: redirection failed");
                exit(126);
            }
            if (try_exec(program_argv, envp) < 0) {
                printf("%s: program not found\n", program_argv[0]);
                exit(127);
            }
        }
        if (shell_command.background) {
            printf("[pid %d]\n", pid);
            continue;
        }
        wait_for(pid);
    }
    return 0;
}
