#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <filesystem>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <link.h>
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
std::uintptr_t MainProgramBase() {
    static const std::uintptr_t base = [] {
        struct Scan {
            std::uintptr_t value = 0;
        };
        Scan scan;
        ::dl_iterate_phdr(
            [](dl_phdr_info* info, size_t, void* data) {
                auto* found = static_cast<Scan*>(data);
                if (found->value == 0 && info->dlpi_name != nullptr && info->dlpi_name[0] == '\0') found->value = info->dlpi_addr;
                return found->value == 0 ? 0 : 1;
            },
            &scan);
        return scan.value;
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
static std::filesystem::path RelinkedModulePath(const std::filesystem::path& path) {
    auto relinked = path;
    relinked += ".guest.prx";
    std::error_code error;
    return std::filesystem::is_regular_file(relinked, error) ? relinked : path;
}

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
            const auto resolved = RelinkedModulePath(ResolvePath_nid_no_patch(path));
            module->native = LoadLibraryExW(resolved.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        if (!module->native) {
            char message[128];
            std::snprintf(message, sizeof(message), "dlopen: Windows loader error %lu (module must be host-compatible)", GetLastError());
            Error(message); return nullptr;
        }
#else
        const auto resolved = path ? RelinkedModulePath(ResolvePath_nid_no_patch(path)).string() : std::string{};
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
