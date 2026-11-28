#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include "prx/libc/include/General.hpp"

#ifdef _WIN32
#include <vector>
#include <windows.h>
#include <winternl.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif !defined(__EMSCRIPTEN__)
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
#endif

#include "SceTypes.hpp"
#include "ModuleTable.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_SYSMODULE_ERROR_INVALID_VALUE = static_cast<int>(0x805A1000);
constexpr int SCE_SYSMODULE_ERROR_UNLOADED = static_cast<int>(0x805A1001);
constexpr int kRtldNow = 2;
constexpr std::uint32_t kMeasuredInternalUnloadedModuleId = 0x800000c4u;

const char* findModuleName(const std::uint32_t id) {
    const auto it = kModuleTable.find(id);
    return it != kModuleTable.end() ? it->second : nullptr;
}

bool isMeasuredRejectedInternalId(const std::uint32_t id) {
    return id == 0x8000ffffu || id == 0x80ffff00u;
}

bool isPublicModuleId(const std::uint16_t id) {
    return std::binary_search(std::begin(kPublicModuleIds), std::end(kPublicModuleIds), id);
}

bool isPs4ModuleId(const std::uint16_t id) {
    static const bool ps4Process = sceKernelIsPs4Process() != 0;
    return ps4Process && std::binary_search(std::begin(kPs4ModuleIds), std::end(kPs4ModuleIds), id);
}

bool isAcceptedModuleId(const std::uint16_t id) {
    return isPublicModuleId(id) || isPs4ModuleId(id);
}

std::mutex gMutex;
std::unordered_map<std::uint32_t, std::int32_t> gLoadCount;
std::unordered_map<std::uint32_t, KernelModule> gHandles;
bool gLibcForLibkernelPreloaded = false;
KernelModule (*gLibcForLibkernelLoader)(const char*) = nullptr;
const char* (*gLibcForLibkernelSandbox)() = nullptr;
int (*gLibcForLibkernelPs4)() = nullptr;

void writeLibcHandle(int* handle, int value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(handle, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::runtime_error("sceSysmoduleMapLibcForLibkernel: invalid handle pointer");
}

int loadLibcForLibkernel(const char* path, int* handle, KernelModule (*loader)(const char*)) {
    const KernelModule loaded = loader != nullptr ? loader(path) : sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, nullptr);
    if (loaded < 0) return loaded;
    if (loaded == 0) throw std::runtime_error("sceSysmoduleMapLibcForLibkernel: loader returned a null handle");
    try {
        writeLibcHandle(handle, loaded);
    } catch (...) {
        sceKernelStopUnloadModule(loaded, 0, nullptr, 0, nullptr, nullptr);
        throw;
    }
    return 0;
}

void* openHostModule(const char* name) {
    const std::string relative = std::string("libs/") + name + ".prx";
    return dlopen_nid_postfix(relative.c_str(), kRtldNow);
}

#ifdef _WIN32
struct ImageRegion {
    std::uint64_t end = 0;
    std::uintptr_t image = 0;
    DWORD stamp = 0;
    DWORD size = 0;
    std::string name;
};

std::mutex gRegionsMutex;
std::map<std::uint64_t, ImageRegion> gRegions;

bool imageIdentity(std::uintptr_t image, DWORD& stamp, DWORD& size) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    stamp = nt->FileHeader.TimeDateStamp;
    size = nt->OptionalHeader.SizeOfImage;
    return true;
}

std::uintptr_t loaderImage(std::uint64_t addr) {
    PVOID image = nullptr;
    if (RtlPcToFileHeader(reinterpret_cast<PVOID>(addr), &image) == nullptr) return 0;
    return reinterpret_cast<std::uintptr_t>(image);
}

