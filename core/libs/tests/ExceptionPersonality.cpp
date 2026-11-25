#include "prx/libc/include/exceptions/Unwind.hpp"
#include <cstdio>
#include <cstdlib>

namespace LibcUnwind {
_Unwind_Reason_Code CallPersonality(Word personality, _Unwind_Action actions, _Unwind_Exception* exception, _Unwind_Context* context);
}

static void Require(bool value) { if (!value) std::abort(); }

static const unsigned char cleanupTable[] = {0xff, 0xff, 0x01, 0x04, 0x10, 0x20, 0x40, 0x00};
static const unsigned char noLandingTable[] = {0xff, 0xff, 0x01, 0x04, 0x10, 0x20, 0x00, 0x00};

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

static _Unwind_Reason_Code Direct(Frame& frame, _Unwind_Action actions, int version = 1) {
    return __gcc_personality_v0_nid_postfix(version, actions, 0, &frame.exception, &frame.context);
}

static _Unwind_Reason_Code Routed(Frame& frame, _Unwind_Action actions) {
    return LibcUnwind::CallPersonality(reinterpret_cast<std::uintptr_t>(__gcc_personality_v0_nid_postfix), actions, &frame.exception, &frame.context);
}

static void RequireInstalled(Frame& frame) {
    Require(frame.context.registers[0] == reinterpret_cast<std::uintptr_t>(&frame.exception));
    Require(frame.context.registers[1] == 0);
    Require(frame.context.registers[16] == region + 0x40);
}

