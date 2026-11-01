#include "prx/libc/include/exceptions/Unwind.hpp"
#include "prx/libc/src/specifics/x86_64/RegisterContext.cpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#define CURSOR_TEST_EXPORT [[gnu::dllexport]]
#else
#define CURSOR_TEST_EXPORT [[gnu::visibility("default")]]
#endif

extern "C" _Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn trace, void* argument);
extern "C" std::uintptr_t APS5_VABI _Unwind_GetRegionStart_nid_postfix(_Unwind_Context* context);
extern "C" std::uintptr_t APS5_VABI _Unwind_GetLanguageSpecificData_nid_postfix(_Unwind_Context* context);
extern "C" [[gnu::noinline]] CURSOR_TEST_EXPORT std::uintptr_t UnwindCursorProbe(int mode);
extern "C" [[gnu::noinline]] CURSOR_TEST_EXPORT std::uintptr_t UnwindCursorCaller(int mode);

namespace {

struct ProcedureInfo {
    std::uintptr_t start;
    std::uintptr_t end;
    std::uintptr_t lsda;
    std::uintptr_t handler;
    std::uintptr_t gp;
    std::uintptr_t flags;
    std::uint32_t format;
    std::uint32_t unwindInfoSize;
    std::uintptr_t unwindInfo;
    std::uintptr_t extra;
};

using Destroy = void (APS5_VABI *)(_Unwind_Context*);
using ValidRegister = bool (APS5_VABI *)(_Unwind_Context*, int);
using GetRegister = std::uintptr_t (APS5_VABI *)(_Unwind_Context*, int);
using SetRegister = void (APS5_VABI *)(_Unwind_Context*, int, std::uintptr_t);
using GetFloatRegister = double (APS5_VABI *)(_Unwind_Context*, int);
using SetFloatRegister = void (APS5_VABI *)(_Unwind_Context*, int, double);
using Step = int (APS5_VABI *)(_Unwind_Context*, bool);
using GetInfo = void (APS5_VABI *)(_Unwind_Context*, ProcedureInfo*);
using JumpTo = void (APS5_VABI *)(_Unwind_Context*);
using IsSignalFrame = bool (APS5_VABI *)(_Unwind_Context*);
using GetFunctionName = bool (APS5_VABI *)(_Unwind_Context*, char*, std::size_t, std::uintptr_t*);
using SetInfo = void (APS5_VABI *)(_Unwind_Context*, bool);
using GetRegisterName = const char* (APS5_VABI *)(_Unwind_Context*, int);

enum Mode { Checks, GetUnknownRegister, SetUnknownRegister, GetFloat, SetFloat };

volatile int jumps = 0;

template<class Function> Function Slot(const _Unwind_Context& context, int index) {
    return reinterpret_cast<Function>(context.cursor[index]);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "unwind cursor check failed: %s\n", message);
    std::exit(1);
}

ProcedureInfo Info(_Unwind_Context& context) {
    ProcedureInfo info;
    std::memset(&info, 0xa5, sizeof(info));
    Slot<GetInfo>(context, 9)(&context, &info);
    return info;
}

std::string FunctionName(_Unwind_Context& context, std::size_t length, std::uintptr_t& offset) {
    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    offset = 0;
    if (!Slot<GetFunctionName>(context, 12)(&context, buffer, length, &offset)) return {};
    return buffer;
}

void CheckRegisters(_Unwind_Context& context) {
    const auto valid = Slot<ValidRegister>(context, 2);
    const auto get = Slot<GetRegister>(context, 3);
    const auto set = Slot<SetRegister>(context, 4);
    Require(valid(&context, -1) && valid(&context, -2) && valid(&context, 0) && valid(&context, 16), "registers -2 to 16 are valid");
    Require(!valid(&context, -3) && !valid(&context, 17), "registers outside -2 to 16 are not valid");
    Require(get(&context, -1) == context.registers[16] && get(&context, 16) == context.registers[16], "UNW_REG_IP and rip read the instruction pointer");
    Require(get(&context, -2) == context.registers[7] && get(&context, 7) == context.registers[7], "UNW_REG_SP and rsp read the stack pointer");
    const auto rbx = get(&context, 3);
    set(&context, 3, rbx ^ 1);
    Require(context.registers[3] == (rbx ^ 1), "setReg writes rbx");
    set(&context, 3, rbx);
    Require(!Slot<ValidRegister>(context, 5)(&context, 17), "no float register is valid");
    Require(!Slot<IsSignalFrame>(context, 11)(&context), "a captured frame is not a signal frame");
}

void CheckRegisterNames(_Unwind_Context& context) {
    const auto name = Slot<GetRegisterName>(context, 14);
    const auto named = [&](int number, const char* expected) { return std::strcmp(name(&context, number), expected) == 0; };
    Require(named(-1, "rip") && named(-2, "rsp") && named(0, "rax") && named(3, "rbx") && named(7, "rsp") && named(15, "r15") && named(16, "rip"),
        "general register names");
    Require(named(17, "xmm0") && named(32, "xmm15") && named(33, "unknown register") && named(-3, "unknown register"), "vector and unknown register names");
}

