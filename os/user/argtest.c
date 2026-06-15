#include "user.h"

int main(int argc, char **argv, char **envp) {
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("argv[%d]=%s\n", i, argv[i]);
    for (int i = 0; envp != 0 && envp[i] != 0; i++)
        printf("envp[%d]=%s\n", i, envp[i]);
    return 0;
}
