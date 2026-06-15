#include "user.h"

int main(void) {
    char path[256];
    if (getcwd(path, sizeof(path)) < 0)
        return 1;
    puts(path);
    return 0;
}
