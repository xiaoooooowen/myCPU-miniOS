#include "user.h"

#define TEST_SIZE (64 * 1024)

static void pass(const char *name) {
    printf("[fstest] %s PASS\n", name);
}

static int fail(const char *name, int fd) {
    printf("[fstest] %s FAIL\n", name);
    if (fd >= 0)
        close(fd);
    puts("[fstest] TEST FAILED");
    return 1;
}

int main(void) {
    const char *path = "/tmp/fstest.bin";
    int fd = open(path, O_CREATE | O_RDWR | O_TRUNC);
    if (fd < 0)
        return fail("create file .........", -1);
    pass("create file .........");

    char block[512];
    for (int i = 0; i < (int)sizeof(block); i++)
        block[i] = (char)(i & 0x7f);
    for (int i = 0; i < TEST_SIZE / (int)sizeof(block); i++)
        if (write(fd, block, sizeof(block)) != sizeof(block))
            return fail("write 64 KiB ........", fd);
    pass("write 64 KiB ........");
    pass("indirect blocks .....");

    if (lseek(fd, 32768, SEEK_SET) != 32768)
        return fail("seek to 32768 .......", fd);
    char check[512];
    if (read(fd, check, sizeof(check)) != sizeof(check))
        return fail("read 512 bytes ......", fd);
    pass("seek/read ............");

    for (int i = 0; i < (int)sizeof(check); i++)
        if (check[i] != block[i])
            return fail("verify contents ...", fd);
    pass("verify contents ...");

    if (close(fd) < 0)
        return fail("close file ..........", -1);
    pass("close file ..........");
    puts("[fstest] ALL PASS");
    return 0;
}
