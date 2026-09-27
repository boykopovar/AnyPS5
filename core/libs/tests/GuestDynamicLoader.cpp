#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <thread>
extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
char* APS5_VABI dlerror_nid_postfix();
int APS5_VABI sceKernelDlsym(KernelModule, const char*, void**);
}
static void Require(bool value) { if (!value) std::abort(); }
int main(int argc, char** argv) {
    Require(argc == 2);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix(argv[1], 0x2000) == nullptr);
    Require(dlerror_nid_postfix() != nullptr);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix("anyps5-missing-module-for-test.prx", 2) == nullptr);
    Require(dlerror_nid_postfix() != nullptr);
    void* executable = dlopen_nid_postfix(nullptr, 2);
    Require(executable != nullptr && dlclose_nid_postfix(executable) == 0);
    void* module = dlopen_nid_postfix(argv[1], 2 | 0x100);
    Require(module != nullptr);
    using Add = int (APS5_VABI *)(int, int);
    auto add = reinterpret_cast<Add>(dlsym_nid_postfix(module, "GuestModuleAdd"));
    Require(add && add(17, 25) == 42);
    void* resolved = nullptr;
    Require(sceKernelDlsym(1, "sceKernelDlsym", &resolved) == 0 && resolved != nullptr);
    resolved = nullptr;
    Require(sceKernelDlsym(0x2001, "sceKernelDlsym", &resolved) == 0 && resolved != nullptr);
    resolved = reinterpret_cast<void*>(1);
    Require(sceKernelDlsym(1, "anyps5_missing_symbol", &resolved) != 0 && resolved == nullptr);
    Require(sceKernelDlsym(1, "sceKernelDlsym", nullptr) != 0);
    resolved = nullptr;
    Require(sceKernelDlsym(static_cast<KernelModule>(reinterpret_cast<std::uintptr_t>(module)),
                           "GuestModuleAdd", &resolved) == 0 && resolved == reinterpret_cast<void*>(add));
    Require(dlsym_nid_postfix(reinterpret_cast<void*>(-2), "GuestModuleAdd") == reinterpret_cast<void*>(add));
    Require(dlsym_nid_postfix(module, "missing_symbol") == nullptr);
    std::thread other([] { Require(dlerror_nid_postfix() == nullptr); });
    other.join();
    Require(dlerror_nid_postfix() != nullptr);
    Require(dlerror_nid_postfix() == nullptr);
    void* second = dlopen_nid_postfix(argv[1], 1);
    Require(second && second != module);
    Require(dlclose_nid_postfix(module) == 0);
    Require(dlsym_nid_postfix(module, "GuestModuleAdd") == nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(second, "GuestModuleAdd"));
    Require(add && add(2, 3) == 5);
    Require(dlclose_nid_postfix(second) == 0);
    Require(dlclose_nid_postfix(second) == -1);
}
