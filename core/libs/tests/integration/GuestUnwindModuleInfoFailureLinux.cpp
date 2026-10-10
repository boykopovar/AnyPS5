#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

extern "C" int APS5_VABI sceKernelGetModuleInfoForUnwind(std::uint64_t address, int flags, ModuleInfoForUnwind* info);

static std::uint64_t expectedAddress = 0;
static int calls = 0;

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

extern "C" int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info) {
    Require(address == expectedAddress, "module lookup received the wrong address");
    Require(flags == 2, "module lookup received the wrong flags");
    Require(info != nullptr && info->st_size == sizeof(ModuleInfoEx), "module lookup received the wrong structure size");
    ++calls;
    return SCE_KERNEL_ERROR_ESRCH;
}

int main() {
    try {
        expectedAddress = reinterpret_cast<std::uint64_t>(&Require);
        ModuleInfoForUnwind info{};
        bool threw = false;
        try {
            sceKernelGetModuleInfoForUnwind(expectedAddress, 1, &info);
        } catch (const std::runtime_error& error) {
            threw = std::string_view(error.what()) == "sceKernelGetModuleInfoForUnwind: failed to query guest module information";
        }
        Require(calls == 1, "guest module lookup was not intercepted exactly once");
        Require(threw, "failed guest module lookup must throw instead of reporting success");
        Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&sceKernelGetModuleInfoForUnwind), 1, &info) == 0, "host query failed");
        Require(calls == 1, "host query attempted a guest module lookup");
        Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0, "host query reported guest unwind tables");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
