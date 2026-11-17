#include "prx/libc/include/exceptions/Unwind.hpp"
#include <cstdio>
#include <cstdlib>

#ifdef __APPLE__
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstring>
#endif

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

#ifdef __APPLE__
static void CheckHostCatch(bool guestRethrow) {
    void* retained = nullptr;
    try {
        _ZSt6_ThrowRKSt20bad_array_new_length_nid_postfix(nullptr);
    } catch (...) {
        const auto* caught = __cxa_current_exception_type_nid_postfix();
        Require(caught != nullptr && std::strcmp(caught->name(), "St20bad_array_new_length") == 0);
        Require(__cxa_uncaught_exceptions_nid_postfix() == 0);
        retained = __cxa_current_primary_exception_nid_postfix();
        Require(retained != nullptr);
    }
    Require(__cxa_current_exception_type_nid_postfix() == nullptr);
    Require(__cxa_current_primary_exception_nid_postfix() == nullptr);

    try {
        try {
            __cxa_rethrow_primary_exception_nid_postfix(retained);
            std::abort();
        } catch (...) {
            Require(__cxa_current_exception_type_nid_postfix() != nullptr);
            if (guestRethrow) __cxa_rethrow_nid_postfix();
            throw;
        }
    } catch (...) {
        const auto* caught = __cxa_current_exception_type_nid_postfix();
        Require(caught != nullptr && std::strcmp(caught->name(), "St20bad_array_new_length") == 0);
        Require(__cxa_uncaught_exceptions_nid_postfix() == 0);
        void* primary = __cxa_current_primary_exception_nid_postfix();
        Require(primary == retained);
        __cxa_decrement_exception_refcount_nid_postfix(primary);
    }
    Require(__cxa_current_exception_type_nid_postfix() == nullptr);
    Require(__cxa_current_primary_exception_nid_postfix() == nullptr);
    Require(__cxa_uncaught_exceptions_nid_postfix() == 0);
    __cxa_decrement_exception_refcount_nid_postfix(retained);
}
#endif

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
#ifdef __APPLE__
    CheckHostCatch(false);
    CheckHostCatch(true);
#endif
    std::puts("exception personality tests passed");
    return 0;
}
