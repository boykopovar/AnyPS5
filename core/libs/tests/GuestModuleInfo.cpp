#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t, int, ModuleInfoEx*);
int APS5_VABI sceKernelGetModuleList(KernelModule*, std::size_t, std::size_t*);
int APS5_VABI sceKernelGetModuleInfo(KernelModule, ModuleInfo*);
}
static void Require(bool value) { if (!value) std::abort(); }
template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
static ModuleInfoEx Query(const void* address, int expected) {
    ModuleInfoEx info{};
    info.st_size = sizeof(ModuleInfoEx);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(address), 2, &info) == expected);
    return info;
}
int main(int argc, char** argv) {
    Require(argc == 2);
    void* module = dlopen_nid_postfix(argv[1], 2);
    Require(module != nullptr);
    const void* add = dlsym_nid_postfix(module, "GuestModuleAdd");
    Require(add != nullptr);
    const auto info = Query(add, 0);
    Require(info.st_size == sizeof(ModuleInfoEx));
    Require(info.id == static_cast<KernelModule>(reinterpret_cast<std::intptr_t>(module)));
    Require(std::string(info.name) == std::string(argv[1]).substr(std::string(argv[1]).find_last_of("/\\") + 1));
    Require(info.segment_count > 0 && info.segment_count <= 4 && info.ref_count == 1);
    bool executable = false;
    for (std::uint32_t i = 0; i < info.segment_count; ++i) {
        const auto& segment = info.segments[i];
        if (reinterpret_cast<std::uintptr_t>(add) >= segment.address && reinterpret_cast<std::uintptr_t>(add) - segment.address < segment.size)
            executable = (segment.prot & 5) == 5;
    }
    Require(executable);
    Require(info.eh_frame_addr != 0 && info.eh_frame_size != 0);
#ifndef _WIN32
    Require(info.eh_frame_hdr_addr != 0 && info.eh_frame_hdr_size != 0);
#endif
    Require(info.init_proc_addr == 0 || info.init_proc_addr >= info.segments[0].address);
    const auto self = Query(reinterpret_cast<const void*>(&Require), 0);
    Require(self.id != info.id && Query(reinterpret_cast<const void*>(&Query), 0).id == self.id);
    void* second = dlopen_nid_postfix(argv[1], 2);
    Require(second && Query(add, 0).id == info.id);
    Require(dlclose_nid_postfix(second) == 0);
    std::vector<KernelModule> handles(512, -1);
    std::size_t count = 0;
    Require(sceKernelGetModuleList(handles.data(), handles.size(), &count) == 0);
    Require(count >= 2 && count < handles.size() && handles[count] == -1);
    handles.resize(count);
    Require(handles.front() == self.id);
    const auto kernel = Query(reinterpret_cast<const void*>(&sceKernelGetModuleList), 0).id;
    Require(std::count(handles.begin(), handles.end(), kernel) == 1);
    Require(std::count(handles.begin(), handles.end(), info.id) == 0);
    auto sorted = handles;
    std::sort(sorted.begin(), sorted.end());
    Require(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
    std::vector<KernelModule> shorter(count, -1);
    std::size_t untouched = 99;
    Require(sceKernelGetModuleList(shorter.data(), count - 1, &untouched) == SCE_KERNEL_ERROR_ENOMEM);
    Require(untouched == 99 && shorter.back() == -1 && std::equal(handles.begin(), handles.end() - 1, shorter.begin()));
    Require(sceKernelGetModuleList(nullptr, 0, &untouched) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetModuleList(handles.data(), handles.size(), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    const auto kernelEx = Query(reinterpret_cast<const void*>(&sceKernelGetModuleInfo), 0);
    ModuleInfo kernelInfo{};
    kernelInfo.st_size = sizeof(ModuleInfo);
    Require(sceKernelGetModuleInfo(kernel, &kernelInfo) == 0);
    Require(kernelInfo.st_size == sizeof(ModuleInfo) && std::string(kernelInfo.name) == kernelEx.name);
    Require(kernelInfo.segment_count == kernelEx.segment_count && kernelInfo.segment_count > 0);
    for (std::uint32_t i = 0; i < kernelInfo.segment_count; ++i)
        Require(kernelInfo.segments[i].address == kernelEx.segments[i].address && kernelInfo.segments[i].size == kernelEx.segments[i].size && kernelInfo.segments[i].prot == kernelEx.segments[i].prot);
    Require(std::all_of(std::begin(kernelInfo.fingerprint), std::end(kernelInfo.fingerprint), [](std::uint8_t byte) { return byte == 0; }));
    for (const auto handle : handles) {
        ModuleInfo listed{};
        listed.st_size = sizeof(ModuleInfo);
        Require(sceKernelGetModuleInfo(handle, &listed) == 0 && listed.name[0] != '\0');
        if (handle == self.id) Require(std::string(listed.name) == self.name);
    }
    for (const std::uint64_t size : {0x158, 0x1a8, 0x1b0, 0x200, 0}) {
        ModuleInfo sized{};
        sized.st_size = size;
        sized.name[0] = 'x';
        Require(sceKernelGetModuleInfo(kernel, &sized) == SCE_KERNEL_ERROR_EINVAL && sized.st_size == size && sized.name[0] == 'x');
    }
    Require(sceKernelGetModuleInfo(kernel, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    ModuleInfo unknown{};
    unknown.st_size = sizeof(ModuleInfo);
    Require(sceKernelGetModuleInfo(info.id, &unknown) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelGetModuleInfo(-1, &unknown) == SCE_KERNEL_ERROR_ESRCH && unknown.name[0] == '\0');
    int local = 0;
    Query(&local, SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    ModuleInfoEx invalid{};
    invalid.st_size = sizeof(ModuleInfoEx);
    Require(ThrowsInvalidArgument([&] { sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 1, &invalid); }));
    invalid.st_size = sizeof(ModuleInfoEx) - 8;
    Require(ThrowsInvalidArgument([&] { sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 2, &invalid); }));
    Require(dlclose_nid_postfix(module) == 0);
}
