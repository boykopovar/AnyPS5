#include "loader_nids.h"

int guest_printf(const char* format, ...) __asm__(NID_printf);
[[noreturn]] void guest_exit(int code) __asm__(NID_exit);
void* guest_dlopen(const char* path, int flags) __asm__(NID_dlopen);
int guest_executable_module(void) __asm__(NID_sceKernelGetExecutableModuleHandle);
int guest_load_start_module(const char* name, unsigned long args, const void* argp, unsigned flags, const void* option, int* result) __asm__(NID_sceKernelLoadStartModule);
int guest_dlsym(int module, const char* symbol, void** address) __asm__(NID_sceKernelDlsym);

enum { RtldNow = 2 };

static long origin(int module) {
    const long* value = 0;
    return guest_dlsym(module, "loader_origin", (void**)&value) == 0 ? *value : -1;
}

int main(void) {
    void* self = guest_dlopen(0, RtldNow);
    guest_printf("dlopen(NULL) is the executable's module %d\n", self != 0 && (int)(long)self == guest_executable_module());

    int started = -1;
    const int title = guest_load_start_module("/app0/sce_module/libSceLoaderFixture.prx", 0, 0, 0, 0, &started);
    const int system = guest_load_start_module("/system/common/lib/libSceLoaderFixture.sprx", 0, 0, 0, 0, &started);
    guest_printf("title module %ld, system library %ld, distinct handles %d\n", origin(title), origin(system), title > 0 && system > 0 && title != system);
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
