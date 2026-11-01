#include "fiber_nids.h"

typedef struct {
    _Alignas(8) unsigned char bytes[0x100];
} Fiber;

typedef void (*FiberEntry)(unsigned long argOnInitialize, unsigned long argOnRun);

int guest_printf(const char* format, ...) __asm__(NID_printf);
int guest_fflush(void* stream) __asm__(NID_fflush);
[[noreturn]] void guest_exit(int code) __asm__(NID_exit);
int guest_fiber_initialize(Fiber* fiber, const char* name, FiberEntry entry, unsigned long argument, void* context, unsigned long size, const void* options, unsigned int version) __asm__(NID__sceFiberInitializeImpl);
int guest_fiber_finalize(Fiber* fiber) __asm__(NID_sceFiberFinalize);
int guest_fiber_run(Fiber* fiber, unsigned long argOnRun, unsigned long* argOnReturn) __asm__(NID_sceFiberRun);
int guest_fiber_switch(Fiber* fiber, unsigned long argOnRun, unsigned long* argOnRunOut) __asm__(NID_sceFiberSwitch);
int guest_fiber_return(unsigned long argOnReturn, unsigned long* argOnRun) __asm__(NID_sceFiberReturnToThread);
int guest_fiber_self(Fiber** fiber) __asm__(NID_sceFiberGetSelf);
int guest_pthread_create(void** thread, const void* attr, void* (*entry)(void*), void* argument, const char* name) __asm__(NID_scePthreadCreate);
int guest_pthread_join(void* thread, void** result) __asm__(NID_scePthreadJoin);

enum { ContextSize = 64 << 10, Rounds = 5 };

static Fiber ping, pong, away;
static int created;
static _Alignas(16) unsigned char pingStack[ContextSize];
static _Alignas(16) unsigned char pongStack[ContextSize];
static _Alignas(16) unsigned char awayStack[ContextSize];

static unsigned long mix(unsigned long value, unsigned long salt) {
    return value * 6364136223846793005ul + salt;
}

static void ping_entry(unsigned long argOnInitialize, unsigned long argOnRun) {
    unsigned long local = mix(argOnInitialize, argOnRun);
    unsigned long value = argOnRun;
    for (int round = 0; round < Rounds; ++round) {
        Fiber* self = 0;
        guest_fiber_self(&self);
        guest_printf("ping round %d self %d value %lu\n", round, self == &ping, value);
        unsigned long received = 0;
        guest_fiber_switch(&pong, value + 1, &received);
        value = received;
        local = mix(local, value);
    }
    guest_printf("ping local %lu\n", local % 1000003);
    unsigned long ignored = 0;
    guest_fiber_return(value * 10, &ignored);
}

static void pong_entry(unsigned long argOnInitialize, unsigned long argOnRun) {
    double scale = (double)argOnInitialize;
    unsigned long value = argOnRun;
    for (;;) {
        guest_printf("pong value %lu scale %.1f\n", value, scale);
        scale *= 2.0;
        unsigned long received = 0;
        guest_fiber_switch(&ping, value * 2, &received);
        value = received;
    }
}

static void away_entry(unsigned long argOnInitialize, unsigned long argOnRun) {
    unsigned long received = 0;
    guest_fiber_return(argOnInitialize + argOnRun, &received);
    guest_printf("away resumed with %lu\n", received);
    guest_fiber_return(received, &received);
}

static void* resume_away(void* argument) {
    while (!__atomic_load_n(&created, __ATOMIC_ACQUIRE)) {
    }
    unsigned long returned = 0;
    const int run = guest_fiber_run(&away, (unsigned long)argument, &returned);
    guest_printf("run away on another thread %d returned %lu\n", run, returned);
    return 0;
}

int main(void) {
    guest_printf("init ping %d\n", guest_fiber_initialize(&ping, "ping", ping_entry, 7, pingStack, ContextSize, 0, 0));
    guest_printf("init pong %d\n", guest_fiber_initialize(&pong, "pong", pong_entry, 2, pongStack, ContextSize, 0, 0));
    unsigned long returned = 0;
    const int run = guest_fiber_run(&ping, 1, &returned);
    guest_printf("run %d returned %lu\n", run, returned);
    Fiber* self = 0;
    guest_printf("self outside %x\n", (unsigned)guest_fiber_self(&self));
    guest_printf("finalize pong %x\n", (unsigned)guest_fiber_finalize(&pong));
    guest_printf("finalize ping %d\n", guest_fiber_finalize(&ping));
    guest_printf("init away %d\n", guest_fiber_initialize(&away, "away", away_entry, 3, awayStack, ContextSize, 0, 0));
    const int awayRun = guest_fiber_run(&away, 4, &returned);
    guest_printf("run away %d returned %lu\n", awayRun, returned);
    void* thread = 0;
    guest_printf("create %d\n", guest_pthread_create(&thread, 0, resume_away, (void*)5, "away"));
    guest_fflush(0);
    __atomic_store_n(&created, 1, __ATOMIC_RELEASE);
    guest_pthread_join(thread, 0);
    return 0;
}

void start_main(void) {
    guest_exit(main());
}

__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call start_main\n"
        "    ud2\n");
