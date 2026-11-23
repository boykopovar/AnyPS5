#ifdef _WIN32
#include "prx/libc/include/RuntimeThrowHandoff.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"
#include <cxxabi.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <typeinfo>

namespace {

constexpr DWORD GccThrow = 0x20474343u;
constexpr std::uint64_t GnuPrimaryClass = 0x474e5543432b2b00ull;
constexpr std::size_t SearchObjectBytes = 512;

struct alignas(16) RuntimeUnwindException {
    std::uint64_t exceptionClass;
    void (*cleanup)(int, void*);
    std::uintptr_t privateWords[6];
};

struct RuntimeCxaException {
    std::type_info* type;
    void (*destructor)(void*);
    void (*unexpected)();
    void (*terminate)();
    RuntimeCxaException* next;
    int handlerCount;
    int handlerSwitchValue;
    const unsigned char* actionRecord;
    const unsigned char* languageSpecificData;
    void* catchTemp;
    void* adjusted;
    RuntimeUnwindException unwind;
};

struct RuntimeRefcountedException {
    int references;
    RuntimeCxaException exception;
};

static_assert(sizeof(RuntimeUnwindException) == 64);
static_assert(offsetof(RuntimeCxaException, unwind) == 80 && sizeof(RuntimeCxaException) == 144);
static_assert(offsetof(RuntimeRefcountedException, exception) == 16 && sizeof(RuntimeRefcountedException) == 160);

constexpr std::size_t RuntimeHeaderBytes = sizeof(RuntimeRefcountedException);
constexpr std::size_t RuntimeUnwindBytes = sizeof(RuntimeUnwindException);
constexpr std::ptrdiff_t UnwindToAllocation = offsetof(RuntimeRefcountedException, exception) + offsetof(RuntimeCxaException, unwind);
constexpr std::ptrdiff_t UnwindToType = offsetof(RuntimeCxaException, unwind) - offsetof(RuntimeCxaException, type);
constexpr std::ptrdiff_t UnwindToDestructor = offsetof(RuntimeCxaException, unwind) - offsetof(RuntimeCxaException, destructor);
constexpr std::ptrdiff_t UnwindToPrivate = offsetof(RuntimeUnwindException, privateWords);
constexpr std::ptrdiff_t AllocationToType = offsetof(RuntimeRefcountedException, exception) + offsetof(RuntimeCxaException, type);
static_assert(UnwindToAllocation == 96 && UnwindToType == 80 && UnwindToDestructor == 72 && UnwindToPrivate == 16 && AllocationToType == 16);

struct Handoff {
    CONTEXT scratch;
    EXCEPTION_RECORD record;
    std::uint8_t* unwind;
    std::size_t objectBytes;
    DWORD64 targetFrame;
    DWORD64 targetRip;
    DWORD64 targetRsp;
    bool active;
    bool targetCleaned;
};

struct RuntimeGlobals {
    void* caught;
    unsigned int uncaught;
};

struct alignas(16) SearchException {
    LibcException::Allocation allocation;
    unsigned char object[SearchObjectBytes];
};

thread_local Handoff* handoff;

using FreeException = void (*)(void*);
using GetGlobals = RuntimeGlobals* (*)();
using AllocationSize = std::size_t (*)(void*);

HMODULE RuntimeModule() { return GetModuleHandleA("libstdc++-6.dll"); }

HMODULE ModuleOf(DWORD64 address) {
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module);
    return module;
}

bool ReadBytes(const void* address, void* value, std::size_t size) {
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, value, size, &copied) && copied == size;
}

template<class T> bool Read(const std::uint8_t* address, T& value) { return ReadBytes(address, &value, sizeof(value)); }

std::size_t HeapAllocationBytes(void* allocation) {
    if (ModuleOf(reinterpret_cast<DWORD64>(allocation)) != nullptr) return 0;
    const auto size = reinterpret_cast<AllocationSize>(GetProcAddress(GetModuleHandleA("ucrtbase.dll"), "_msize"));
    if (!size) return 0;
    const auto bytes = size(allocation);
    return bytes == static_cast<std::size_t>(-1) ? 0 : bytes;
}