int main() {
    {
        Frame frame(cleanupTable, 0x18);
        Require(Direct(frame, _UA_CLEANUP_PHASE) == _URC_INSTALL_CONTEXT);
        RequireInstalled(frame);
    }
    {
        Frame frame(cleanupTable, 0x18);
        Require(Routed(frame, _UA_CLEANUP_PHASE) == _URC_INSTALL_CONTEXT);
        RequireInstalled(frame);
    }
    {
        Frame frame(cleanupTable, 0x18);
        Require(Direct(frame, _UA_SEARCH_PHASE) == _URC_CONTINUE_UNWIND);
        Require(frame.context.registers[16] == region + 0x18);
    }
    {
        Frame frame(cleanupTable, 0x18);
        Require(Routed(frame, _UA_SEARCH_PHASE) == _URC_CONTINUE_UNWIND);
    }
    {
        Frame before(cleanupTable, 0x10);
        Require(Direct(before, _UA_CLEANUP_PHASE) == _URC_CONTINUE_UNWIND);
        Frame after(cleanupTable, 0x31);
        Require(Direct(after, _UA_CLEANUP_PHASE) == _URC_CONTINUE_UNWIND);
    }
    {
        Frame frame(cleanupTable, 0x10, true);
        Require(Direct(frame, _UA_CLEANUP_PHASE) == _URC_INSTALL_CONTEXT);
        RequireInstalled(frame);
    }
    {
        Frame frame(noLandingTable, 0x18);
        Require(Direct(frame, _UA_CLEANUP_PHASE) == _URC_CONTINUE_UNWIND);
    }
    {
        Frame frame(nullptr, 0x18);
        Require(Direct(frame, _UA_CLEANUP_PHASE) == _URC_CONTINUE_UNWIND);
    }
    {
        Frame frame(cleanupTable, 0x18);
        Require(Direct(frame, _UA_CLEANUP_PHASE, 2) == _URC_FATAL_PHASE1_ERROR);
    }
    {
        Frame frame(cleanupTable, 0x18);
        Require(LibcUnwind::CallPersonality(0x1000, _UA_SEARCH_PHASE, &frame.exception, &frame.context) == _URC_FATAL_PHASE1_ERROR);
        Require(LibcUnwind::CallPersonality(0x1000, _UA_CLEANUP_PHASE, &frame.exception, &frame.context) == _URC_FATAL_PHASE2_ERROR);
    }
    {
        static constexpr std::uint64_t NativeGnuExceptionClass = 0x474e5543432b2b00ULL;
        static constexpr std::uint64_t NativeClangExceptionClass = 0x434c4e47432b2b00ULL;
        static const unsigned char PositiveFilterNoTypeTableLsda[] = {
            0xff, 0xff, 0x01, 0x04,
            0x00, 0x20, 0x08, 0x01,
            0x01, 0x00
        };
        static const unsigned char CyclicActionLsda[] = {
            0xff, 0xff, 0x01, 0x04,
            0x00, 0x20, 0x08, 0x01,
            0x00, 0x7f
        };
        static const unsigned char EmptySpecLsda[] = {
            0xff, 0x03, 0x08, 0x01, 0x04,
            0x00, 0x20, 0x08, 0x01,
            0x7f, 0x00,
            0x00
        };

        Frame noLsda(nullptr, 0x10);
        Frame cleanup(cleanupTable, 0x18);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, NativeGnuExceptionClass, nullptr, &cleanup.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE, NativeGnuExceptionClass, nullptr, &cleanup.context) == _URC_FATAL_PHASE2_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, NativeClangExceptionClass, nullptr, &cleanup.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE, NativeClangExceptionClass, nullptr, &cleanup.context) == _URC_FATAL_PHASE2_ERROR);

        Require(__gxx_personality_v0_nid_postfix(1, 0, 0, &noLsda.exception, &noLsda.context) == _URC_FATAL_PHASE2_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE | _UA_CLEANUP_PHASE, 0, &noLsda.exception, &noLsda.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE | _UA_HANDLER_FRAME, 0, &noLsda.exception, &noLsda.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE | _UA_FORCE_UNWIND, 0, &noLsda.exception, &noLsda.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE | _UA_HANDLER_FRAME | _UA_FORCE_UNWIND, 0, &noLsda.exception, &noLsda.context) == _URC_FATAL_PHASE2_ERROR);

        Require(Direct(cleanup, 0) == _URC_FATAL_PHASE2_ERROR);
        Require(Direct(cleanup, _UA_SEARCH_PHASE | _UA_CLEANUP_PHASE) == _URC_FATAL_PHASE1_ERROR);
        Require(Direct(cleanup, _UA_SEARCH_PHASE | _UA_HANDLER_FRAME) == _URC_FATAL_PHASE1_ERROR);
        Require(Direct(cleanup, _UA_SEARCH_PHASE | _UA_FORCE_UNWIND) == _URC_FATAL_PHASE1_ERROR);
        Require(Direct(cleanup, _UA_CLEANUP_PHASE | _UA_HANDLER_FRAME) == _URC_FATAL_PHASE2_ERROR);
        Require(Direct(cleanup, _UA_CLEANUP_PHASE | _UA_HANDLER_FRAME | _UA_FORCE_UNWIND) == _URC_FATAL_PHASE2_ERROR);

        Frame positiveNoTable(PositiveFilterNoTypeTableLsda, 0x10);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, 0, &positiveNoTable.exception, &positiveNoTable.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE, 0, &positiveNoTable.exception, &positiveNoTable.context) == _URC_FATAL_PHASE2_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE | _UA_FORCE_UNWIND, 0, &positiveNoTable.exception, &positiveNoTable.context) == _URC_CONTINUE_UNWIND);

        Frame cyclic(CyclicActionLsda, 0x10);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, 0, &cyclic.exception, &cyclic.context) == _URC_FATAL_PHASE1_ERROR);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE, 0, &cyclic.exception, &cyclic.context) == _URC_FATAL_PHASE2_ERROR);

        Frame emptySpec(EmptySpecLsda, 0x10);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, 0, &emptySpec.exception, &emptySpec.context) == _URC_HANDLER_FOUND);
        Require(__gxx_personality_v0_nid_postfix(1, _UA_CLEANUP_PHASE | _UA_HANDLER_FRAME, 0, &emptySpec.exception, &emptySpec.context) == _URC_INSTALL_CONTEXT);
        Require(emptySpec.context.registers[1] == static_cast<LibcUnwind::Word>(-1));
        Require(emptySpec.context.registers[16] == region + 0x08);
    }
    std::puts("exception personality tests passed");
    return 0;
}
