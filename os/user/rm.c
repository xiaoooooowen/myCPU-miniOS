#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        puts("usage: rm PATH...");
        return 1;
    }
    int result = 0;
    for (int i = 1; i < argc; i++)
        if (unlink(argv[i]) < 0) {
            printf("rm: cannot remove %s\n", argv[i]);
            result = 1;
        }
    return result;
}