std::string mappedFileName(const void* address) {
    using QueryFunction = LONG(NTAPI*)(HANDLE, PVOID, int, PVOID, SIZE_T, SIZE_T*);
    static const auto query = reinterpret_cast<QueryFunction>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryVirtualMemory")));
    if (query == nullptr) throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: ntdll.dll has no NtQueryVirtualMemory");
    constexpr int memoryMappedFilenameInformation = 2;
    std::vector<std::uint64_t> buffer((sizeof(UNICODE_STRING) + 32768 * sizeof(wchar_t)) / sizeof(std::uint64_t) + 1);
    if (query(GetCurrentProcess(), const_cast<void*>(address), memoryMappedFilenameInformation, buffer.data(), buffer.size() * sizeof(std::uint64_t), nullptr) < 0) return {};
    const auto* name = reinterpret_cast<const UNICODE_STRING*>(buffer.data());
    const int characters = name->Length / sizeof(wchar_t);
    if (characters == 0) return {};
    const int bytes = WideCharToMultiByte(CP_ACP, 0, name->Buffer, characters, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (bytes == 0 || WideCharToMultiByte(CP_ACP, 0, name->Buffer, characters, result.data(), bytes, nullptr, nullptr) != bytes) return {};
    return result;
}

void writeUnwindRegion(std::uint64_t begin, std::uint64_t size, const std::string& name, ModuleInfoForUnwind* info) {
    info->st_size = sizeof(ModuleInfoForUnwind);
    info->eh_frame_hdr_addr = 0;
    info->eh_frame_addr = 0;
    info->eh_frame_size = 0;
    info->seg0_addr = begin;
    info->seg0_size = size;
    std::strncpy(info->name, name.c_str(), sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
}

bool fillCachedImageRegion(std::uint64_t addr, std::uintptr_t image, ModuleInfoForUnwind* info) {
    std::lock_guard lock(gRegionsMutex);
    auto found = gRegions.upper_bound(addr);
    if (found == gRegions.begin()) return false;
    --found;
    const auto& region = found->second;
    if (addr >= region.end) return false;
    DWORD stamp = 0;
    DWORD size = 0;
    if (region.image != image || !imageIdentity(image, stamp, size) || stamp != region.stamp || size != region.size) {
        gRegions.erase(found);
        return false;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t begin = addr & ~(page - 1);
    writeUnwindRegion(begin, region.end - begin, region.name, info);
    return true;
}
#endif

bool fillModuleInfoForUnwind(std::uint64_t addr, ModuleInfoForUnwind* info) {
#ifdef _WIN32
    const auto image = loaderImage(addr);
    if (image != 0 && fillCachedImageRegion(addr, image, info)) return true;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        return false;
    }
    const auto path = mappedFileName(mbi.BaseAddress);
    const auto begin = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
    writeUnwindRegion(begin, mbi.RegionSize, path, info);
    ImageRegion region;
    if (mbi.Type == MEM_IMAGE && image != 0 && imageIdentity(image, region.stamp, region.size) && begin >= image && begin + mbi.RegionSize <= image + region.size) {
        region.end = begin + mbi.RegionSize;
        region.image = image;
        region.name = path;
        std::lock_guard lock(gRegionsMutex);
        gRegions.insert_or_assign(begin, std::move(region));
    }
    return true;
#elif defined(__APPLE__)
    mach_vm_address_t region = addr;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t host{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&host), &count, &object) != KERN_SUCCESS || region > addr) {
        return false;
    }
    Dl_info image{};
    info->st_size = sizeof(ModuleInfoForUnwind);
    std::strncpy(info->name, dladdr(reinterpret_cast<void*>(addr), &image) && image.dli_fname != nullptr ? image.dli_fname : "", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->eh_frame_hdr_addr = 0;
    info->eh_frame_addr = 0;
    info->eh_frame_size = 0;
    info->seg0_addr = region;
    info->seg0_size = size;
    return true;
#elif defined(__EMSCRIPTEN__)
    return sceKernelGetModuleInfoForUnwind(addr, 0, info) == 0;
#else
    const auto mapping = HostMapping::Find(addr, "sceSysmoduleGetModuleInfoForUnwind");
    if (!mapping) {
        return false;
    }
    info->st_size = sizeof(ModuleInfoForUnwind);
    std::strncpy(info->name, mapping->path.c_str(), sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->eh_frame_hdr_addr = 0;
    info->eh_frame_addr = 0;
    info->eh_frame_size = 0;
    info->seg0_addr = mapping->start;
    info->seg0_size = mapping->end - mapping->start;
    return true;
#endif
}

}

void SetSysmoduleLibcForLibkernelSources_nid_no_patch(KernelModule (*loader)(const char*), const char* (*sandbox)(), int (*ps4)()) {
    std::lock_guard lock(gMutex);
    gLibcForLibkernelLoader = loader;
    gLibcForLibkernelSandbox = sandbox;
    gLibcForLibkernelPs4 = ps4;
}

extern "C" {

int APS5_VABI sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)flags;
    if (!info) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: null info");
    }
    ModuleInfoForUnwind localInfo{};
    if (!fillModuleInfoForUnwind(addr, &localInfo)) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: address not found");
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: invalid info pointer");
    }
    return 0;
}

