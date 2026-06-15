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
    int pid = atoi(argv[index]);
    if (kill(pid, signal) < 0) {
        term_style_begin(ANSI_RED);
        printf("kill: cannot terminate PID %d (not found or protected)\n",
               pid);
        term_style_end();
        return 1;
    }
    return 0;
}
