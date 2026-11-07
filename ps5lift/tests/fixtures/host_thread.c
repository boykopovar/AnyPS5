#include "host.h"

#include <pthread.h>
#include <stdlib.h>

struct Thread {
    pthread_t handle;
    long (*entry)(long);
    long argument;
    long result;
};

static void* run(void* argument) {
    struct Thread* thread = argument;
    thread->result = thread->entry(thread->argument);
    return 0;
}

unsigned long host_thread_create(long (*entry)(long), long argument) {
    struct Thread* thread = malloc(sizeof(*thread));
    thread->entry = entry;
    thread->argument = argument;
    if (pthread_create(&thread->handle, 0, run, thread) != 0) abort();
    return (unsigned long)thread;
}

long host_thread_join(unsigned long handle) {
    struct Thread* thread = (struct Thread*)handle;
    pthread_join(thread->handle, 0);
    long result = thread->result;
    free(thread);
    return result;
}
