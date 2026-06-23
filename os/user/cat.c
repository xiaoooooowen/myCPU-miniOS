#include "user.h"

static int copy_fd(int fd) {
    char buffer[256];
    long count;
    while ((count = read(fd, buffer, sizeof(buffer))) > 0)
        if (write(1, buffer, (size_t)count) != count)
            return 1;
    return count < 0 ? 1 : 0;
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2)
        return copy_fd(0);
    int status = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("cat: cannot open %s\n", argv[i]);
            status = 1;
            continue;
        }
        if (copy_fd(fd) != 0)
            status = 1;
        close(fd);
    }
    return status;
}
