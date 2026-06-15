#include "user.h"

#define TEST_SIZE (64 * 1024)

int main(void) {
    const char *path = "/tmp/fstest.bin";
    int fd = open(path, O_CREATE | O_RDWR | O_TRUNC);
    if (fd < 0)
        goto fail;
    char block[512];
    for (int i = 0; i < (int)sizeof(block); i++)
        block[i] = (char)(i & 0x7f);
    for (int i = 0; i < TEST_SIZE / (int)sizeof(block); i++)
        if (write(fd, block, sizeof(block)) != sizeof(block))
            goto fail_close;
    if (lseek(fd, 32768, SEEK_SET) != 32768)
        goto fail_close;
    char check[512];
    if (read(fd, check, sizeof(check)) != sizeof(check))
        goto fail_close;
    for (int i = 0; i < (int)sizeof(check); i++)
        if (check[i] != block[i])
            goto fail_close;
    close(fd);
    puts("[fstest] PASS (64 KiB + seek)");
    return 0;
fail_close:
    close(fd);
fail:
    puts("[fstest] FAIL");
    return 1;
}