void PrepareProbe(SearchException& probe, std::type_info* type, const std::uint8_t* object, std::size_t objectBytes) {
    probe.allocation.header.type = type;
    probe.allocation.header.unwind.exception_class = LibcException::PrimaryClass;
    std::memcpy(probe.object, object, objectBytes);
}

_Unwind_Reason_Code RuntimeFramePersonality(SearchException& probe, _Unwind_Action action, DWORD64 pc, DWORD64 imageBase, const RUNTIME_FUNCTION* entry, PVOID lsda, DWORD64* landing) {
    _Unwind_Context context {};
    context.registers[16] = pc;
    context.lsda = reinterpret_cast<std::uintptr_t>(lsda);
    context.region = imageBase + entry->BeginAddress;
    context.textBase = imageBase;
    auto& header = probe.allocation.header;
    const auto result = __gxx_personality_v0_nid_postfix(1, action, header.unwind.exception_class, &header.unwind, &context);
    if (landing) *landing = context.registers[16];
    return result;
}

bool RuntimeFrameCatches(SearchException& probe, DWORD64 pc, DWORD64 imageBase, const RUNTIME_FUNCTION* entry, PVOID lsda) {
    return RuntimeFramePersonality(probe, _UA_SEARCH_PHASE, pc, imageBase, entry, lsda, nullptr) != _URC_CONTINUE_UNWIND;
}

bool FindTarget(const CONTEXT& start, Handoff& state, SearchException& probe) {
    const HMODULE runtime = RuntimeModule();
    const HMODULE system = GetModuleHandleA("KERNELBASE.dll");
    const HMODULE modules[] = {system, GetModuleHandleA("libgcc_s_seh-1.dll"), runtime};
    const auto runtimePersonality = reinterpret_cast<PEXCEPTION_ROUTINE>(GetProcAddress(runtime, "__gxx_personality_seh0"));
    if (!runtimePersonality) return false;
    CONTEXT context = start;
    bool sawRuntime = false;
    for (int depth = 0; depth < 64 && context.Rip != 0; ++depth) {
        const HMODULE module = ModuleOf(context.Rip);
        DWORD64 imageBase = 0;
        const auto entry = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        bool known = false;
        for (const auto candidate : modules) known = known || (module != nullptr && module == candidate);
        if (!known) {
            const std::uintptr_t registers[17] = {context.Rax, context.Rdx, context.Rcx, context.Rbx, context.Rsi, context.Rdi, context.Rbp, context.Rsp,
                context.R8, context.R9, context.R10, context.R11, context.R12, context.R13, context.R14, context.R15, context.Rip};
            return sawRuntime && LibcUnwindHasFrame_nid_no_patch(registers);
        }
        if (entry == nullptr) return false;
        sawRuntime = sawRuntime || module == runtime;
        const DWORD64 pc = context.Rip;
        state.targetRip = context.Rip;
        state.targetRsp = context.Rsp;
        PVOID handlerData = nullptr;
        DWORD64 establisher = 0;
        const auto handler = RtlVirtualUnwind(UNW_FLAG_EHANDLER, imageBase, pc, entry, &context, &handlerData, &establisher, nullptr);
        if (handler != nullptr && module != system && (handler != runtimePersonality || RuntimeFrameCatches(probe, pc, imageBase, entry, handlerData))) return false;
        state.targetFrame = establisher;
    }
    return false;
}

}

extern "C" {
extern const char RuntimeHandoffUnwind_nid_no_patch[];
extern const char RuntimeHandoffLanded_nid_no_patch[];
[[noreturn]] void RuntimeHandoffDeliver_nid_no_patch(const CONTEXT* landed, std::uint8_t* unwind, DWORD64 landedRsp);
}

