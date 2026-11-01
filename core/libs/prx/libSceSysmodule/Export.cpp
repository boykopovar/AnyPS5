#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include <cstdint>
#include <cstring>
#include <map>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
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

const char* findModuleName(const std::uint32_t id) {
    const auto it = kModuleTable.find(id);
    return it != kModuleTable.end() ? it->second : nullptr;
}

bool isPublicModuleId(const std::uint16_t id) {
    return std::binary_search(std::begin(kPublicModuleIds), std::end(kPublicModuleIds), id);
}

std::mutex gMutex;
std::unordered_map<std::uint32_t, std::int32_t> gLoadCount;
std::unordered_map<std::uint32_t, KernelModule> gHandles;

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
    char path[4096] = {};
    DWORD len = GetMappedFileNameA(GetCurrentProcess(), mbi.BaseAddress, path, sizeof(path) - 1);
    path[len] = '\0';
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

extern "C" {

int APS5_VABI sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)flags;
    if (!fillModuleInfoForUnwind(addr, info)) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: address not found");
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
        *handle = existing->second;
        return 0;
    }
    void* module = openHostModule(name);
    if (!module) {
        throw std::runtime_error(std::string("sceSysmoduleGetModuleHandleInternal: failed to open ") + name);
    }
    const auto value = static_cast<KernelModule>(reinterpret_cast<std::intptr_t>(module));
    gHandles.emplace(id, value);
    *handle = value;
    return 0;
}

int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id) {
    if (!isPublicModuleId(id)) {
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
    if (!isPublicModuleId(id)) {
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
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    if (ret) {
        *ret = 0;
    }
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id) {
    if (!isPublicModuleId(id)) {
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
    it->second--;
    if (ret) {
        *ret = 0;
    }
    return 0;
}

int APS5_VABI sceSysmoduleIsLoadedInternal(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleIsLoadedInternal: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return SCE_SYSMODULE_ERROR_UNLOADED;
    }
    return 0;
}

int APS5_VABI sceSysmoduleIsCalledFromSysModule() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysmoduleIsCameraPreloaded() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysmoduleLoadModuleByNameInternal(std::uint8_t* fname, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysmoduleMapLibcForLibkernel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysmodulePreloadModuleForLibkernel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysmoduleUnloadModuleByNameInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
