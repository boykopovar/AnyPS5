#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdexcept>
#include <source_location>
#include <thread>

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
static bool FailsAsUnrelinked(const std::filesystem::path& module) {
    void* handle = dlopen_nid_postfix(module.string().c_str(), 2);
    const char* error = dlerror_nid_postfix();
    return handle == nullptr && error != nullptr && std::strstr(error, module.filename().string().c_str()) != nullptr &&
        std::strstr(error, "--module-dir") != nullptr;
}
static void CheckUnrelinkedModule(const std::filesystem::path& module) {
#ifdef _WIN32
    const UINT processMode = SetErrorMode(0);
    DWORD threadMode = 0;
    const bool threadModeSet = SetThreadErrorMode(SEM_NOGPFAULTERRORBOX, &threadMode) != 0;
#endif
    const bool first = FailsAsUnrelinked(module);
    const bool again = FailsAsUnrelinked(module);
#ifdef _WIN32
    const DWORD threadModeAfter = GetThreadErrorMode();
    SetThreadErrorMode(threadMode, nullptr);
    SetErrorMode(processMode);
    Require(threadModeSet && threadModeAfter == SEM_NOGPFAULTERRORBOX);
#endif
    Require(first && again);
}
#ifdef _WIN32
static void CheckGlobalLookupOrder(const char* path) {
    const std::filesystem::path other = "anyps5-global-order-for-test.guest.prx";
    std::filesystem::copy_file(path, other, std::filesystem::copy_options::overwrite_existing);
    void* local = dlopen_nid_postfix(path, 2);
    void* earlierGlobal = dlopen_nid_postfix(other.string().c_str(), 2 | 0x100);
    void* promoted = dlopen_nid_postfix(path, 2 | 0x100);
    Require(local && earlierGlobal && promoted);
    void* firstSymbol = dlsym_nid_postfix(local, "GuestModuleAdd");
    void* earlierSymbol = dlsym_nid_postfix(earlierGlobal, "GuestModuleAdd");
    Require(firstSymbol && earlierSymbol && firstSymbol != earlierSymbol);
    const std::array<void*, 2> scopes{nullptr, reinterpret_cast<void*>(-2)};
    for (void* scope : scopes) Require(dlsym_nid_postfix(scope, "GuestModuleAdd") == earlierSymbol);
    Require(dlclose_nid_postfix(promoted) == 0);
    for (void* scope : scopes) Require(dlsym_nid_postfix(scope, "GuestModuleAdd") == earlierSymbol);
    Require(dlclose_nid_postfix(local) == 0);
    Require(dlclose_nid_postfix(earlierGlobal) == 0);
    std::filesystem::remove(other);
}
static bool CheckGlobalVisibility(const char* path, bool globalFirst) {
    using Add = int (APS5_VABI *)(int, int);
    const std::array<void*, 2> scopes{nullptr, reinterpret_cast<void*>(-2)};
    const auto requireHidden = [&] {
        for (void* scope : scopes) {
            Require(dlsym_nid_postfix(scope, "GuestModuleAdd") == nullptr);
            Require(dlerror_nid_postfix() != nullptr);
        }
    };
    requireHidden();
    void* first = dlopen_nid_postfix(path, 2 | (globalFirst ? 0x100 : 0));
    Require(first != nullptr);
    if (!globalFirst) requireHidden();
    void* second = dlopen_nid_postfix(path, 2 | (globalFirst ? 0 : 0x100));
    Require(second != nullptr && second != first);
    auto add = reinterpret_cast<Add>(dlsym_nid_postfix(first, "GuestModuleAdd"));
    Require(add && add(19, 23) == 42);
    Require(dlsym_nid_postfix(second, "GuestModuleAdd") == reinterpret_cast<void*>(add));
    for (void* scope : scopes) Require(dlsym_nid_postfix(scope, "GuestModuleAdd") == reinterpret_cast<void*>(add));
    void* global = globalFirst ? first : second;
    void* local = globalFirst ? second : first;
    Require(dlclose_nid_postfix(global) == 0);
    Require(dlsym_nid_postfix(local, "GuestModuleAdd") == reinterpret_cast<void*>(add));
    Require(add(20, 22) == 42);
    bool visible = true;
    for (void* scope : scopes) {
        const bool found = dlsym_nid_postfix(scope, "GuestModuleAdd") == reinterpret_cast<void*>(add);
        if (!found) {
            std::fprintf(stderr, "RTLD_GLOBAL lost after closing its handle: global_first=%d scope=%s; surviving handle returns %d\n",
                globalFirst, scope == nullptr ? "NULL" : "-2", add(20, 22));
            dlerror_nid_postfix();

            visible = false;
        }
    }
    Require(dlclose_nid_postfix(local) == 0);
    requireHidden();
    void* reloaded = dlopen_nid_postfix(path, 2);
    Require(reloaded != nullptr);
    auto reloadedAdd = reinterpret_cast<Add>(dlsym_nid_postfix(reloaded, "GuestModuleAdd"));
    Require(reloadedAdd && reloadedAdd(21, 21) == 42);
    requireHidden();
    Require(dlclose_nid_postfix(reloaded) == 0);
    return visible;
}
#endif
int main(int argc, char** argv) {
    Require(argc == 2);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix(argv[1], 0x2000) == nullptr);
    Require(dlerror_nid_postfix() != nullptr);
    Require(dlerror_nid_postfix() == nullptr);
    Require(dlopen_nid_postfix("anyps5-missing-module-for-test.prx", 2) == nullptr);
    const char* missing = dlerror_nid_postfix();
    Require(missing != nullptr && std::strstr(missing, "--module-dir") == nullptr);
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
#ifdef _WIN32
    CheckGlobalLookupOrder(argv[1]);
    const bool localFirstVisible = CheckGlobalVisibility(argv[1], false);
    const bool globalFirstVisible = CheckGlobalVisibility(argv[1], true);
    Require(localFirstVisible && globalFirstVisible);
