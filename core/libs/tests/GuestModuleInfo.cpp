#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t, int, ModuleInfoEx*);
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
    Require(info.eh_frame_hdr_addr != 0 && info.eh_frame_hdr_size != 0 && info.eh_frame_addr != 0 && info.eh_frame_size != 0);
    Require(info.init_proc_addr == 0 || info.init_proc_addr >= info.segments[0].address);
    const auto self = Query(reinterpret_cast<const void*>(&Require), 0);
    Require(self.id != info.id && Query(reinterpret_cast<const void*>(&Query), 0).id == self.id);
    void* second = dlopen_nid_postfix(argv[1], 2);
    Require(second && Query(add, 0).id == info.id);
    Require(dlclose_nid_postfix(second) == 0);
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
