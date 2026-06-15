#include "user.h"

int main(void) {
    volatile uint64_t value = 0;
    for (;;)
        value = (value << 1) ^ (value + 1);
}
