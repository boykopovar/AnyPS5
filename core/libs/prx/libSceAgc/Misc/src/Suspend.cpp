#include "prx/libSceAgc/Misc/include/Suspend.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"

#include <cstdint>
#include <exception>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// sceAgcSuspendPoint forwards to the driver's suspend-point facility, the same
// AgcDriverSuspendPoint_nid_postfix the queue workers and the frame recorder use
// (Execution/src/Driver/EntryPoints.cpp into Driver::SuspendPoint in
// Execution/src/Driver/Queues/Submission.cpp). The name is a real firmware export:
// the PS5 3.20 libSceAgc table binds it under NID h9z6+0hEydk, beside
// sceAgcSuspendPointAndCheckStatus under NID b+fis+WZ3Ig, and the firmware
// libSceAgcVsh module keeps sceAgcSuspendPoint among its reporter-trap exports.
// The driver header declares no SubmitCdbg facility and the only tree SubmitCdbg
// (sceAgcDriverSuspendPointSubmitCdbg in libSceAgcDriver/Unimplemented.cpp) is an
// untraced NotImplemented stub, so forwarding there would refuse every suspend;
// the plain driver suspend stays until a disassembly shows the firmware calling
// through a Cdbg export.
int APS5_VABI sceAgcSuspendPoint(void) {
    AgcDriverSuspendPoint_nid_postfix();
    return 0;
}

// The suspend point stays opaque to this library: only its readability is checked,
// with the non-throwing GuestMemory::Accessible query (the GuestShaderFields read
// guard pattern from issue #4782), before the real driver instance performs the
// suspend. An unreadable pointer throws APS5_INVALID_ARG_EX before the driver runs,
// as the null-shader halves do where the console would fault; only a driver refusal
// (which throws, e.g. a suspend requested from inside a suspend) returns
// GRAPHICS5_ERROR_SUSPEND_DISALLOWED. The one-argument shape and the firmware
// behaviour are unverified, see docs/dev/debt/libSceAgc.md.
// sceAgcDriverIsSuspendPointInFlight is not consulted because its body is an
// untraced stub.
int APS5_VABI sceAgcSuspendPointAndCheckStatus(void* suspendPoint) {
    if (!AgcDriver::GuestMemory::Accessible(suspendPoint, 1)) APS5_INVALID_ARG_EX;
    try {
        AgcDriverSuspendPoint_nid_postfix();
    } catch (const std::exception&) {
        return GRAPHICS5_ERROR_SUSPEND_DISALLOWED;
    }
    return 0;
}

int APS5_VABI sceAgcSetSubmitMode(std::uint32_t mode) {
    if (mode != 0) NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
