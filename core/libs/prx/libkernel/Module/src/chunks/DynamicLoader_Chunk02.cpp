extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/Module/ModuleArgs.hpp"
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
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
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#else
#include <link.h>
#ifdef __linux__
#include <unistd.h>
#endif
#endif
#endif
#include "prx/libc/include/HostThreadLocal.hpp"

#ifdef _WIN32
extern "C" void ForgetUnloadedUnwindImages_nid_no_patch();
#endif

namespace {
struct LoaderErrorTag;
std::string& LoaderError() {
    return HostThreadLocal<std::string, LoaderErrorTag>();
}
thread_local bool pendingError = false;
void Error(const char* message) {
    LoaderError() = message;
    pendingError = true;
}
struct ModulePaths {
    std::filesystem::path source;
    std::filesystem::path resolved;
};

ModulePaths ResolveModulePaths(const char* path) {
    ModulePaths paths{ResolvePath_nid_no_patch(path)};
    paths.resolved = RelinkedModulePath_nid_no_patch(paths.source);
    std::error_code error;
    if (std::filesystem::is_regular_file(paths.resolved, error) || std::string_view(path).find_first_of("/\\") != std::string_view::npos) return paths;
    try {
        ModulePaths app{ResolvePath_nid_no_patch(("/app0/" + std::string(path)).c_str())};
        app.resolved = RelinkedModulePath_nid_no_patch(app.source);
        return std::filesystem::is_regular_file(app.resolved, error) ? app : paths;
    } catch (const std::filesystem::filesystem_error&) {
        return paths;
    }
}

void LoadFailure(const std::filesystem::path& source, const std::filesystem::path& resolved, const std::string& reason) {
    std::error_code status;
    const auto message = resolved == source && std::filesystem::is_regular_file(source, status)
        ? "dlopen: " + source.string() + " was not relinked: no " + source.filename().string() + ".guest.prx was found (" + reason +
            "); relink its directory with the relinker's --module-dir <dir> option"
        : "dlopen: cannot load " + resolved.string() + " (" + reason + ")";
    Error(message.c_str());
    static std::mutex reportedLock;
    static std::set<std::filesystem::path> reported;
    std::lock_guard lock(reportedLock);
    if (reported.insert(resolved).second) std::fprintf(stderr, "[module] %s\n", message.c_str());
}
#ifdef _WIN32
HMODULE LoadWithoutErrorDialog(const std::filesystem::path& path) {
    const auto mode = GetThreadErrorMode();
    SetThreadErrorMode(mode | SEM_FAILCRITICALERRORS, nullptr);
    const auto loaded = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    const auto error = GetLastError();
    SetThreadErrorMode(mode, nullptr);
    SetLastError(error);
    return loaded;
}
#endif
struct Module {
    void* native = nullptr;
    bool owned = true;
    bool global = false;
#ifdef _WIN32
    std::uintptr_t globalOrder = 0;
#endif
#ifdef __APPLE__
    const void* image = nullptr;
    const void* loadBias = nullptr;
#endif
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
#ifdef __APPLE__
bool FromGuestModule(void* address) {
    Dl_info info{};
    if (address == nullptr || !::dladdr(address, &info) || info.dli_fname == nullptr || info.dli_fbase == nullptr) return false;
    if (static_cast<const mach_header_64*>(info.dli_fbase)->filetype == MH_EXECUTE) return true;
    const std::string path(info.dli_fname);
    return path.ends_with(".prx");
}

void FindImage(Module& module, bool mainProgram) {
    for (std::uint32_t index = 0; index < _dyld_image_count() && module.image == nullptr; ++index) {
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        if (header == nullptr) continue;
        if (mainProgram) {
            if (header->filetype == MH_EXECUTE) module.image = header;
            continue;
        }
        void* loaded = ::dlopen(_dyld_get_image_name(index), RTLD_LAZY | RTLD_NOLOAD);
        if (loaded == nullptr) continue;
        if (loaded == module.native) module.image = header;
        ::dlclose(loaded);
    }
    module.loadBias = module.image;
    unsigned long size = 0;
    const auto* meta = module.image ? getsectiondata(static_cast<const mach_header_64*>(module.image), "__APS5DATA", "__meta", &size) : nullptr;
    std::uint64_t version = 0;
    if (meta != nullptr && size >= 0x60 && (std::memcpy(&version, meta, sizeof(version)), version == 2)) {
        std::uint64_t loadBias = 0;
        std::memcpy(&loadBias, meta + 0x08, sizeof(loadBias));
        module.loadBias = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(loadBias));
    }
}

bool HiddenImage(const void* image) {
    if (image == nullptr || static_cast<const mach_header_64*>(image)->filetype == MH_EXECUTE) return false;
    bool local = false;
    for (const auto& [key, module] : modules) {
        if (module->image != image) continue;
        if (module->global) return false;
        local = true;
    }
    return local;
}

void* DefaultScopeSymbol(const char* symbolName) {
    auto* result = ::dlsym(RTLD_DEFAULT, symbolName);
    if (!FromGuestModule(result)) return nullptr;
    Dl_info info{};
    if (!::dladdr(result, &info) || info.dli_fbase == nullptr) return result;
    std::lock_guard lock(modulesMutex);
    return HiddenImage(info.dli_fbase) ? nullptr : result;
}
#elif !defined(_WIN32)
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
