#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    for (int i = 0; envp != 0 && envp[i] != 0; i++)
        puts(envp[i]);
    return 0;
}