asm(".text\n"
    ".seh_proc RuntimeHandoffUnwind_nid_no_patch\n"
    "RuntimeHandoffUnwind_nid_no_patch:\n"
    "    pushq %rax\n"
    "    subq $0x48, %rsp\n"
    ".seh_stackalloc 0x48\n"
    ".seh_endprologue\n"
    "    movq %r10, 0x20(%rsp)\n"
    "    movq %r11, 0x28(%rsp)\n"
    "    callq *__imp_RtlUnwindEx(%rip)\n"
    "    int3\n"
    ".seh_endproc\n"
    "RuntimeHandoffLanded_nid_no_patch:\n"
    "    movq %rsp, %r10\n"
    "    andq $-16, %rsp\n"
    "    subq $0x520, %rsp\n"
    "    movq %rax, 0x510(%rsp)\n"
    "    movq %r10, 0x518(%rsp)\n"
    "    leaq 0x20(%rsp), %rcx\n"
    "    callq *__imp_RtlCaptureContext(%rip)\n"
    "    leaq 0x20(%rsp), %rcx\n"
    "    movq 0x510(%rsp), %rdx\n"
    "    movq 0x518(%rsp), %r8\n"
    "    callq RuntimeHandoffDeliver_nid_no_patch\n"
    "    int3\n");

namespace {

[[noreturn, gnu::noinline]] void ThrowInProject(void* object, std::type_info* type, void (*destructor)(void*)) {
    __cxxabiv1::__cxa_throw(object, type, destructor);
}

[[noreturn]] void HandoffFailed(const char* reason) {
    std::fprintf(stderr, "\nFATAL: cannot hand a C++ exception from libstdc++-6.dll to libc: %s\n", reason);
    std::fflush(stderr);
    std::abort();
}

}

extern "C" [[noreturn]] void RuntimeHandoffDeliver_nid_no_patch(const CONTEXT* landed, std::uint8_t* unwind, DWORD64 landedRsp) {
    Handoff* state = handoff;
    if (!state || !state->active || state->unwind != unwind) HandoffFailed("the landing does not match the pending handoff");
    if (landedRsp != state->targetRsp) HandoffFailed("the native unwind stopped at another frame");
    CONTEXT project = *landed;
    project.Rsp = landedRsp;
    project.Rip = state->targetRip;
    DWORD64 imageBase = 0;
    const auto entry = RtlLookupFunctionEntry(project.Rip, &imageBase, nullptr);
    if (!entry) HandoffFailed("the last runtime frame has no unwind data");
    auto* const object = unwind + RuntimeUnwindBytes;
    std::type_info* type = nullptr;
    void (*destructor)(void*) = nullptr;
    std::memcpy(&type, unwind - UnwindToType, sizeof(type));
    std::memcpy(&destructor, unwind - UnwindToDestructor, sizeof(destructor));
    const HMODULE runtime = RuntimeModule();
    PVOID handlerData = nullptr;
    DWORD64 establisher = 0;
    if (!state->targetCleaned) {
        state->targetCleaned = true;
        CONTEXT frame = project;
        const auto handler = RtlVirtualUnwind(UNW_FLAG_UHANDLER, imageBase, frame.Rip, entry, &frame, &handlerData, &establisher, nullptr);
        DWORD64 landing = 0;
        SearchException probe {};
        PrepareProbe(probe, type, object, state->objectBytes);
        if (handler != nullptr && handler == reinterpret_cast<PEXCEPTION_ROUTINE>(GetProcAddress(runtime, "__gxx_personality_seh0"))
                && RuntimeFramePersonality(probe, _UA_CLEANUP_PHASE, project.Rip, imageBase, entry, handlerData, &landing) == _URC_INSTALL_CONTEXT) {
            CONTEXT cleanup = project;
            cleanup.Rip = landing;
            cleanup.Rax = reinterpret_cast<DWORD64>(unwind);
            cleanup.Rdx = 0;
            RtlRestoreContext(&cleanup, nullptr);
        }
    }
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, project.Rip, entry, &project, &handlerData, &establisher, nullptr);

    void* copy = __cxxabiv1::__cxa_allocate_exception(state->objectBytes);
    std::memcpy(copy, object, state->objectBytes);

    const auto freeException = reinterpret_cast<FreeException>(GetProcAddress(runtime, "__cxa_free_exception"));
    const auto getGlobals = reinterpret_cast<GetGlobals>(GetProcAddress(runtime, "__cxa_get_globals"));
    if (!freeException || !getGlobals) HandoffFailed("libstdc++-6.dll does not export its exception runtime");
    freeException(object);
    if (auto* globals = getGlobals(); globals->uncaught != 0) --globals->uncaught;
    state->active = false;

    project.Rsp -= sizeof(DWORD64);
    std::memcpy(reinterpret_cast<void*>(project.Rsp), &project.Rip, sizeof(project.Rip));
    project.Rip = reinterpret_cast<DWORD64>(&ThrowInProject);
    project.Rcx = reinterpret_cast<DWORD64>(copy);
    project.Rdx = reinterpret_cast<DWORD64>(type);
    project.R8 = reinterpret_cast<DWORD64>(destructor);
    RtlRestoreContext(&project, nullptr);
    HandoffFailed("RtlRestoreContext returned");
}

