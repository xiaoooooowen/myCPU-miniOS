#ifndef MINIOS_TRAP_H
#define MINIOS_TRAP_H

#include <stdint.h>

void trap_handler(uint64_t *trap_frame);
void trap_set_silent(int silent);

#endif /* MINIOS_TRAP_H */