#endif
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
    std::filesystem::remove(relinked);
    {
        std::ofstream elf(guest, std::ios::binary);
        elf.write("\x7f" "ELF", 4);
    }
    CheckUnrelinkedModule(guest);
    std::filesystem::copy_file(argv[1], relinked, std::filesystem::copy_options::overwrite_existing);
    void* redirected = dlopen_nid_postfix(guest.string().c_str(), 2);
    Require(redirected != nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(redirected, "GuestModuleAdd"));
    Require(add && add(40, 2) == 42);
    Require(dlclose_nid_postfix(redirected) == 0);
    std::filesystem::remove(guest);
    std::filesystem::remove(relinked);

    const bool createdLibs = std::filesystem::create_directories("libs");
    std::filesystem::copy_file(argv[1], "libs/anyps5-system-module-for-test.prx", std::filesystem::copy_options::overwrite_existing);
    for (const char* path : {"/gkbHtXc3Wq/common/lib/anyps5-system-module-for-test.sprx", "/system/common/lib/anyps5-system-module-for-test.sprx"}) {
        int started = -1;
        const int system = sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, &started);
        Require(system > 0 && started == 0);
        add = reinterpret_cast<Add>(dlsym_nid_postfix(reinterpret_cast<void*>(static_cast<std::intptr_t>(system)), "GuestModuleAdd"));
        Require(add && add(30, 12) == 42);
        Require(sceKernelStopUnloadModule(system, 0, nullptr, 0, nullptr, nullptr) == 0);
    }
    Require(sceKernelLoadStartModule("/gkbHtXc3Wq/common/lib/anyps5-missing-system-module.sprx", 0, nullptr, 0, nullptr, nullptr) == static_cast<int>(0x80020002u));
    Require(sceKernelLoadStartModule("/gkbHtXc3Wq/common/lib/anyps5-system-module-for-test.prx", 0, nullptr, 0, nullptr, nullptr) == static_cast<int>(0x80020002u));
    std::filesystem::copy_file(argv[1], "anyps5-escape-module-for-test.prx", std::filesystem::copy_options::overwrite_existing);
    Require(sceKernelLoadStartModule("/gkbHtXc3Wq/common/lib/../anyps5-escape-module-for-test.sprx", 0, nullptr, 0, nullptr, nullptr) == static_cast<int>(0x80020002u));
    std::filesystem::remove("anyps5-escape-module-for-test.prx");
    std::filesystem::remove("libs/anyps5-system-module-for-test.prx");
    std::error_code notEmpty;
    if (createdLibs) std::filesystem::remove("libs", notEmpty);

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

    const std::filesystem::path layeredRoot = "anyps5-relinked-prx-layers-for-test";
    const std::filesystem::path layeredGuest = layeredRoot / "Engine.Layers.prx";
    std::filesystem::create_directories(layeredRoot / "prx");
    {
        std::ofstream elf(layeredGuest, std::ios::binary);
        elf.write("" "ELF", 4);
        std::ofstream broken(layeredRoot / "prx" / "Engine.Layers.prx.guest.prx", std::ios::binary);
        broken << "not a module";
    }
    std::filesystem::copy_file(argv[1], layeredRoot / "ENGINE.LAYERS.PRX.GUEST.PRX", std::filesystem::copy_options::overwrite_existing);
    void* layered = dlopen_nid_postfix(layeredGuest.string().c_str(), 2);
    Require(layered != nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(layered, "GuestModuleAdd"));
    Require(add && add(19, 23) == 42);
    Require(dlclose_nid_postfix(layered) == 0);
    std::filesystem::remove_all(layeredRoot);

    const std::filesystem::path foldedRoot = "anyps5-relinked-prx-folded-for-test";
    const std::filesystem::path foldedGuest = foldedRoot / "Engine.Folded.prx";
    std::filesystem::create_directories(foldedRoot / "prx");
    {
        std::ofstream elf(foldedGuest, std::ios::binary);
        elf.write("" "ELF", 4);
    }
    std::filesystem::copy_file(argv[1], foldedRoot / "prx" / "ENGINE.FOLDED.PRX.GUEST.PRX", std::filesystem::copy_options::overwrite_existing);
    void* folded = dlopen_nid_postfix(foldedGuest.string().c_str(), 2);
    Require(folded != nullptr);
    add = reinterpret_cast<Add>(dlsym_nid_postfix(folded, "GuestModuleAdd"));
    Require(add && add(10, 32) == 42);
    Require(dlclose_nid_postfix(folded) == 0);
    std::filesystem::remove_all(foldedRoot);

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
    int local = 0;
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(&local) == -1);
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(nullptr) == -1);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(&local) == 0);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(nullptr) == 0);
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
