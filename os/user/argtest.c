#include "user.h"

static int report(const char *name, int passed) {
    printf("[argtest] %s %s\n", name, passed ? "PASS" : "FAIL");
    return passed;
}

int main(int argc, char **argv, char **envp) {
    int passed = 1;

    passed &= report("argc ...............", argc == 3);
    passed &= report("argv contents ......",
                     argc == 3 &&
                     strcmp(argv[1], "hello") == 0 &&
                     strcmp(argv[2], "two words") == 0);

    char *path = getenv_from(envp, "PATH");
    char *home = getenv_from(envp, "HOME");
    char *pwd = getenv_from(envp, "PWD");
    passed &= report("environment ........",
                     path != 0 && strcmp(path, "/bin:/tests") == 0 &&
                     home != 0 && strcmp(home, "/") == 0 &&
                     pwd != 0 && pwd[0] == '/');

    puts(passed ? "[argtest] ALL PASS" : "[argtest] TEST FAILED");
    return passed ? 0 : 1;
}
