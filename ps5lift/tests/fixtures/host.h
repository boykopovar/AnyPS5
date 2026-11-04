#ifndef PS5LIFT_FIXTURE_HOST_H
#define PS5LIFT_FIXTURE_HOST_H

long host_write(const char* text, unsigned long length);
[[noreturn]] void host_exit(int code);
unsigned long host_ticks(void);
unsigned long host_thread_create(long (*entry)(long), long argument);
long host_thread_join(unsigned long thread);
void host_sort(void* base, unsigned long count, unsigned long size, int (*compare)(const void*, const void*));
double host_apply(double (*function)(double, long), double value, long count);
long host_on_thread(long (*function)(long), long argument);
long host_thread_keeps_environment(long (*function)(long), long argument);
long host_call_at_thread_exit(long (*function)(long), long argument);
long host_walk_then_take_stack(long (*function)(long), long argument);
long host_stack_guarded(void);
long host_call_eight(long (*function)(long, long, long, long, long, long, long, long));
struct RecordedCalls {
    long (*integers)(long, long, long, long, long, long, long, long);
    double (*mixed)(double, long, double);
    long (*unaligned)(long, long);
};
long host_call_recorded(const struct RecordedCalls* calls, long base);
long double host_extended(long a, long b, long c, long d, long e, long f, long g, long double x, long double y);

#endif
