#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2)
        return 1;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_CREATE | O_WRONLY);
        if (fd < 0)
            return 1;
        close(fd);
    }
    return 0;
}
