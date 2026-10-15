#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/Module/ModuleArgs.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <condition_variable>
#include <filesystem>
#include <cstdio>
#include <map>
#include <memory>
#include <limits>
#include <mutex>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#else
#include <link.h>
#endif
#endif

#ifdef _WIN32
extern "C" void ForgetUnloadedUnwindImages_nid_no_patch();
#endif

namespace {
thread_local std::array<char, 512> loaderError{};
thread_local bool pendingError = false;
void Error(const char* message) {
    std::snprintf(loaderError.data(), loaderError.size(), "%s", message);
    pendingError = true;
}
struct Module {
    void* native = nullptr;
    bool owned = true;
    bool global = false;
    ~Module() {
        if (owned && native) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(native));
            ForgetUnloadedUnwindImages_nid_no_patch();
#else
            ::dlclose(native);
#endif
        }
    }
};
std::mutex modulesMutex;
std::map<std::uintptr_t, std::shared_ptr<Module>> modules;
std::uintptr_t nextHandle = 0x20000000;
std::map<const void*, std::uintptr_t> imageIds;
void* Symbol(Module& module, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module.native), name));
#else
    return ::dlsym(module.native, name);
#endif
}
#ifndef _WIN32
#ifdef __APPLE__
std::uintptr_t MainProgramBase() {
    static const std::uintptr_t base = [] {
        for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
            const auto* header = _dyld_get_image_header(index);
            if (header != nullptr && header->filetype == MH_EXECUTE) return reinterpret_cast<std::uintptr_t>(header);
        }
        return std::uintptr_t{0};
    }();
    return base;
}

bool FromGuestModule(void* address) {
    Dl_info info{};
    if (address == nullptr || !::dladdr(address, &info) || info.dli_fname == nullptr) return false;
    if (reinterpret_cast<std::uintptr_t>(info.dli_fbase) == MainProgramBase()) return true;
    const std::string path(info.dli_fname);
    return path.ends_with(".prx");
}
#else
const link_map* MainProgramMap() {
    static const link_map* const map = [] {
        void* handle = ::dlopen(nullptr, RTLD_LAZY);
        link_map* found = nullptr;
        const bool resolved = handle != nullptr && ::dlinfo(handle, RTLD_DI_LINKMAP, &found) == 0 && found != nullptr;
        if (handle != nullptr) ::dlclose(handle);
        if (!resolved) throw std::runtime_error("dlsym: cannot find the link map of the main program");
        return found;
    }();
    return map;
}

bool FromGuestModule(void* address) {
    Dl_info info{};
    link_map* image = nullptr;
    if (address == nullptr || !::dladdr1(address, &info, reinterpret_cast<void**>(&image), RTLD_DL_LINKMAP) || info.dli_fname == nullptr || image == nullptr) return false;
    if (image == MainProgramMap()) return true;
    const std::string path(info.dli_fname);
    return path.ends_with(".prx");
}
#endif

void* DefaultScopeSymbol(const char* symbolName) {
    auto* result = ::dlsym(RTLD_DEFAULT, symbolName);
    return FromGuestModule(result) ? result : nullptr;
}
#endif

void* FindSymbol(Module& module, const char* name) {
    if (auto* symbol = Symbol(module, name)) return symbol;
    const auto nid = Nid::ComputeNid(name, "");
#ifdef _WIN32
    return Symbol(module, nid.c_str());
#else
    if (auto* symbol = Symbol(module, nid.c_str())) return symbol;
    constexpr char guestSuffix[] = "#guest";
    if (auto* symbol = Symbol(module, (std::string(name) + guestSuffix).c_str())) return symbol;
    return Symbol(module, (nid + guestSuffix).c_str());
#endif
}
}

extern "C" {
char* APS5_VABI dlerror_nid_postfix() {
    if (!pendingError) return nullptr;
    pendingError = false;
    return loaderError.data();
}
#ifdef _WIN32
static void InitializeDeferredModule(HMODULE native) {
    using Entry = int (APS5_VABI *)(std::size_t, const void*, void*);
    using Initializer = void (APS5_VABI *)(int, char**, char**);
    constexpr auto initializing = std::numeric_limits<std::uint32_t>::max();
    static std::mutex initializedLock;
    static std::condition_variable initializedChanged;
    auto* table = reinterpret_cast<std::uint32_t*>(GetProcAddress(native, "__aps5_guest_initialize"));
    if (!table) return;
    const auto owner = GetCurrentThreadId();
    std::unique_lock lock(initializedLock);
    while (table[1] == initializing) {
        if (table[0] == 0) throw std::runtime_error("dlopen: deferred module initialization failed");
        if (table[0] == owner) return;
        initializedChanged.wait(lock);
    }
    const auto entry = table[0];
    const auto count = table[1];
    if (entry == 0 && count == 0) return;
    DWORD protection = 0;
    if (!VirtualProtect(table, 2 * sizeof(*table), PAGE_READWRITE, &protection))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "open deferred module initialization metadata");
    table[0] = owner;
    table[1] = initializing;
    lock.unlock();
    const auto finish = [&](bool succeeded) {
        lock.lock();
        table[0] = 0;
        table[1] = succeeded ? 0 : initializing;
        DWORD previous = 0;
        const bool restored = VirtualProtect(table, 2 * sizeof(*table), protection, &previous);
        const auto error = restored ? ERROR_SUCCESS : GetLastError();
        if (!restored) table[1] = initializing;
        lock.unlock();
        initializedChanged.notify_all();
        if (!restored) throw std::system_error(static_cast<int>(error), std::system_category(), "restore deferred module initialization metadata");
    };
    try {
        auto* base = reinterpret_cast<std::uint8_t*>(native);
        const auto* pending = static_cast<const PendingModuleArgs*>(__aps5_get_pending_module_args_nid_no_patch());
        if (entry != 0) __aps5_set_module_init_result_nid_no_patch(reinterpret_cast<Entry>(base + entry)(pending->args, pending->argp, nullptr));
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto initializer = *reinterpret_cast<Initializer*>(base + table[2 + index]);
            if (initializer) initializer(0, nullptr, nullptr);
        }
    } catch (...) {
        finish(false);
        throw;
    }
    finish(true);
}
#endif

