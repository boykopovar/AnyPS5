#include "prx/libc/include/exceptions/Unwind.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

namespace LibcUnwind {
_Unwind_Reason_Code CallPersonality(Word personality, _Unwind_Action actions, _Unwind_Exception* exception, _Unwind_Context* context);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

const unsigned char cleanupTable[] = {0xff, 0xff, 0x01, 0x04, 0x10, 0x20, 0x40, 0x00};
const unsigned char noLandingTable[] = {0xff, 0xff, 0x01, 0x04, 0x10, 0x20, 0x00, 0x00};

constexpr std::uintptr_t region = 0x10000;

struct Frame {
    _Unwind_Context context {};
    _Unwind_Exception exception {};

    Frame(const unsigned char* table, std::uintptr_t resume, bool signalFrame = false) {
        context.region = region;
        context.lsda = reinterpret_cast<std::uintptr_t>(table);
        context.registers[16] = region + resume;
        context.signalFrame = signalFrame;
    }
};

_Unwind_Reason_Code Direct(Frame& frame, _Unwind_Action actions, int version = 1) {
    return __gcc_personality_v0_nid_postfix(version, actions, 0, &frame.exception, &frame.context);
}

_Unwind_Reason_Code Routed(Frame& frame, _Unwind_Action actions) {
    return LibcUnwind::CallPersonality(reinterpret_cast<std::uintptr_t>(__gcc_personality_v0_nid_postfix), actions, &frame.exception, &frame.context);
}

void RequireInstalled(const Frame& frame) {
    RequireEqual(frame.context.registers[0], reinterpret_cast<std::uintptr_t>(&frame.exception), "exception register");
    RequireEqual(frame.context.registers[1], std::uintptr_t {0}, "selector register");
    RequireEqual(frame.context.registers[16], region + 0x40, "landing pad address");
}

const Case directCleanup{"GccPersonality_CleanupPhaseInsideCallSite_InstallsLandingPad", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(Direct(frame, _UA_CLEANUP_PHASE), _URC_INSTALL_CONTEXT, "cleanup phase result");
    RequireInstalled(frame);
}};

const Case routedCleanup{"CallPersonality_CleanupPhaseInsideCallSite_InstallsLandingPad", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(Routed(frame, _UA_CLEANUP_PHASE), _URC_INSTALL_CONTEXT, "cleanup phase result");
    RequireInstalled(frame);
}};

const Case directSearch{"GccPersonality_SearchPhase_ContinuesUnwindAndKeepsIp", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(Direct(frame, _UA_SEARCH_PHASE), _URC_CONTINUE_UNWIND, "search phase result");
    RequireEqual(frame.context.registers[16], region + 0x18, "instruction pointer");
}};

const Case routedSearch{"CallPersonality_SearchPhase_ContinuesUnwind", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(Routed(frame, _UA_SEARCH_PHASE), _URC_CONTINUE_UNWIND, "search phase result");
}};

const Case outsideCallSite{"GccPersonality_IpOutsideCallSite_ContinuesUnwind", [] {
    for (const std::uintptr_t resume : {std::uintptr_t {0x10}, std::uintptr_t {0x31}}) {
        Frame frame(cleanupTable, resume);
        RequireEqual(Direct(frame, _UA_CLEANUP_PHASE), _URC_CONTINUE_UNWIND, "resume offset " + std::to_string(resume));
    }
}};

const Case signalFrame{"GccPersonality_SignalFrameAtCallSiteStart_InstallsLandingPad", [] {
    Frame frame(cleanupTable, 0x10, true);
    RequireEqual(Direct(frame, _UA_CLEANUP_PHASE), _URC_INSTALL_CONTEXT, "cleanup phase result");
    RequireInstalled(frame);
}};

const Case noLandingPad{"GccPersonality_CallSiteWithoutLandingPad_ContinuesUnwind", [] {
    Frame frame(noLandingTable, 0x18);
    RequireEqual(Direct(frame, _UA_CLEANUP_PHASE), _URC_CONTINUE_UNWIND, "cleanup phase result");
}};

const Case noLsda{"GccPersonality_NullLsda_ContinuesUnwind", [] {
    Frame frame(nullptr, 0x18);
    RequireEqual(Direct(frame, _UA_CLEANUP_PHASE), _URC_CONTINUE_UNWIND, "cleanup phase result");
}};

const Case wrongVersion{"GccPersonality_UnsupportedVersion_FailsWithPhase1Error", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(Direct(frame, _UA_CLEANUP_PHASE, 2), _URC_FATAL_PHASE1_ERROR, "version 2 result");
}};

const Case invalidSearch{"CallPersonality_InvalidPersonalityInSearchPhase_FailsWithPhase1Error", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(LibcUnwind::CallPersonality(0x1000, _UA_SEARCH_PHASE, &frame.exception, &frame.context),
        _URC_FATAL_PHASE1_ERROR, "search phase result");
}};

const Case invalidCleanup{"CallPersonality_InvalidPersonalityInCleanupPhase_FailsWithPhase2Error", [] {
    Frame frame(cleanupTable, 0x18);
    RequireEqual(LibcUnwind::CallPersonality(0x1000, _UA_CLEANUP_PHASE, &frame.exception, &frame.context),
        _URC_FATAL_PHASE2_ERROR, "cleanup phase result");
}};

} // namespace
