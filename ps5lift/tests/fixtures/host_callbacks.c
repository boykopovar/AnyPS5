#include "host.h"

#include <pthread.h>
#include <stdlib.h>

void host_sort(void* base, unsigned long count, unsigned long size, int (*compare)(const void*, const void*)) {
    qsort(base, count, size, compare);
}

double host_apply(double (*function)(double, long), double value, long count) {
    return function(function(value, count), count + 1);
}

struct ThreadCall {
    long (*function)(long);
    long argument;
    long result;
};

static void* run(void* argument) {
    struct ThreadCall* call = argument;
    call->result = call->function(call->argument);
    return 0;
}

long host_on_thread(long (*function)(long), long argument) {
    struct ThreadCall call = {function, argument, 0};
    pthread_t thread;
    if (pthread_create(&thread, 0, run, &call) != 0) abort();
    pthread_join(thread, 0);
    return call.result;
}

static void* keep_environment(void* argument) {
    struct ThreadCall* call = argument;
    const unsigned before = __builtin_ia32_stmxcsr() & ~0x3fu;
    call->function(call->argument);
    call->result = (__builtin_ia32_stmxcsr() & ~0x3fu) == before;
    return 0;
}

long host_thread_keeps_environment(long (*function)(long), long argument) {
    struct ThreadCall call = {function, argument, 0};
    pthread_t thread;
    if (pthread_create(&thread, 0, keep_environment, &call) != 0) abort();
    pthread_join(thread, 0);
    return call.result;
}

struct ExitCall {
    struct ThreadCall call;
    pthread_key_t key;
};

static void call_at_exit(void* argument) {
    struct ThreadCall* call = argument;
    call->result = call->function(call->argument);
}

static void* exit_after_call(void* argument) {
    struct ExitCall* exit = argument;
    if (pthread_setspecific(exit->key, &exit->call) != 0) abort();
    exit->call.function(0);
    return 0;
}

long host_call_at_thread_exit(long (*function)(long), long argument) {
    struct ExitCall exit = {{function, argument, 0}, 0};
    if (pthread_key_create(&exit.key, call_at_exit) != 0) abort();
    pthread_t thread;
    if (pthread_create(&thread, 0, exit_after_call, &exit) != 0) abort();
    pthread_join(thread, 0);
    pthread_key_delete(exit.key);
    return exit.call.result;
}

long host_stack_guarded(void) {
    return 1;
}

long host_walk_then_take_stack(long (*function)(long), long argument) {
    return host_on_thread(function, argument);
}

long host_call_eight(long (*function)(long, long, long, long, long, long, long, long)) {
    return function(1, 2, 3, 4, 5, 6, 7, 8) + function(10, 20, 30, 40, 50, 60, 70, 80);
}

long host_call_recorded(const struct RecordedCalls* calls, long base) {
    return calls->integers(base, base + 1, base + 2, base + 3, base + 4, base + 5, base + 6, base + 7) + (long)(calls->mixed(1.5, base, 0.25) * 1000.0) + calls->unaligned(base, 3);
}
