#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_ITANIUM_UNWINDABI_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_ITANIUM_UNWINDABI_HPP

#include <cstdint>

using _Unwind_Word = std::uintptr_t;
using _Unwind_Ptr = std::uintptr_t;
using _Unwind_Reason_Code = int;
using _Unwind_Action = int;

static constexpr _Unwind_Reason_Code _URC_NO_REASON = 0;
static constexpr _Unwind_Reason_Code _URC_FOREIGN_EXCEPTION_CAUGHT = 1;
static constexpr _Unwind_Reason_Code _URC_FATAL_PHASE2_ERROR = 2;
static constexpr _Unwind_Reason_Code _URC_FATAL_PHASE1_ERROR = 3;
static constexpr _Unwind_Reason_Code _URC_NORMAL_STOP = 4;
static constexpr _Unwind_Reason_Code _URC_END_OF_STACK = 5;
static constexpr _Unwind_Reason_Code _URC_HANDLER_FOUND = 6;
static constexpr _Unwind_Reason_Code _URC_INSTALL_CONTEXT = 7;
static constexpr _Unwind_Reason_Code _URC_CONTINUE_UNWIND = 8;

static constexpr _Unwind_Action _UA_SEARCH_PHASE = 1;
static constexpr _Unwind_Action _UA_CLEANUP_PHASE = 2;
static constexpr _Unwind_Action _UA_HANDLER_FRAME = 4;
static constexpr _Unwind_Action _UA_FORCE_UNWIND = 8;
static constexpr _Unwind_Action _UA_END_OF_STACK = 16;

struct _Unwind_Exception {
    std::uint64_t exception_class;
    void (*exception_cleanup)(_Unwind_Reason_Code, _Unwind_Exception*);
    std::uintptr_t private_1;
    std::uintptr_t private_2;
};

struct _Unwind_Context;

using _Unwind_Trace_Fn = _Unwind_Reason_Code (*)(_Unwind_Context*, void*);
using _Unwind_Stop_Fn = _Unwind_Reason_Code (*)(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*, void*);

#endif
