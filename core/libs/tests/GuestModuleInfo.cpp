#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <source_location>
#include <string>
#include <vector>
extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t, int, ModuleInfoEx*);
int APS5_VABI sceKernelGetModuleList(KernelModule*, std::size_t, std::size_t*);
int APS5_VABI sceKernelGetModuleInfo(KernelModule, ModuleInfo*);
int APS5_VABI sceKernelGetModuleList2(KernelModule*, std::size_t, std::size_t*);
int APS5_VABI sceKernelGetModuleInfo2(KernelModule, ModuleInfo*);
std::int32_t APS5_VABI sceKernelGetLibkernelTextLocation_nid_postfix(void**, std::size_t*);
std::int32_t APS5_VABI sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix(InternalSegmentInfo*);
}
static void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "module info check failed at line %u\n", location.line());
        std::abort();
    }
}
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
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(address), 1, &info) == expected);
    return info;
}
int main(int argc, char** argv) {
    Require(argc == 2);
    void* module = dlopen_nid_postfix(argv[1], 2);
    Require(module != nullptr);
    const void* add = dlsym_nid_postfix(module, "GuestModuleAdd");
    Require(add != nullptr);
    const auto info = Query(add, 0);
    Require(info.st_size == 0);
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
#if defined(_WIN32) && defined(__SEH__)
    Require(info.eh_frame_addr == 0 && info.eh_frame_size == 0);
#else
    Require(info.eh_frame_addr != 0 && info.eh_frame_size != 0);
#endif
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
    ModuleInfo program{};
    program.st_size = sizeof(ModuleInfo);
    Require(sceKernelGetModuleInfo(self.id, &program) == 0 && program.segment_count > 0);
    void* textAddress = nullptr;
    std::size_t textSize = 0;
    Require(sceKernelGetLibkernelTextLocation_nid_postfix(&textAddress, &textSize) == 0);
    Require(reinterpret_cast<std::uintptr_t>(textAddress) == program.segments[0].address);
    Require(textSize == program.segments[0].size);
    Require(ThrowsInvalidArgument([&] { sceKernelGetLibkernelTextLocation_nid_postfix(nullptr, &textSize); }));
    Require(ThrowsInvalidArgument([&] { sceKernelGetLibkernelTextLocation_nid_postfix(&textAddress, nullptr); }));
    InternalSegmentInfo segment{};
    Require(sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix(&segment) == 0);
    Require(segment.address == program.segments[0].address && segment.size == program.segments[0].size);
    Require(ThrowsInvalidArgument([] { sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix(nullptr); }));
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
    std::vector<KernelModule> titleHandles(handles.size() + 1, -1);
    std::size_t titleCount = 0;
    Require(sceKernelGetModuleList2(titleHandles.data(), titleHandles.size(), &titleCount) == 0);
    Require(titleCount >= 1 && titleCount < count && titleHandles[titleCount] == -1);
    titleHandles.resize(titleCount);
    Require(titleHandles.front() == self.id);
    for (const auto handle : titleHandles) {
        Require(std::count(handles.begin(), handles.end(), handle) == 1);
        ModuleInfo title{}, listed{};
        title.st_size = listed.st_size = sizeof(ModuleInfo);
        Require(sceKernelGetModuleInfo2(handle, &title) == 0 && sceKernelGetModuleInfo(handle, &listed) == 0);
        Require(std::string(title.name) == listed.name && title.segment_count == listed.segment_count);
    }
    std::size_t titleUntouched = 99;
    Require(sceKernelGetModuleList2(titleHandles.data(), titleCount - 1, &titleUntouched) == SCE_KERNEL_ERROR_ENOMEM && titleUntouched == 99);
    Require(sceKernelGetModuleList2(nullptr, 0, &titleUntouched) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetModuleList2(titleHandles.data(), titleHandles.size(), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    for (const auto handle : handles) {
        if (std::count(titleHandles.begin(), titleHandles.end(), handle) != 0) continue;
        ModuleInfo system{};
        system.st_size = sizeof(ModuleInfo);
        system.name[0] = 'x';
        Require(sceKernelGetModuleInfo2(handle, &system) == SCE_KERNEL_ERROR_EPERM && system.name[0] == 'x');
        Require(sceKernelGetModuleInfo2(handle, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        system.st_size = 0x158;
        Require(sceKernelGetModuleInfo2(handle, &system) == SCE_KERNEL_ERROR_EINVAL);
    }
    ModuleInfo absent{};
    absent.st_size = sizeof(ModuleInfo);
    Require(sceKernelGetModuleInfo2(-1, &absent) == SCE_KERNEL_ERROR_ESRCH && absent.name[0] == 0);
    int local = 0;
    Query(&local, SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    ModuleInfoEx invalid{};
    invalid.st_size = sizeof(ModuleInfoEx);
    for (const int flags : {-1, 0, 3, 4, 8}) {
        std::memset(&invalid, 0xbb, sizeof(invalid));
        Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), flags, &invalid) == SCE_KERNEL_ERROR_EINVAL);
        Require(std::all_of(reinterpret_cast<const unsigned char*>(&invalid), reinterpret_cast<const unsigned char*>(&invalid + 1), [](unsigned char value) { return value == 0; }));
    }
    Require(ThrowsInvalidArgument([&] { sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 0, nullptr); }));
    std::vector<KernelModule> everyModule(64);
    std::vector<KernelModule> titleOnlyModules(64);
    std::size_t everyCount = 0;
    std::size_t titleOnlyCount = 0;
    Require(sceKernelGetModuleList(everyModule.data(), everyModule.size(), &everyCount) == 0);
    Require(sceKernelGetModuleList2(titleOnlyModules.data(), titleOnlyModules.size(), &titleOnlyCount) == 0);
    const auto systemModule = std::find_if(everyModule.begin(), everyModule.begin() + static_cast<std::ptrdiff_t>(everyCount),
        [&](KernelModule handle) { return std::find(titleOnlyModules.begin(), titleOnlyModules.begin() + static_cast<std::ptrdiff_t>(titleOnlyCount), handle) == titleOnlyModules.begin() + static_cast<std::ptrdiff_t>(titleOnlyCount); });
    Require(systemModule != everyModule.begin() + static_cast<std::ptrdiff_t>(everyCount));
    ModuleInfo systemInfo{};
    systemInfo.st_size = sizeof(ModuleInfo);
    Require(sceKernelGetModuleInfo(*systemModule, &systemInfo) == 0 && systemInfo.segment_count > 0);
    const auto systemAddress = static_cast<std::uintptr_t>(systemInfo.segments[0].address);
    ModuleInfoEx anyModule{};
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(&Query), 2, &anyModule) == 0 && anyModule.segment_count > 0 && anyModule.st_size == 0);
    Require(sceKernelGetModuleInfoFromAddr(systemAddress, 1, &anyModule) == 0 && anyModule.id == *systemModule);
    std::memset(&anyModule, 0xbb, sizeof(anyModule));
    Require(sceKernelGetModuleInfoFromAddr(systemAddress, 2, &anyModule) == SCE_KERNEL_ERROR_ESRCH && anyModule.segment_count == 0 && anyModule.name[0] == 0);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 1, &anyModule) == 0 && anyModule.id == info.id);
    ModuleInfoEx unset{};
    unset.st_size = 0xf0000;
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(add), 1, &unset) == 0 && unset.st_size == 0);
    const auto again = Query(add, 0);
    Require(again.eh_frame_hdr_addr == info.eh_frame_hdr_addr && again.eh_frame_hdr_size == info.eh_frame_hdr_size);
    Require(again.eh_frame_addr == info.eh_frame_addr && again.eh_frame_size == info.eh_frame_size);
    Require(dlclose_nid_postfix(module) == 0);
    void* reloaded = dlopen_nid_postfix(argv[1], 2);
    Require(reloaded != nullptr);
    const void* reloadedAdd = dlsym_nid_postfix(reloaded, "GuestModuleAdd");
    Require(reloadedAdd != nullptr);
    const auto fresh = Query(reloadedAdd, 0);
    Require(fresh.eh_frame_size == info.eh_frame_size && fresh.eh_frame_hdr_size == info.eh_frame_hdr_size);
    if (info.eh_frame_addr)
        Require(fresh.eh_frame_addr - fresh.segments[0].address == info.eh_frame_addr - info.segments[0].address);
    else Require(fresh.eh_frame_addr == 0);
    Require(dlclose_nid_postfix(reloaded) == 0);
}
