#include "host.h"

long host_write(const char* text, unsigned long length) {
    long result;
    __asm__ volatile("syscall" : "=a"(result) : "a"(1), "D"(1), "S"(text), "d"(length) : "rcx", "r11", "memory");
    return result;
}

void host_exit(int code) {
    __asm__ volatile("syscall" : : "a"(231), "D"(code) : "rcx", "r11", "memory");
    __builtin_unreachable();
}

unsigned long host_ticks(void) {
    struct {
        long seconds;
        long nanoseconds;
    } time;
    long result;
    __asm__ volatile("syscall" : "=a"(result) : "a"(228), "D"(1), "S"(&time) : "rcx", "r11", "memory");
    return (unsigned long)(time.seconds * 1000000000l + time.nanoseconds);
}
