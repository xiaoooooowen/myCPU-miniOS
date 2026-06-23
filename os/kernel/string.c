#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *dst = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;
    for (size_t i = 0; i < length; i++)
        dst[i] = src[i];
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    unsigned char *dst = (unsigned char *)destination;
    for (size_t i = 0; i < length; i++)
        dst[i] = (unsigned char)value;
    return destination;
}

void *memmove(void *destination, const void *source, size_t length) {
    unsigned char *dst = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;
    if (dst < src) {
        for (size_t i = 0; i < length; i++)
            dst[i] = src[i];
    } else if (dst > src) {
        for (size_t i = length; i > 0; i--)
            dst[i - 1] = src[i - 1];
    }
    return destination;
}
