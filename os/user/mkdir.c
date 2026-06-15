#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc != 2 || mkdir(argv[1]) < 0) {
        puts("usage: mkdir DIR");
        return 1;
    }
    return 0;
}
