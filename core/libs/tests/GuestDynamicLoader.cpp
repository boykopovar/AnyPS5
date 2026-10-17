#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <stdexcept>
#include <source_location>
#include <thread>
extern "C" {
int APS5_VABI sceKernelDlsym(std::int32_t, const char*, void**);
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
char* APS5_VABI dlerror_nid_postfix();
int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_postfix(const void*);
int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_postfix(const void*);
}
static void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "guest_dynamic_loader: failed at %s:%u\n", location.file_name(), location.line());
    std::abort();
}
extern "C" int APS5_VABI GuestDefaultScopeOnly(int a, int b) { return a * b; }
template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
int main(int argc, char** argv) {
    Require(argc == 2);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix(argv[1], 0x2000) == nullptr);
    Require(dlerror_nid_postfix() != nullptr);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix("anyps5-missing-module-for-test.prx", 2) == nullptr);
    Require(dlerror_nid_postfix() != nullptr);
    void* mainSymbol = nullptr;
    const int mainResult = sceKernelDlsym(0, "GuestDefaultScopeOnly", &mainSymbol);
    std::fprintf(stderr, "sceKernelDlsym(0, GuestDefaultScopeOnly): result=0x%08x, symbol=%p\n", static_cast<unsigned>(mainResult), mainSymbol);
    Require(mainResult == 0 && mainSymbol != nullptr);
    using MainFunction = int (APS5_VABI *)(int, int);
    Require(reinterpret_cast<MainFunction>(mainSymbol)(6, 7) == 42);
    Require(sceKernelDlsym(0, "anyps5_missing_main_symbol", &mainSymbol) == static_cast<int>(0x80020003u));
    dlerror_nid_postfix();
    void* executable = dlopen_nid_postfix(nullptr, 2);
    Require(executable != nullptr && dlclose_nid_postfix(executable) == 0);
    void* module = dlopen_nid_postfix(argv[1], 2 | 0x100);
    Require(module != nullptr);
    using Add = int (APS5_VABI *)(int, int);
    auto add = reinterpret_cast<Add>(dlsym_nid_postfix(module, "GuestModuleAdd"));
    Require(add && add(17, 25) == 42);
    Require(dlsym_nid_postfix(reinterpret_cast<void*>(-2), "GuestModuleAdd") == reinterpret_cast<void*>(add));
    Require(dlsym_nid_postfix(nullptr, "GuestModuleAdd") == reinterpret_cast<void*>(add));
#ifndef _WIN32
    auto mul = reinterpret_cast<Add>(dlsym_nid_postfix(module, "GuestModuleMul"));
    Require(mul && mul(6, 7) == 42);
    auto sub = reinterpret_cast<Add>(dlsym_nid_postfix(module, "GuestModuleSub"));
    Require(sub && sub(50, 8) == 42);
