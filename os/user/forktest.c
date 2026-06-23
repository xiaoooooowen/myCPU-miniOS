#include "user.h"

int main(void) {
    int pid = fork();
    if (pid < 0) {
        puts("[forktest] fork child ........ FAIL");
        puts("[forktest] TEST FAILED");
        return 1;
    }
    if (pid == 0) {
        puts("[forktest] child branch ...... PASS");
        exit(42);
    }

    puts("[forktest] fork child ........ PASS");
    int status = 0;
    int result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -2);

    if (result != pid) {
        puts("[forktest] waitpid ........... FAIL");
        puts("[forktest] TEST FAILED");
        return 1;
    }
    puts("[forktest] waitpid ........... PASS");

    if (status != 42) {
        puts("[forktest] child exit code ... FAIL");
        puts("[forktest] TEST FAILED");
        return 1;
    }
    puts("[forktest] child exit code ... PASS");
    puts("[forktest] ALL PASS");
    return 0;
}
