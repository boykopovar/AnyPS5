#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>

extern "C" int APS5_VABI sceKernelGetModuleInfoForUnwind(std::uint64_t address, int flags, ModuleInfoForUnwind* info);

namespace {

struct LookupLog {
    int calls = 0;
    std::uint64_t address = 0;
    int flags = -1;
    bool validStructure = false;
};

LookupLog lookups;

class LookupInterception {
public:
    LookupInterception() { lookups = {}; }
    ~LookupInterception() { lookups = {}; }
    LookupInterception(const LookupInterception&) = delete;
    LookupInterception& operator=(const LookupInterception&) = delete;
};

void GuestCodeAddress() {}

} // namespace

extern "C" int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info) {
    ++lookups.calls;
    lookups.address = address;
    lookups.flags = flags;
    lookups.validStructure = info != nullptr && info->st_size == sizeof(ModuleInfoEx);
    return SCE_KERNEL_ERROR_ESRCH;
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrowsWithMessage;

const Case guestLookupFails{"GetModuleInfoForUnwind_GuestModuleLookupFails_Throws", [] {
    const LookupInterception interception;
    const auto address = reinterpret_cast<std::uint64_t>(&GuestCodeAddress);
    RequireThrowsWithMessage<std::runtime_error>([address] {
        ModuleInfoForUnwind info{};
        sceKernelGetModuleInfoForUnwind(address, 1, &info);
    }, "sceKernelGetModuleInfoForUnwind: failed to query guest module information", "failed guest module lookup");
    RequireEqual(lookups.calls, 1, "guest module lookups");
    RequireEqual(lookups.address, address, "looked up address");
    RequireEqual(lookups.flags, 2, "lookup flags");
    Require(lookups.validStructure, "lookup received a ModuleInfoEx with the right structure size");
}};

const Case hostAddress{"GetModuleInfoForUnwind_HostAddress_SkipsGuestLookupAndReportsNoTables", [] {
    const LookupInterception interception;
    ModuleInfoForUnwind info{};
    RequireEqual(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&sceKernelGetModuleInfoForUnwind), 1, &info), 0,
                 "host query result");
    RequireEqual(lookups.calls, 0, "guest module lookups");
    RequireEqual(info.eh_frame_hdr_addr, std::uint64_t{0}, "eh_frame_hdr address");
    RequireEqual(info.eh_frame_addr, std::uint64_t{0}, "eh_frame address");
    RequireEqual(info.eh_frame_size, std::uint64_t{0}, "eh_frame size");
}};

} // namespace
