#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 3) {
        puts("usage: write FILE TEXT...");
        return 1;
    }
    int fd = open(argv[1], O_CREATE | O_WRONLY | O_TRUNC);
    if (fd < 0)
        return 1;
    for (int i = 2; i < argc; i++) {
        if (i > 2)
            write(fd, " ", 1);
        write(fd, argv[i], strlen(argv[i]));
    }
    write(fd, "\n", 1);
    close(fd);
    return 0;
}
