#include "kernel_nids.h"

int guest_printf(const char* format, ...) __asm__(NID_printf);
[[noreturn]] void guest_exit(int code) __asm__(NID_exit);
int guest_pthread_create(void** thread, const void* attr, void* (*entry)(void*), void* argument, const char* name) __asm__(NID_scePthreadCreate);
int guest_pthread_join(void* thread, void** result) __asm__(NID_scePthreadJoin);
[[noreturn]] void guest_pthread_exit(void* result) __asm__(NID_scePthreadExit);
int guest_attr_init(void** attr) __asm__(NID_scePthreadAttrInit);
int guest_attr_destroy(void** attr) __asm__(NID_scePthreadAttrDestroy);
int guest_attr_set_stack_size(void** attr, unsigned long size) __asm__(NID_scePthreadAttrSetstacksize);
int guest_attr_get(void* thread, void** attr) __asm__(NID_scePthreadAttrGet);
int guest_attr_get_stack(void** attr, void** address, unsigned long* size) __asm__(NID_scePthreadAttrGetstack);
void* guest_pthread_self(void) __asm__(NID_scePthreadSelf);
int guest_install_exception_handler(int signal, void (*handler)(int, void*)) __asm__(NID_sceKernelInstallExceptionHandler);
int guest_remove_exception_handler(int signal) __asm__(NID_sceKernelRemoveExceptionHandler);
int guest_raise_exception(void* thread, int signal) __asm__(NID_sceKernelRaiseException);
int guest_load_start_module(const char* name, unsigned long args, const void* argp, unsigned flags, const void* option, int* result) __asm__(NID_sceKernelLoadStartModule);
int guest_dlsym(int module, const char* symbol, void** address) __asm__(NID_sceKernelDlsym);
int guest_mutex_init(void** mutex, const void* attr, const char* name) __asm__(NID_scePthreadMutexInit);
int guest_mutex_lock(void** mutex) __asm__(NID_scePthreadMutexLock);
int guest_mutex_unlock(void** mutex) __asm__(NID_scePthreadMutexUnlock);
int guest_mutex_destroy(void** mutex) __asm__(NID_scePthreadMutexDestroy);
int guest_usleep(unsigned int microseconds) __asm__(NID_sceKernelUsleep);
unsigned long guest_process_time(void) __asm__(NID_sceKernelGetProcessTime);
unsigned long guest_read_tsc(void) __asm__(NID_sceKernelReadTsc);
unsigned long guest_tsc_frequency(void) __asm__(NID_sceKernelGetTscFrequency);
void (*guest_signal(int number, void (*handler)(int)))(int) __asm__(NID_signal);
int guest_raise(int number) __asm__(NID_raise);
int guest_sigprocmask(int how, const unsigned* set, unsigned* old) __asm__(NID_sigprocmask);
[[gnu::returns_twice]] int guest_sigsetjmp(long* buffer, long save_mask) __asm__(NID_sigsetjmp);
[[noreturn]] void guest_siglongjmp(long* buffer, int value) __asm__(NID_siglongjmp);
int guest_attr_set_create_suspend(void** attr) __asm__(NID_scePthreadAttrSetcreatesuspend);
int guest_pthread_resume(void* thread) __asm__(NID_scePthreadResume);
int guest_mprotect(const void* address, unsigned long length, int protection) __asm__(NID_sceKernelMprotect);

struct module_segment {
    unsigned long address;
    unsigned size;
    int prot;
};

struct module_info {
    unsigned long st_size;
    char name[256];
    int id;
    unsigned tls_index;
    unsigned long tls_init_address;
    unsigned tls_init_size, tls_size, tls_offset, tls_align;
    unsigned long init_address, fini_address, reserved[2], eh_frame_hdr_address, eh_frame_address;
    unsigned eh_frame_hdr_size, eh_frame_size;
    struct module_segment segments[4];
    unsigned segment_count, ref_count;
};
_Static_assert(__builtin_offsetof(struct module_info, segments) == 0x160, "ModuleInfoEx layout");

struct unwind_info {
    unsigned long st_size;
    char name[256];
    unsigned long eh_frame_hdr_address, eh_frame_address, eh_frame_size, first_segment_address, first_segment_size;
};

int guest_module_info_from_address(unsigned long address, int flags, struct module_info* info) __asm__(NID_sceKernelGetModuleInfoFromAddr);

void unwound_function(void);
__asm__(".text\n"
        ".globl unwound_function\n"
        ".type unwound_function, @function\n"
        "unwound_function:\n"
        "    .cfi_startproc\n"
        "    ret\n"
        "    .cfi_endproc\n"
        ".size unwound_function, .-unwound_function\n");
int guest_module_info_for_unwind(unsigned long address, int flags, struct unwind_info* info) __asm__(NID_sceKernelGetModuleInfoForUnwind);

static void interrupted(int number) {
    (void)number;
}

enum { Threads = 4, Iterations = 10000 };

static const unsigned long ApplicationAreaEnd = 0xfc00000000ul;

static void* mutex;
static volatile long counter;

