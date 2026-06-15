#include "user.h"

int main(void) {
    int pid = fork();
    if (pid < 0)
        goto fail;
    if (pid == 0)
        exit(42);
    int status = 0;
    int result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -2);
    if (result != pid || status != 42)
        goto fail;
    puts("[forktest] PASS");
    return 0;
fail:
    puts("[forktest] FAIL");
    return 1;
}
