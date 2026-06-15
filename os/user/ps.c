#include "user.h"

static const char *state_name(int state) {
    const char *names[] = {"UNUSED", "READY", "RUNNING", "BLOCKED", "ZOMBIE"};
    return state >= 0 && state <= 4 ? names[state] : "UNKNOWN";
}

int main(void) {
    struct process_info entries[16];
    int count = getprocs(entries, 16);
    if (count < 0)
        return 1;
    puts("PID PPID STATE    TICKS SWITCH NAME");
    for (int i = 0; i < count; i++)
        printf("%d   %d    %s %lu %lu %s\n",
               entries[i].pid, entries[i].ppid,
               state_name(entries[i].state),
               entries[i].runtime_ticks, entries[i].context_switches,
               entries[i].name);
    return 0;
}