void CheckInfo(_Unwind_Context& context, std::uintptr_t probe) {
    const auto ip = context.registers[16];
    const auto info = Info(context);
    Require(info.start == probe && info.start < ip && ip < info.end, "getInfo gives the range of the function holding the IP");
    Require(info.unwindInfo != 0 && info.format == 0x04000000 && info.flags == 0, "getInfo gives the FDE in DWARF mode");
    std::uint32_t length;
    std::memcpy(&length, reinterpret_cast<const void*>(info.unwindInfo), sizeof(length));
    Require(info.unwindInfoSize == length + 4, "the unwind info size covers the whole FDE");
    Require(info.extra != 0 && info.extra <= info.start, "getInfo gives the start of the segment holding the function");
    Require(info.start == _Unwind_GetRegionStart_nid_postfix(&context) && info.lsda == _Unwind_GetLanguageSpecificData_nid_postfix(&context),
        "getInfo agrees with _Unwind_GetRegionStart and _Unwind_GetLanguageSpecificData");
}

_Unwind_Reason_Code APS5_VABI TraceFirstFrame(_Unwind_Context* context, void* argument) {
    *static_cast<ProcedureInfo*>(argument) = Info(*context);
    return _URC_NORMAL_STOP;
}

void CheckCursor(_Unwind_Context& context, const ProcedureInfo& traced) {
    const auto probe = reinterpret_cast<std::uintptr_t>(&UnwindCursorProbe);
    const auto caller = reinterpret_cast<std::uintptr_t>(&UnwindCursorCaller);
    const auto ip = context.registers[16];
    const auto set = Slot<SetRegister>(context, 4);
    const auto setInfo = Slot<SetInfo>(context, 13);
    const auto step = Slot<Step>(context, 8);
    setInfo(&context, true);
    CheckInfo(context, probe);
    CheckRegisters(context);
    CheckRegisterNames(context);

    std::uintptr_t offset = 0;
    Require(FunctionName(context, 64, offset) == "UnwindCursorProbe" && offset == ip - probe, "getFunctionName names the exported probe");
    Require(FunctionName(context, 8, offset) == "UnwindC" && offset == ip - probe, "getFunctionName truncates the name to the buffer");

    set(&context, -1, caller);
    setInfo(&context, false);
    Require(Info(context).start == caller, "setInfoBasedOnIPRegister(false) looks the new IP up as it is");
    set(&context, -1, 0);
    setInfo(&context, false);
    const ProcedureInfo none {};
    const auto missing = Info(context);
    Require(std::memcmp(&missing, &none, sizeof(none)) == 0, "getInfo is all zero without unwind info");
    Require(step(&context, false) == 0 && context.registers[16] == 0, "step ends without moving when there is no unwind info");
    set(&context, -1, ip);
    setInfo(&context, true);
    CheckInfo(context, probe);

    _Unwind_Context walk = context;
    Require(step(&walk, false) == 1, "step moves to the caller");
    Require(Info(walk).start == caller && walk.registers[7] > context.registers[7], "step reaches UnwindCursorCaller");
    Require(FunctionName(walk, 64, offset) == "UnwindCursorCaller" && offset == walk.registers[16] - caller, "getFunctionName names the caller");
    int frames = 1;
    int result;
    while ((result = step(&walk, false)) == 1 && frames < 4096) ++frames;
    Require(result == 0 && frames >= 2, "step reaches the end of the stack");
    Require(traced.start == probe && traced.end == Info(context).end, "_Unwind_Backtrace passes a cursor that describes its frame");

    Slot<Destroy>(context, 0)(&context);
    Slot<Destroy>(context, 1)(&context);
    Require(context.registers[16] == ip && Info(context).start == probe, "the destructors leave the cursor alone");
}

[[gnu::noinline]] void JumpBack(_Unwind_Context& context) {
    Slot<JumpTo>(context, 10)(&context);
}

[[gnu::noinline]] void CheckJumpTo() {
    _Unwind_Context context;
    LibcCaptureRegisters(context.registers);
    const int seen = jumps;
    jumps = seen + 1;
    if (seen == 0) JumpBack(context);
    Require(jumps == 2, "jumpto resumes at the captured registers");
}

}

extern "C" std::uintptr_t UnwindCursorProbe(int mode) {
    _Unwind_Context context;
    LibcCaptureRegisters(context.registers);
    ProcedureInfo traced {};
    switch (mode) {
    case Checks:
        Require(_Unwind_Backtrace_nid_postfix(TraceFirstFrame, &traced) == _URC_NORMAL_STOP, "_Unwind_Backtrace stops when the callback asks");
        CheckCursor(context, traced);
        break;
    case GetUnknownRegister: Slot<GetRegister>(context, 3)(&context, 17); break;
    case SetUnknownRegister: Slot<SetRegister>(context, 4)(&context, 17, 0); break;
    case GetFloat: Slot<GetFloatRegister>(context, 6)(&context, 17); break;
    case SetFloat: Slot<SetFloatRegister>(context, 7)(&context, 17, 1.0); break;
    }
    return context.registers[16];
}

extern "C" std::uintptr_t UnwindCursorCaller(int mode) {
    return UnwindCursorProbe(mode) + 1;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
    std::signal(SIGABRT, [](int) { std::_Exit(3); });
#endif
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "get-register") UnwindCursorCaller(GetUnknownRegister);
    else if (mode == "set-register") UnwindCursorCaller(SetUnknownRegister);
    else if (mode == "get-float-register") UnwindCursorCaller(GetFloat);
    else if (mode == "set-float-register") UnwindCursorCaller(SetFloat);
    else {
        Require(mode.empty(), "unknown mode");
        UnwindCursorCaller(Checks);
        CheckJumpTo();
        std::puts("unwind cursor tests passed");
        return 0;
    }
    std::fprintf(stderr, "the cursor returned from %s\n", mode.c_str());
    return 1;
}