static void* worker(void* argument) {
    for (int i = 0; i < Iterations; ++i) {
        guest_mutex_lock(&mutex);
        counter = counter + 1;
        guest_mutex_unlock(&mutex);
    }
    return (void*)((long)argument * 3);
}

static void* exiting_worker(void* argument) {
    guest_pthread_exit((void*)((long)argument + 70));
}

enum { LargeStack = 32 << 20, Levels = 5000 };

[[gnu::noinline]] static long reach(long depth, volatile char* above) {
    volatile char frame[4096];
    frame[0] = above[0];
    if (depth == 0) return frame[0];
    return reach(depth - 1, frame) + 1;
}

static long stack_reported;

static void* deep_worker(void* argument) {
    (void)argument;
    volatile char top = 0;
    void* attr = 0;
    void* address = 0;
    unsigned long size = 0;
    guest_attr_init(&attr);
    guest_attr_get(guest_pthread_self(), &attr);
    guest_attr_get_stack(&attr, &address, &size);
    guest_attr_destroy(&attr);
    stack_reported = size >= LargeStack && (volatile char*)address <= &top && &top < (volatile char*)address + size;
    return (void*)reach(Levels, &top);
}

enum { RaisedSignal = 30, InterruptedRipOffset = 0xe0, LargeFrame = 1 << 18 };

static volatile int interrupt_ready;
static volatile int interrupt_handled;
static volatile unsigned long interrupted_rip;

static void on_exception(int signal, void* context) {
    (void)signal;
    interrupted_rip = *(volatile unsigned long*)((char*)context + InterruptedRipOffset);
    interrupt_handled = 1;
}

[[gnu::noinline]] static long spin_in_large_frame(void) {
    volatile char frame[LargeFrame];
    for (int i = 0; i < LargeFrame; ++i) frame[i] = (char)i;
    interrupt_ready = 1;
    while (!interrupt_handled) {
    }
    long damaged = 0;
    for (int i = 0; i < LargeFrame; ++i) damaged += frame[i] != (char)i;
    return damaged;
}

[[clang::disable_tail_calls]] static void* interrupted_worker(void* argument) {
    (void)argument;
    return (void*)spin_in_large_frame();
}

static volatile int suspended_handled;
static void* volatile suspended_handler_thread;

static void on_suspended_exception(int signal, void* context) {
    (void)signal;
    (void)context;
    suspended_handler_thread = guest_pthread_self();
    suspended_handled = 1;
}

static void* suspended_worker(void* argument) {
    (void)argument;
    return (void*)(long)suspended_handled;
}

enum { SigBlock = 1, SigSetmask = 3, Sigusr1 = 30 };

static long jump_buffer[12];
static const unsigned empty_set[4];
static const unsigned usr1_set[4] = {1u << (Sigusr1 - 1)};

static int sigsetjmp_keeps_blocked_set(void) {
    guest_sigprocmask(SigSetmask, empty_set, 0);
    if (guest_sigsetjmp(jump_buffer, 1l << 32) == 0) {
        guest_sigprocmask(SigBlock, usr1_set, 0);
        guest_siglongjmp(jump_buffer, 1);
    }
    unsigned blocked[4];
    guest_sigprocmask(SigBlock, 0, blocked);
    guest_sigprocmask(SigSetmask, empty_set, 0);
    return (blocked[0] >> (Sigusr1 - 1)) & 1;
}

enum { ProtectReadWrite = 3 };

extern const char etext[];

static unsigned long rdtsc(void) {
    unsigned int low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return (unsigned long)high << 32 | low;
}

