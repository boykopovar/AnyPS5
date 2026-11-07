#include "host.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

long host_write(const char* text, unsigned long length) {
    return write(1, text, length);
}

void host_exit(int code) {
    _exit(code);
}

unsigned long host_ticks(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (unsigned long)time.tv_sec * 1000000000ul + (unsigned long)time.tv_nsec;
}