RuntimeThrowHandoff LibcRuntimeThrowHandoff_nid_no_patch(EXCEPTION_POINTERS* info) {
    const auto* record = info->ExceptionRecord;
    if (handoff && handoff->active) return RuntimeThrowHandoff::InProgress;
    constexpr DWORD unwinding = EXCEPTION_NONCONTINUABLE | EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND;
    if (record->ExceptionCode != GccThrow || (record->ExceptionFlags & unwinding) != 0 || record->NumberParameters != 1) return RuntimeThrowHandoff::None;
    auto* const unwind = reinterpret_cast<std::uint8_t*>(record->ExceptionInformation[0]);
    std::uint64_t exceptionClass = 0;
    if (!unwind || !Read(unwind, exceptionClass) || exceptionClass != GnuPrimaryClass) return RuntimeThrowHandoff::None;
    auto* const allocation = unwind - UnwindToAllocation;
    int references = 0;
    std::type_info* headerType = nullptr;
    std::type_info* type = nullptr;
    if (!Read(allocation, references) || references != 1) return RuntimeThrowHandoff::None;
    if (!Read(allocation + AllocationToType, headerType) || !Read(unwind - UnwindToType, type) || !type || type != headerType) return RuntimeThrowHandoff::None;
    const auto allocationBytes = HeapAllocationBytes(allocation);
    if (allocationBytes <= RuntimeHeaderBytes) return RuntimeThrowHandoff::None;
    const auto objectBytes = allocationBytes - RuntimeHeaderBytes;
    if (objectBytes > SearchObjectBytes) return RuntimeThrowHandoff::None;

    SearchException probe {};
    PrepareProbe(probe, type, unwind + RuntimeUnwindBytes, objectBytes);
    if (!handoff) {
        void* storage = HeapAlloc(GetProcessHeap(), 0, sizeof(Handoff));
        if (!storage) return RuntimeThrowHandoff::None;
        handoff = new (storage) Handoff {};
    }
    Handoff& state = *handoff;
    state = Handoff {};
    if (!FindTarget(*info->ContextRecord, state, probe)) return RuntimeThrowHandoff::None;
    state.unwind = unwind;
    state.objectBytes = objectBytes;
    state.record = *record;
    state.record.NumberParameters = 4;
    state.record.ExceptionInformation[1] = state.targetFrame;
    state.record.ExceptionInformation[2] = reinterpret_cast<ULONG_PTR>(RuntimeHandoffLanded_nid_no_patch);
    state.record.ExceptionInformation[3] = 0;
    const ULONG_PTR target[3] = {state.record.ExceptionInformation[1], state.record.ExceptionInformation[2], 0};
    std::memcpy(unwind + UnwindToPrivate + sizeof(ULONG_PTR), target, sizeof(target));
    state.active = true;

    auto& context = *info->ContextRecord;
    context.Rax = context.Rip;
    context.Rip = reinterpret_cast<DWORD64>(RuntimeHandoffUnwind_nid_no_patch);
    context.Rcx = state.targetFrame;
    context.Rdx = state.record.ExceptionInformation[2];
    context.R8 = reinterpret_cast<DWORD64>(&state.record);
    context.R9 = reinterpret_cast<DWORD64>(unwind);
    context.R10 = reinterpret_cast<DWORD64>(&state.scratch);
    context.R11 = 0;
    return RuntimeThrowHandoff::Redirected;
}
#endif