int main(void) {
    guest_printf("mutex init %d\n", guest_mutex_init(&mutex, 0, "counter"));
    void* threads[Threads];
    for (long i = 0; i < Threads; ++i) {
        const int created = guest_pthread_create(&threads[i], 0, worker, (void*)(i + 1), "worker");
        if (created != 0) guest_printf("create %ld failed 0x%x\n", i, created);
    }
    long sum = 0;
    for (int i = 0; i < Threads; ++i) {
        void* result = 0;
        guest_pthread_join(threads[i], &result);
        sum += (long)result;
    }
    guest_printf("counter %ld results %ld\n", counter, sum);
    guest_printf("mutex destroy %d\n", guest_mutex_destroy(&mutex));
    int guest_handles = (unsigned long)guest_pthread_self() < ApplicationAreaEnd;
    for (int i = 0; i < Threads; ++i) guest_handles &= threads[i] != 0 && (unsigned long)threads[i] < ApplicationAreaEnd;
    guest_printf("thread handles are guest addresses %d\n", guest_handles);

    void* exiting = 0;
    void* exited = 0;
    guest_pthread_create(&exiting, 0, exiting_worker, (void*)7, "exiting");
    guest_printf("scePthreadExit join %d result %ld\n", guest_pthread_join(exiting, &exited), (long)exited);

    void* large = 0;
    void* large_attr = 0;
    void* levels = 0;
    guest_attr_init(&large_attr);
    guest_attr_set_stack_size(&large_attr, LargeStack);
    guest_pthread_create(&large, &large_attr, deep_worker, 0, "deep");
    guest_attr_destroy(&large_attr);
    guest_printf("32 MiB stack join %d levels %ld, reported stack holds it %ld\n", guest_pthread_join(large, &levels), (long)levels, stack_reported);

    void* target = 0;
    void* target_attr = 0;
    void* damaged = 0;
    guest_install_exception_handler(RaisedSignal, on_exception);
    guest_attr_init(&target_attr);
    guest_attr_set_stack_size(&target_attr, 1 << 20);
    guest_pthread_create(&target, &target_attr, interrupted_worker, 0, "interrupted");
    guest_attr_destroy(&target_attr);
    while (!interrupt_ready) guest_usleep(1000);
    const int raised = guest_raise_exception(target, RaisedSignal);
    guest_pthread_join(target, &damaged);
    guest_remove_exception_handler(RaisedSignal);
    guest_printf("raised %d, frame intact %d, rip after the last call %d\n", raised, damaged == 0, interrupted_rip - (unsigned long)interrupted_worker < 0x100);

    void* suspended = 0;
    void* suspended_attr = 0;
    void* handled_first = 0;
    guest_install_exception_handler(RaisedSignal, on_suspended_exception);
    guest_attr_init(&suspended_attr);
    guest_attr_set_create_suspend(&suspended_attr);
    guest_pthread_create(&suspended, &suspended_attr, suspended_worker, 0, "suspended");
    guest_attr_destroy(&suspended_attr);
    const int raised_suspended = guest_raise_exception(suspended, RaisedSignal);
    const int resumed = guest_pthread_resume(suspended);
    guest_pthread_join(suspended, &handled_first);
    guest_remove_exception_handler(RaisedSignal);
    guest_printf("raised at a suspended thread %d, resumed %d, handled on it before its entry %d\n", raised_suspended, resumed,
                 (long)handled_first == 1 && suspended_handler_thread == suspended);

    int started = -1;
    const int module = guest_load_start_module("/app0/libkernelmodule.so", 0, 0, 0, 0, &started);
    void* answer = 0;
    const int found = guest_dlsym(module, "kernel_module_answer", &answer);
    void* missing = 0;
    const int absent = guest_dlsym(module, "kernel_module_absent", &missing);
    guest_printf("module loaded %d started %d, dlsym %d answer %ld, absent symbol %d\n", module > 0, started, found, answer ? ((long (*)(long))answer)(6) : -1,
                 absent != 0 && missing == 0);
    const int recased = guest_load_start_module("/app0/LibKernelModule.SO", 0, 0, 0, 0, &started);
    void* recased_answer = 0;
    guest_printf("module found under another case %d, same answer %d\n", recased > 0,
                 guest_dlsym(recased, "kernel_module_answer", &recased_answer) == 0 && recased_answer == answer);

    const unsigned long code = (unsigned long)&unwound_function;
    struct module_info module_info;
    module_info.st_size = sizeof(module_info);
    const int queried = guest_module_info_from_address(code, 1, &module_info);
    int contains = 0;
    for (unsigned index = 0; index < module_info.segment_count && index < 4; ++index) {
        const struct module_segment* segment = &module_info.segments[index];
        if (code >= segment->address && code - segment->address < segment->size) contains = 1;
    }
    struct unwind_info unwind_info;
    unwind_info.st_size = sizeof(unwind_info);
    const int unwound = guest_module_info_for_unwind(code, 0, &unwind_info);
    guest_printf("module of the executable's code %d, holds it %d, unwind tables %d %d\n", queried, contains, unwound,
                 unwind_info.eh_frame_hdr_address == module_info.eh_frame_hdr_address && unwind_info.eh_frame_hdr_address != 0 && unwind_info.eh_frame_size != 0);

    const unsigned long before = guest_process_time();
    guest_usleep(20000);
    const unsigned long after = guest_process_time();
    guest_printf("slept %s\n", after - before >= 20000 ? "at least 20 ms" : "too briefly");

    const unsigned long frequency = guest_tsc_frequency();
    const unsigned long first = guest_read_tsc();
    const unsigned long middle = rdtsc();
    const unsigned long last = guest_read_tsc();
    guest_printf("tsc %s, frequency %s\n", first <= middle && middle <= last ? "ordered" : "out of order", frequency > 1000000 ? "plausible" : "implausible");
    guest_signal(2, interrupted);
    guest_printf("signal returns the guest's handler %d\n", guest_signal(2, (void (*)(int))0) == interrupted);
    guest_printf("ignoring SIGPIPE replaces the default %d\n", guest_signal(13, (void (*)(int))1) == 0);
    guest_printf("raising an ignored SIGPIPE returns %d\n", guest_raise(13));
    guest_printf("signal reports SIG_IGN %d\n", guest_signal(13, (void (*)(int))0) == (void (*)(int))1);
    guest_printf("siglongjmp without a saved mask keeps the blocked set %d\n", sigsetjmp_keeps_blocked_set());
    guest_mprotect(etext, 1, ProtectReadWrite);
    guest_printf("making the page past the code's end writable went unnoticed\n");
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
