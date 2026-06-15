#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    const char *path = argc > 1 ? argv[1] : 0;
    struct dirent_info entries[128];
    int count = getdents(path, entries, 128);
    if (count < 0) {
        printf("ls: cannot access %s\n", path ? path : ".");
        return 1;
    }
    for (int i = 0; i < count; i++) {
        printf("%s", entries[i].name);
        if ((entries[i].mode & 0xff) == MODE_DIR)
            printf("/");
        else if (entries[i].mode & MODE_EXEC)
            printf("*");
        printf("\n");
    }
    return 0;
}
