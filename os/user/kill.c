#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int signal = 15;
    int index = 1;
    if (argc > 1 && (strcmp(argv[1], "-9") == 0 ||
                     strcmp(argv[1], "-15") == 0)) {
        signal = atoi(argv[1] + 1);
        index++;
    }
    if (index >= argc) {
        puts("usage: kill [-9|-15] PID");
        return 1;
    }
    if (kill(atoi(argv[index]), signal) < 0) {
        puts("kill: failed");
        return 1;
    }
    return 0;
}
