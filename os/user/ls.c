#include "user.h"

int main(int argc, char **argv, char **envp) {
    (void)envp;
    const char *path = argc > 1 ? argv[1] : 0;
    struct dirent_info entries[128];
    int count = getdents(path, entries, 128);
    if (count < 0) {
        term_style_begin(ANSI_RED);
        printf("ls: cannot access %s\n", path ? path : ".");
        term_style_end();
        return 1;
    }
    for (int i = 0; i < count; i++) {
        int type = entries[i].mode & 0xff;
        if (type == MODE_DIR)
            term_style_begin(ANSI_BLUE);
        else if (entries[i].mode & MODE_EXEC)
            term_style_begin(ANSI_GREEN);
        printf("%s", entries[i].name);
        if (type == MODE_DIR)
            printf("/");
        else if (entries[i].mode & MODE_EXEC)
            printf("*");
        if (type == MODE_DIR || (entries[i].mode & MODE_EXEC))
            term_style_end();
        printf("\n");
    }
    return 0;
}
