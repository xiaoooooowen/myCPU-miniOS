#include "user.h"

int main(void) {
    puts("[spin] running; use ps and kill PID");
    volatile uint64_t value = 0;
    for (;;)
        value = (value << 1) ^ (value + 1);
}