#endif
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

    const std::filesystem::path guest = "anyps5-relinked-module-for-test.prx";
    auto relinked = guest;
    relinked += ".guest.prx";
    {
        std::ofstream elf(guest, std::ios::binary);
        elf.write("\x7f" "ELF", 4);
    }
    std::filesystem::copy_file(argv[1], relinked, std::filesystem::copy_options::overwrite_existing);
    void* redirected = dlopen_nid_postfix(guest.string().c_str(), 2);
    Require(redirected != nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(redirected, "GuestModuleAdd"));
    Require(add && add(40, 2) == 42);
    Require(dlclose_nid_postfix(redirected) == 0);
    std::filesystem::remove(guest);
    std::filesystem::remove(relinked);

    const std::filesystem::path nestedRoot = "anyps5-relinked-prx-dir-for-test";
    const std::filesystem::path nestedGuest = nestedRoot / "Engine.Render.prx";
    const std::filesystem::path nestedRelinked = nestedRoot / "prx" / "Engine.Render.prx.guest.prx";
    std::filesystem::create_directories(nestedRelinked.parent_path());
    {
        std::ofstream elf(nestedGuest, std::ios::binary);
        elf.write("\x7f" "ELF", 4);
    }
    std::filesystem::copy_file(argv[1], nestedRelinked, std::filesystem::copy_options::overwrite_existing);
    void* nested = dlopen_nid_postfix(nestedGuest.string().c_str(), 2);
    Require(nested != nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(nested, "GuestModuleAdd"));
    Require(add && add(20, 22) == 42);
    Require(dlclose_nid_postfix(nested) == 0);
    std::filesystem::remove_all(nestedRoot);

    void* pinned = dlopen_nid_postfix(argv[1], 2);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(pinned, "GuestModuleAdd"));
    Require(add && _sceKernelRtldThreadAtexitIncrement_nid_postfix(reinterpret_cast<const void*>(add)) == 0);
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(reinterpret_cast<const void*>(add)) == 0);
    Require(dlclose_nid_postfix(pinned) == 0);
    Require(add(20, 22) == 42);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(reinterpret_cast<const void*>(add)) == 0);
    Require(add(40, 2) == 42);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(reinterpret_cast<const void*>(add)) == 0);
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(reinterpret_cast<const void*>(&ThrowsRuntimeError<void (*)()>)) == 0);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(reinterpret_cast<const void*>(&ThrowsRuntimeError<void (*)()>)) == 0);
    Require(ThrowsRuntimeError([] { _sceKernelRtldThreadAtexitDecrement_nid_postfix(reinterpret_cast<const void*>(&Require)); }));
    int local = 0;
    Require(ThrowsRuntimeError([&] { _sceKernelRtldThreadAtexitIncrement_nid_postfix(&local); }));
#ifndef _WIN32
    Require(dlsym_nid_postfix(nullptr, "malloc") == nullptr);
    Require(dlsym_nid_postfix(nullptr, "dlopen") == nullptr);
    Require(dlsym_nid_postfix(nullptr, "pthread_create") == nullptr);
    Require(dlsym_nid_postfix(nullptr, "GuestDefaultScopeOnly") == reinterpret_cast<void*>(&GuestDefaultScopeOnly));
    Require(dlsym_nid_postfix(reinterpret_cast<void*>(-2), "GuestDefaultScopeOnly") == reinterpret_cast<void*>(&GuestDefaultScopeOnly));
    const auto hostLoaded = std::filesystem::absolute("anyps5-host-loaded-module-for-test.prx");
    std::filesystem::copy_file(argv[1], hostLoaded, std::filesystem::copy_options::overwrite_existing);
    void* host = ::dlopen(hostLoaded.c_str(), RTLD_NOW | RTLD_GLOBAL);
    Require(host != nullptr);
    void* hostAdd = ::dlsym(host, "3qcG+yBRS+s");
    Require(hostAdd != nullptr);
    Require(dlsym_nid_postfix(nullptr, "GuestModuleAdd") == hostAdd);
    void* hostMul = ::dlsym(host, "GuestModuleMul#guest");
    void* hostSub = ::dlsym(host, "BOyBJaKwOa8#guest");
    Require(hostMul != nullptr && hostSub != nullptr);
    for (void* scope : std::array<void*, 2>{nullptr, reinterpret_cast<void*>(-2)}) {
        auto defaultMul = reinterpret_cast<Add>(dlsym_nid_postfix(scope, "GuestModuleMul"));
        auto defaultSub = reinterpret_cast<Add>(dlsym_nid_postfix(scope, "GuestModuleSub"));
        Require(reinterpret_cast<void*>(defaultMul) == hostMul);
        Require(reinterpret_cast<void*>(defaultSub) == hostSub);
        Require(defaultMul(6, 7) == 42 && defaultSub(50, 8) == 42);
    }
    Require(dlsym_nid_postfix(nullptr, "malloc") == nullptr);
    Require(::dlclose(host) == 0);
#endif
    std::puts("guest_dynamic_loader: all checks passed");
}
