#include "user.h"

static const char *state_name(int state) {
    const char *names[] = {"UNUSED", "READY", "RUNNING", "BLOCKED", "ZOMBIE"};
    return state >= 0 && state <= 4 ? names[state] : "UNKNOWN";
}

static int unsigned_width(uint64_t value) {
    int width = 1;
    while (value >= 10) {
        value /= 10;
        width++;
    }
    return width;
}

static void spaces(int count) {
    while (count-- > 0)
        putchar(' ');
}

static void print_int_column(int value, int width) {
    int length = unsigned_width(value < 0 ? (uint64_t)(-(long)value) :
                                (uint64_t)value);
    if (value < 0)
        length++;
    printf("%d", value);
    spaces(width - length);
}

static void print_uint_column(uint64_t value, int width) {
    printf("%lu", value);
    spaces(width - unsigned_width(value));
}

static void print_string_column(const char *value, int width) {
    printf("%s", value);
    spaces(width - (int)strlen(value));
}

int main(void) {
    struct process_info entries[16];
    int count = getprocs(entries, 16);
    if (count < 0)
        return 1;
    puts("PID   PPID  STATE     TICKS     SWITCH    NAME");
    for (int i = 0; i < count; i++) {
        print_int_column(entries[i].pid, 6);
        print_int_column(entries[i].ppid, 6);
        print_string_column(state_name(entries[i].state), 10);
        print_uint_column(entries[i].runtime_ticks, 10);
        print_uint_column(entries[i].context_switches, 10);
        printf("%s\n", entries[i].name);
    }
    return 0;
}