void* APS5_VABI dlopen_nid_postfix(const char* path, int flags) {
    if ((flags & ~0x103) || (flags & 3) == 0 || (flags & 3) == 3) {
        Error("dlopen: unsupported flags"); return nullptr;
    }
    try {
        auto module = std::make_shared<Module>();
        module->global = (flags & 0x100) != 0 || !path;
#ifdef _WIN32
        if (!path) {
            module->native = GetModuleHandleW(nullptr);
            module->owned = false;
        } else {
            if (!*path) { Error("dlopen: empty module path"); return nullptr; }
            const auto resolved = RelinkedModulePath_nid_no_patch(ResolvePath_nid_no_patch(path));
            module->native = LoadLibraryExW(resolved.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (module->native) InitializeDeferredModule(static_cast<HMODULE>(module->native));
        }
        if (!module->native) {
            char message[128];
            std::snprintf(message, sizeof(message), "dlopen: Windows loader error %lu (module must be host-compatible)", GetLastError());
            Error(message); return nullptr;
        }
#else
        const auto resolved = path ? RelinkedModulePath_nid_no_patch(ResolvePath_nid_no_patch(path)).string() : std::string{};
        const int nativeFlags = ((flags & 3) == 1 ? RTLD_LAZY : RTLD_NOW) |
            ((flags & 0x100) ? RTLD_GLOBAL : RTLD_LOCAL);
        module->native = ::dlopen(path ? resolved.c_str() : nullptr, nativeFlags);
        if (!module->native) { Error(::dlerror()); return nullptr; }
#endif
        std::lock_guard lock(modulesMutex);
        const auto handle = nextHandle++;
        modules.emplace(handle, std::move(module));
        return reinterpret_cast<void*>(handle);
    } catch (const std::exception& error) { Error(error.what()); return nullptr; }
}
void* APS5_VABI dlsym_nid_postfix(void* handle, const char* name) {
    if (!name || !*name) { Error("dlsym: empty symbol name"); return nullptr; }
    try {
        std::vector<std::shared_ptr<Module>> search;
        {
            std::lock_guard lock(modulesMutex);
            if (handle == nullptr || handle == reinterpret_cast<void*>(static_cast<std::intptr_t>(-2))) {
                for (const auto& [key, module] : modules) if (module->global) search.push_back(module);
            } else {
                auto found = modules.find(reinterpret_cast<std::uintptr_t>(handle));
                if (found == modules.end()) { Error("dlsym: invalid or unsupported module handle"); return nullptr; }
                search.push_back(found->second);
            }
        }
        for (const auto& module : search) if (auto* result = FindSymbol(*module, name)) return result;
#ifndef _WIN32
        if (handle == nullptr || handle == reinterpret_cast<void*>(static_cast<std::intptr_t>(-2))) {
            if (auto* result = DefaultScopeSymbol(name)) return result;
            const auto nid = Nid::ComputeNid(name, "");
            if (auto* result = DefaultScopeSymbol(nid.c_str())) return result;
        }
#endif
        Error("dlsym: symbol not found in supported module scope");
        return nullptr;
    } catch (const std::exception& error) { Error(error.what()); return nullptr; }
}
int APS5_VABI dlclose_nid_postfix(void* handle) {
    const auto key = reinterpret_cast<std::uintptr_t>(handle);
    std::shared_ptr<Module> module;
#ifdef _WIN32
    bool lastReference = true;
#endif
    {
        std::lock_guard lock(modulesMutex);
        auto found = modules.find(key);
        if (found == modules.end()) { Error("dlclose: invalid module handle"); return -1; }
        module = found->second;
#ifdef _WIN32
        for (const auto& [other, entry] : modules) {
            if (other != key && entry->native == module->native) lastReference = false;
        }
#endif
    }
#ifdef _WIN32
    if (module->owned && module->native && lastReference) {
        GuestAllocations::Mutation mutation;
        mutation.UnregisterImage(module->native);
    }
#endif
    {
        std::lock_guard lock(modulesMutex);
        auto found = modules.find(key);
        if (found == modules.end()) { Error("dlclose: invalid module handle"); return -1; }
        modules.erase(found);
    }
    // Unload outside the registry lock: module destructors may call loader APIs.
    module.reset();
    return 0;
}
std::int32_t ModuleIdForImage_nid_no_patch(const void* native) {
    std::lock_guard lock(modulesMutex);
    for (const auto& [handle, module] : modules) {
        if (module->native == native) return static_cast<std::int32_t>(handle);
    }
    const auto [found, inserted] = imageIds.emplace(native, nextHandle);
    if (inserted) ++nextHandle;
    return static_cast<std::int32_t>(found->second);
}
}
