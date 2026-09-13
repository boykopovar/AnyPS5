#include "prx/libSceAgc/Patch/include/CondExec.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
 (void)cmd;
 (void)command;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
 (void)cmd;
 (void)buffer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    (void)cmd;
    (void)command;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    (void)cmd;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