int APS5_VABI sceSysmoduleGetModuleHandleInternal(std::uint32_t id, std::int32_t* handle) {
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleGetModuleHandleInternal: invalid id 0");
    }
    if (!handle) {
        throw std::runtime_error("sceSysmoduleGetModuleHandleInternal: null handle");
    }
    const char* name = findModuleName(id);
    if (!name) {
        throw std::runtime_error(std::string("sceSysmoduleGetModuleHandleInternal: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    const auto loaded = gLoadCount.find(id);
    if (loaded == gLoadCount.end() || loaded->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    const auto existing = gHandles.find(id);
    if (existing != gHandles.end()) {
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(handle, &existing->second, sizeof(existing->second));
        errno = savedErrno;
        if (!copied) {
            throw std::runtime_error("sceSysmoduleGetModuleHandleInternal: invalid handle pointer");
        }
        return 0;
    }
    void* module = openHostModule(name);
    if (!module) {
        throw std::runtime_error(std::string("sceSysmoduleGetModuleHandleInternal: failed to open ") + name);
    }
    const auto value = static_cast<KernelModule>(reinterpret_cast<std::intptr_t>(module));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(handle, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) {
        dlclose_nid_postfix(module);
        throw std::runtime_error("sceSysmoduleGetModuleHandleInternal: invalid handle pointer");
    }
    gHandles.emplace(id, value);
    return 0;
}

int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id) {
    if (!isAcceptedModuleId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        APS5_LOG_OUT("unknown id: %u", static_cast<unsigned>(id));
        return 0;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    return 0;
}

int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id) {
    if (!isAcceptedModuleId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        APS5_LOG_OUT("unknown id: %u", static_cast<unsigned>(id));
        return 0;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    return 0;
}

int APS5_VABI sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) {
    (void)argc;
    (void)argv;
    (void)unk;
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleLoadModuleInternalWithArg: unknown id ") + std::to_string(id));
    }
    if (ret) {
        const int val = 0;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(ret, &val, sizeof(val));
        errno = savedErrno;
        if (!copied) {
            throw std::runtime_error("sceSysmoduleLoadModuleInternalWithArg: invalid ret pointer");
        }
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id) {
    if (!isAcceptedModuleId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        APS5_LOG_OUT("unknown id: %u", static_cast<unsigned>(id));
        return 0;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    it->second--;
    return 0;
}

int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        if (isMeasuredRejectedInternalId(id)) {
            return SCE_SYSMODULE_ERROR_INVALID_VALUE;
        }
        throw std::runtime_error(std::string("sceSysmoduleLoadModuleInternal: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        if (isMeasuredRejectedInternalId(id)) {
            return SCE_SYSMODULE_ERROR_INVALID_VALUE;
        }
        throw std::runtime_error(std::string("sceSysmoduleUnloadModuleInternal: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    it->second--;
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) {
    (void)argc;
    (void)argv;
    (void)unk;
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleUnloadModuleInternalWithArg: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    if (ret) {
        const int val = 0;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(ret, &val, sizeof(val));
        errno = savedErrno;
        if (!copied) {
            throw std::runtime_error("sceSysmoduleUnloadModuleInternalWithArg: invalid ret pointer");
        }
    }
    it->second--;
    return 0;
}

int APS5_VABI sceSysmoduleIsLoadedInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        if (id == kMeasuredInternalUnloadedModuleId) {
            return SCE_SYSMODULE_ERROR_UNLOADED;
        }
        if (isMeasuredRejectedInternalId(id) || id == 0x00ffffffu) {
            return SCE_SYSMODULE_ERROR_INVALID_VALUE;
        }
        throw std::runtime_error(std::string("sceSysmoduleIsLoadedInternal: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    return 0;
}

int APS5_VABI sceSysmoduleIsCalledFromSysModule(const void* address) {
    ModuleInfoEx info{};
    if (sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uint64_t>(address), 1, &info) != 0) return 0;
    const auto value = reinterpret_cast<std::uint64_t>(address);
    bool executable = false;
    for (std::uint32_t index = 0; index < info.segment_count; ++index) {
        const auto& segment = info.segments[index];
        if ((segment.prot & 4) != 0 && value >= segment.address && value - segment.address < segment.size) {
            executable = true;
            break;
        }
    }
    if (!executable) return 0;
    const std::string_view module(info.name);
    std::lock_guard lock(gMutex);
    for (const auto& [id, count] : gLoadCount) {
        if (count < 1) continue;
        const char* name = findModuleName(id);
        if (name == nullptr) continue;
        const std::string_view expected(name);
        if (module == expected ||
            (module.size() == expected.size() + 4 && module.starts_with(expected) && module.ends_with(".prx")) ||
            (module.size() == expected.size() + 10 && module.starts_with(expected) && module.ends_with(".guest.prx"))) return 1;
    }
    return 0;
}

int APS5_VABI sceSysmoduleIsCameraPreloaded(void) {
    if (sceKernelIsPs4Process() == 1) NotImplemented_nid_no_patch("sceSysmoduleIsCameraPreloaded in a PS4 process");
    return 0;
}

KernelModule APS5_VABI sceSysmoduleLoadModuleByNameInternal(const char* fname, std::size_t args, const void* argp, std::uint32_t flags, int* res) {
    if (fname == nullptr) return static_cast<KernelModule>(SCE_KERNEL_ERROR_EINVAL);
    char path[0x100];
    std::string name;
    const int copyError = CopyGuestString_nid_no_patch(fname, sizeof(path), name);
    if (copyError == 63) {
        name.resize(sizeof(path));
        if (!GuestCopyIn_nid_no_patch(name.data(), fname, sizeof(path))) return static_cast<KernelModule>(SCE_KERNEL_ERROR_EFAULT);
    } else if (copyError != 0) {
        return static_cast<KernelModule>(SCE_KERNEL_ERROR_EFAULT);
    }
    const char* sandbox = sceKernelGetFsSandboxRandomWord();
    KernelModule result = static_cast<KernelModule>(SCE_KERNEL_ERROR_ENOENT);
    const auto attempt = [&](const char* root, const char* tier, const char* ext) {
        std::snprintf(path, sizeof(path), "/%s/%s/lib/%s.%s", root, tier, name.c_str(), ext);
        result = sceKernelLoadStartModule(path, args, argp, flags, nullptr, res);
        return result > 0;
    };
    if (sandbox != nullptr &&
        (attempt(sandbox, "priv_ex", "sprx") || attempt(sandbox, "common_ex", "sprx") ||
         attempt(sandbox, "priv", "sprx") || attempt(sandbox, "common", "sprx"))) return result;
    if (attempt("system_ex", "priv_ex", "sprx") || attempt("system_ex", "common_ex", "sprx") ||
        attempt("system", "priv", "sprx") || attempt("system", "common", "sprx")) return result;
    if (sandbox != nullptr && (attempt(sandbox, "priv", "prx") || attempt(sandbox, "common", "prx"))) return result;
    if (attempt("system", "priv", "prx") || attempt("system", "common", "prx")) return result;
    return result;
}

int APS5_VABI sceSysmoduleMapLibcForLibkernel(int* handle) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(handle, sizeof(*handle));
    errno = savedErrno;
    if (!writable) {
        throw std::runtime_error("sceSysmoduleMapLibcForLibkernel: invalid handle pointer");
    }
    bool preloaded;
    KernelModule (*loader)(const char*);
    const char* (*sandboxSource)();
    int (*ps4Source)();
    {
        std::lock_guard lock(gMutex);
        preloaded = gLibcForLibkernelPreloaded;
        loader = gLibcForLibkernelLoader;
        sandboxSource = gLibcForLibkernelSandbox;
        ps4Source = gLibcForLibkernelPs4;
    }
    const bool ps4 = (ps4Source != nullptr ? ps4Source() : sceKernelIsPs4Process()) == 1;
    if (!ps4 && !preloaded) {
        writeLibcHandle(handle, -1);
        return 0x5d;
    }
    int result = loadLibcForLibkernel("/app0/sce_module/libc.prx", handle, loader);
    if (result == 0) return 0;
    const char* sandbox = sandboxSource != nullptr ? sandboxSource() : sceKernelGetFsSandboxRandomWord();
    if (sandbox != nullptr) {
        char path[0x100];
        std::snprintf(path, sizeof(path), "/%s/common/lib/libc.sprx", sandbox);
        result = loadLibcForLibkernel(path, handle, loader);
        if (result == 0) return 0;
    }
    result = loadLibcForLibkernel("/system/common/lib/libc.sprx", handle, loader);
    if (result != 0 && !ps4) writeLibcHandle(handle, -1);
    return result;
}

int APS5_VABI sceSysmodulePreloadModuleForLibkernel() {
    int (*ps4Source)();
    {
        std::lock_guard lock(gMutex);
        ps4Source = gLibcForLibkernelPs4;
    }
    if ((ps4Source != nullptr ? ps4Source() : sceKernelIsPs4Process()) == 1) {
        NotImplemented_nid_no_patch(__func__);
    }
    {
        std::lock_guard lock(gMutex);
        gLibcForLibkernelPreloaded = true;
    }
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModuleByNameInternal(KernelModule handle, std::size_t args, const void* argp, std::uint32_t flags, int* res) {
    return sceKernelStopUnloadModule(handle, args, argp, flags, nullptr, res);
}

}
