#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#endif

#include "SceTypes.hpp"
#include "ModuleTable.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
int APS5_VABI dlclose_nid_postfix(void*);
char* APS5_VABI dlerror_nid_postfix();
}

namespace {

const char* findModuleName(const std::uint32_t id) {
    const auto it = kModuleTable.find(id);
    return it != kModuleTable.end() ? it->second : nullptr;
}

std::mutex gMutex;
enum class Phase { Loading, Loaded, Unloading };
struct Provider {
    Phase phase = Phase::Loading;
    std::int32_t references = 0;
    void* handle = nullptr;
};
std::unordered_map<std::uint32_t, Provider> gProviders;

const char* requireModuleName(std::uint32_t id, const char* operation) {
    const char* name = findModuleName(id);
    if (!name) {
        throw std::runtime_error(std::string(operation) + ": unknown id " + std::to_string(id));
    }
    return name;
}

std::filesystem::path providerPath(const char* name) {
#ifdef _WIN32
    wchar_t executable[32768];
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768) {
        throw std::runtime_error("sceSysmodule: cannot resolve executable directory");
    }
    const auto directory = std::filesystem::path(executable).parent_path();
#else
    const auto directory = std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
    return directory / "libs" / (std::string(name) + ".prx");
}

int acquireProvider(std::uint32_t id, const char* operation) {
    const char* name = requireModuleName(id, operation);
    {
        std::lock_guard lock(gMutex);
        auto [found, inserted] = gProviders.try_emplace(id);
        if (!inserted) {
            auto& provider = found->second;
            if (provider.phase != Phase::Loaded) {
                throw std::runtime_error(std::string(operation) + ": provider transition in progress");
            }
            if (provider.references == std::numeric_limits<std::int32_t>::max()) {
                throw std::runtime_error(std::string(operation) + ": provider reference overflow");
            }
            ++provider.references;
            return 0;
        }
    }
    void* handle = nullptr;
    try {
        const auto path = providerPath(name).string();
        handle = dlopen_nid_postfix(path.c_str(), 2);
    } catch (...) {
        std::lock_guard lock(gMutex);
        gProviders.erase(id);
        throw;
    }
    if (!handle) {
        const char* error = dlerror_nid_postfix();
        {
            std::lock_guard lock(gMutex);
            gProviders.erase(id);
        }
        throw std::runtime_error(std::string(operation) + ": failed to load " + name +
                                 (error ? std::string(": ") + error : ""));
    }
    {
        std::lock_guard lock(gMutex);
        auto& provider = gProviders.at(id);
        provider.handle = handle;
        provider.references = 1;
        provider.phase = Phase::Loaded;
    }
    return 0;
}

int releaseProvider(std::uint32_t id, const char* operation) {
    requireModuleName(id, operation);
    void* handle = nullptr;
    {
        std::lock_guard lock(gMutex);
        const auto found = gProviders.find(id);
        if (found == gProviders.end()) return static_cast<int>(0x80A90003);
        auto& provider = found->second;
        if (provider.phase != Phase::Loaded) {
            throw std::runtime_error(std::string(operation) + ": provider transition in progress");
        }
        if (--provider.references > 0) return 0;
        provider.phase = Phase::Unloading;
        handle = provider.handle;
        provider.handle = nullptr;
    }
    const int result = dlclose_nid_postfix(handle);
    if (result != 0) {
        throw std::runtime_error(std::string(operation) + ": native provider release failed");
    }
    {
        std::lock_guard lock(gMutex);
        gProviders.erase(id);
    }
    return 0;
}

bool fillModuleInfoForUnwind(std::uint64_t addr, ModuleInfoForUnwind* info) {
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        return false;
    }
    info->st_size = sizeof(ModuleInfoForUnwind);
    info->eh_frame_hdr_addr = 0;
    info->eh_frame_addr = 0;
    info->eh_frame_size = 0;
    info->seg0_addr = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
    info->seg0_size = mbi.RegionSize;
    char path[4096] = {};
    DWORD len = GetMappedFileNameA(GetCurrentProcess(), mbi.BaseAddress, path, sizeof(path) - 1);
    path[len] = '\0';
    std::strncpy(info->name, path, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    return true;
#else
    std::ifstream maps("/proc/self/maps");
    if (!maps) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: failed to open /proc/self/maps");
    }
    std::string line;
    while (std::getline(maps, line)) {
        std::uint64_t start = 0;
        std::uint64_t end = 0;
        char perms[8] = {};
        std::uint64_t offset = 0;
        unsigned int devMajor = 0;
        unsigned int devMinor = 0;
        std::uint64_t inode = 0;
        char path[4096] = {};
        int parsed = std::sscanf(
            line.c_str(),
            "%llx-%llx %7s %llx %x:%x %llu %4095s",
            (unsigned long long*)&start,
            (unsigned long long*)&end,
            perms,
            (unsigned long long*)&offset,
            &devMajor,
            &devMinor,
            (unsigned long long*)&inode,
            path
        );
        if (parsed < 7 || addr < start || addr >= end) {
            continue;
        }
        info->st_size = sizeof(ModuleInfoForUnwind);
        std::strncpy(info->name, parsed >= 8 ? path : "", sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        info->eh_frame_hdr_addr = 0;
        info->eh_frame_addr = 0;
        info->eh_frame_size = 0;
        info->seg0_addr = start;
        info->seg0_size = end - start;
        return true;
    }
    return false;
#endif
}

}

extern "C" {

int APS5_VABI sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)flags;
    if (!fillModuleInfoForUnwind(addr, info)) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: address not found");
    }
    return 0;
}

int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleIsLoaded: invalid id 0");
    }
    requireModuleName(id, "sceSysmoduleIsLoaded");
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gProviders.find(id);
    if (it == gProviders.end() || it->second.phase != Phase::Loaded || it->second.references < 1) {
        return 0x80A90002;
    }
    return 0;
}

int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleLoadModule: invalid id 0");
    }
    return acquireProvider(id, "sceSysmoduleLoadModule");
}

int APS5_VABI sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) {
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleLoadModuleInternalWithArg: invalid id 0");
    }
    if (argc != 0 || argv || unk != 0) {
        throw std::runtime_error("sceSysmoduleLoadModuleInternalWithArg: unsupported arguments");
    }
    const int result = acquireProvider(id, "sceSysmoduleLoadModuleInternalWithArg");
    if (ret) {
        *ret = result;
    }
    return result;
}

int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleUnloadModule: invalid id 0");
    }
    return releaseProvider(id, "sceSysmoduleUnloadModule");
}

int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleLoadModuleInternal: invalid id 0");
    }
    return acquireProvider(id, "sceSysmoduleLoadModuleInternal");
}

int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleUnloadModuleInternal: invalid id 0");
    }
    return releaseProvider(id, "sceSysmoduleUnloadModuleInternal");
}

}
