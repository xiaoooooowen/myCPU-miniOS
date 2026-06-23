#include "user.h"

struct test_case {
    const char *name;
    const char *path;
    char *const *argv;
};

static int run_test(const struct test_case *test, char *const envp[]) {
    int pid = fork();
    if (pid < 0) {
        printf("[testall] cannot fork %s\n", test->name);
        return 0;
    }
    if (pid == 0) {
        execve(test->path, test->argv, envp);
        printf("[testall] cannot exec %s\n", test->path);
        exit(127);
    }

    int status = 0;
    int result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -2);

    if (result != pid) {
        printf("[testall] waitpid failed for %s\n", test->name);
        return 0;
    }
    return status == 0;
}

int main(void) {
    char *argtest_argv[] = {"argtest", "hello", "two words", 0};
    char *forktest_argv[] = {"forktest", 0};
    char *fstest_argv[] = {"fstest", 0};
    char *envp[] = {
        "PATH=/bin:/tests",
        "HOME=/",
        "PWD=/",
        0,
    };
    const struct test_case tests[] = {
        {"argtest", "/tests/argtest", argtest_argv},
        {"forktest", "/tests/forktest", forktest_argv},
        {"fstest", "/tests/fstest", fstest_argv},
    };

    puts("MiniOS Test Suite");
    puts("=================");

    int passed = 0;
    int total = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < total; i++) {
        printf("\nRunning %s\n", tests[i].name);
        if (run_test(&tests[i], envp)) {
            passed++;
        } else {
            printf("[testall] %s ........ FAIL\n", tests[i].name);
        }
    }

    printf("\nSummary: %d/%d programs passed\n", passed, total);
    puts(passed == total ? "[ALL TESTS PASSED]" : "[TEST SUITE FAILED]");
    return passed == total ? 0 : 1;
}
