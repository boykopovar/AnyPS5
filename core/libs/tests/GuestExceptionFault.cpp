#include "SceTypes.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <chrono>
#include <csignal>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
[[noreturn]] void APS5_VABI longjmp_nid_postfix(void* buffer, int value);
}

struct RegisterState {
    std::uint8_t vectorIn[32];
    std::uint8_t vectorOut[32];
    std::uint32_t avx;
    std::uint32_t padding;
    std::uint64_t r12;
    std::uint64_t r13;
};

static_assert(offsetof(RegisterState, vectorOut) == 32 && offsetof(RegisterState, avx) == 64 && offsetof(RegisterState, r12) == 72 && offsetof(RegisterState, r13) == 80);

extern "C" {
std::uint64_t APS5_VABI FaultTestRead(std::uintptr_t address);
void FaultTestReadSite();
void FaultTestReadResume();
std::uint64_t APS5_VABI FaultTestWrite(std::uintptr_t address, std::uint64_t value);
void FaultTestWriteSite();
void FaultTestWriteResume();
std::uint64_t APS5_VABI FaultTestIllegal();
void FaultTestIllegalSite();
void FaultTestIllegalResume();
std::uint64_t APS5_VABI FaultTestDivide(std::int32_t dividend, std::int32_t divisor);
void FaultTestDivideSite();
void FaultTestDivideResume();
void APS5_VABI FaultTestRegisters(std::uintptr_t address, RegisterState* state);
void FaultTestRegistersSite();
void FaultTestRegistersResume();
std::uint64_t APS5_VABI FaultTestJump(std::uintptr_t address, void* buffer);
void FaultTestJumpSite();
std::uint64_t APS5_VABI FaultTestSimdDivide();
void FaultTestSimdDivideSite();
void FaultTestSimdDivideResume();
}

asm(".text\n"
    ".globl FaultTestRead\n"
    "FaultTestRead:\n"
    "    xorl %eax, %eax\n"
    ".globl FaultTestReadSite\n"
    "FaultTestReadSite:\n"
    "    movq (%rdi), %rax\n"
    ".globl FaultTestReadResume\n"
    "FaultTestReadResume:\n"
    "    ret\n"
    ".globl FaultTestWrite\n"
    "FaultTestWrite:\n"
    "    xorl %eax, %eax\n"
    ".globl FaultTestWriteSite\n"
    "FaultTestWriteSite:\n"
    "    movq %rsi, (%rdi)\n"
    ".globl FaultTestWriteResume\n"
    "FaultTestWriteResume:\n"
    "    ret\n"
    ".globl FaultTestIllegal\n"
    "FaultTestIllegal:\n"
    "    xorl %eax, %eax\n"
    ".globl FaultTestIllegalSite\n"
    "FaultTestIllegalSite:\n"
    "    ud2\n"
    ".globl FaultTestIllegalResume\n"
    "FaultTestIllegalResume:\n"
    "    ret\n"
    ".globl FaultTestDivide\n"
    "FaultTestDivide:\n"
    "    movl %edi, %eax\n"
    "    cltd\n"
    ".globl FaultTestDivideSite\n"
    "FaultTestDivideSite:\n"
    "    idivl %esi\n"
    ".globl FaultTestDivideResume\n"
    "FaultTestDivideResume:\n"
    "    ret\n"
    ".globl FaultTestRegisters\n"
    "FaultTestRegisters:\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %rbx\n"
    "    movq %rsi, %rbx\n"
    "    movabsq $0x1111222233334444, %r12\n"
    "    movabsq $0x5555666677778888, %r13\n"
    "    cmpl $0, 64(%rbx)\n"
    "    je 1f\n"
    "    vmovdqu 0(%rbx), %ymm8\n"
    "    jmp 2f\n"
    "1:\n"
    "    movdqu 0(%rbx), %xmm8\n"
    "2:\n"
    ".globl FaultTestRegistersSite\n"
    "FaultTestRegistersSite:\n"
    "    movq (%rdi), %rax\n"
    ".globl FaultTestRegistersResume\n"
    "FaultTestRegistersResume:\n"
    "    movq %r12, 72(%rbx)\n"
    "    movq %r13, 80(%rbx)\n"
    "    cmpl $0, 64(%rbx)\n"
    "    je 3f\n"
    "    vmovdqu %ymm8, 32(%rbx)\n"
    "    vzeroupper\n"
    "    jmp 4f\n"
    "3:\n"
    "    movdqu %xmm8, 32(%rbx)\n"
    "4:\n"
    "    popq %rbx\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    ret\n"
    ".globl FaultTestJump\n"
    "FaultTestJump:\n"
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    subq $8, %rsp\n"
    "    leaq 5f(%rip), %rax\n"
    "    movq %rax, 0(%rsi)\n"
    "    movq %rbx, 8(%rsi)\n"
    "    movq %rsp, 16(%rsi)\n"
    "    movq %rbp, 24(%rsi)\n"
    "    movq %r12, 32(%rsi)\n"
    "    movq %r13, 40(%rsi)\n"
    "    movq %r14, 48(%rsi)\n"
    "    movq %r15, 56(%rsi)\n"
    "    stmxcsr 64(%rsi)\n"
    "    fnstcw 68(%rsi)\n"
    ".globl FaultTestJumpSite\n"
    "FaultTestJumpSite:\n"
    "    movq (%rdi), %rax\n"
    "    xorl %eax, %eax\n"
    "5:\n"
    "    addq $8, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    ret\n"
    ".globl FaultTestSimdDivide\n"
    "FaultTestSimdDivide:\n"
    "    subq $8, %rsp\n"
    "    stmxcsr (%rsp)\n"
    "    movl $0x1d80, 4(%rsp)\n"
    "    ldmxcsr 4(%rsp)\n"
    "    movl $1, %eax\n"
    "    cvtsi2ss %eax, %xmm0\n"
    "    xorps %xmm1, %xmm1\n"
    "    xorl %eax, %eax\n"
    ".globl FaultTestSimdDivideSite\n"
    "FaultTestSimdDivideSite:\n"
    "    divss %xmm1, %xmm0\n"
    ".globl FaultTestSimdDivideResume\n"
    "FaultTestSimdDivideResume:\n"
    "    ldmxcsr (%rsp)\n"
    "    addq $8, %rsp\n"
    "    ret\n");

static constexpr int GUEST_SIGILL = 4;
static constexpr int GUEST_SIGFPE = 8;
static constexpr int GUEST_SIGBUS = 10;
static constexpr int GUEST_SIGSEGV = 11;
static constexpr std::uint32_t T_PRIVINFLT = 1;
static constexpr std::uint32_t T_PROTFLT = 9;
static constexpr std::uint32_t T_PAGEFLT = 12;
static constexpr std::uint32_t T_DIVIDE = 18;
static constexpr std::uint32_t T_XMMFLT = 29;
static constexpr std::uint64_t PGEX_P = 0x1;
static constexpr std::uint64_t PGEX_W = 0x2;
static constexpr std::uint64_t PGEX_U = 0x4;
static constexpr std::size_t RbxOffset = 0x80;
static constexpr std::size_t RaxOffset = 0x78;
static constexpr std::size_t R12Offset = 0xa0;
static constexpr std::size_t R13Offset = 0xa8;
static constexpr std::size_t TrapnoOffset = 0xc0;
static constexpr std::size_t AddrOffset = 0xc8;
static constexpr std::size_t ErrOffset = 0xd8;
static constexpr std::size_t RipOffset = 0xe0;
static constexpr std::size_t Xmm8Offset = 0x140 + 160 + 8 * 16;
static constexpr std::uintptr_t NullAddress = 0x10;
static constexpr std::uintptr_t NonCanonicalAddress = 0x8000000000000000ULL;
static constexpr std::uint64_t Resumed = 0x600dcafe0000beefULL;
static constexpr std::uint64_t PatchedR12 = 0x5a5a1234abcd0001ULL;
static constexpr int JumpValue = 7;
static constexpr std::size_t PageBytes = 4096;
static constexpr std::size_t WatchedBytes = 65536;
static constexpr int RetryCalls = 3;
static constexpr int ChildTimeoutSeconds = 10;
static constexpr const char* RemoveAndReturnMode = "remove-and-return";
static constexpr const char* HandledMarker = "[guest_exception_fault] handler removed itself\n";

enum class Mode { Resume, Registers, Nested, Jump, Retry, Unprotect, Remove };

static void Require(bool value) { if (!value) std::abort(); }

template <typename TValue>
static TValue Field(void* context, std::size_t offset) {
    TValue value{};
    std::memcpy(&value, static_cast<unsigned char*>(context) + offset, sizeof(value));
    return value;
}

template <typename TValue>
static void SetField(void* context, std::size_t offset, TValue value) {
    std::memcpy(static_cast<unsigned char*>(context) + offset, &value, sizeof(value));
}

static std::uintptr_t Address(void (*label)()) {
    return reinterpret_cast<std::uintptr_t>(label);
}

struct Expected {
    int signum;
    std::uint32_t trapno;
    std::uint64_t addr;
    std::uintptr_t site;
    std::uintptr_t resume;
};

static Mode mode = Mode::Resume;
static Expected expected{};
static std::atomic<int> calls{0};
static std::atomic<int> nestedCalls{0};
static std::uint64_t observedErr = 0;
static std::atomic<std::thread::id> handlerThread;
static RegisterState* registerState = nullptr;
static void* unprotectPage = nullptr;
alignas(16) static std::uint8_t jumpBuffer[128];

static void MakeWritable(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, PageBytes, PAGE_READWRITE, &previous) != 0);
#else
    Require(mprotect(page, PageBytes, PROT_READ | PROT_WRITE) == 0);
#endif
}

static void MarkHandled() {
#ifdef _WIN32
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), HandledMarker, static_cast<DWORD>(std::strlen(HandledMarker)), &written, nullptr);
#else
    Require(write(STDOUT_FILENO, HandledMarker, std::strlen(HandledMarker)) == static_cast<ssize_t>(std::strlen(HandledMarker)));
#endif
}

static void Check(int signum, void* context, const Expected& want) {
    Require(signum == want.signum);
    Require(Field<std::uint32_t>(context, TrapnoOffset) == want.trapno);
    Require(Field<std::uint64_t>(context, AddrOffset) == want.addr);
    Require(Field<std::uint64_t>(context, RipOffset) == want.site);
}

static void Resume(void* context, std::uintptr_t resume) {
    SetField<std::uint64_t>(context, RipOffset, resume);
    SetField<std::uint64_t>(context, RaxOffset, Resumed);
}

static void APS5_VABI IllegalHandler(int signum, void* context) {
    if (mode == Mode::Nested && calls.load() == 1) {
        Check(signum, context, {GUEST_SIGILL, T_PRIVINFLT, 0, Address(FaultTestIllegalSite), 0});
        nestedCalls.fetch_add(1);
        Resume(context, Address(FaultTestIllegalResume));
        return;
    }
    Check(signum, context, expected);
    calls.fetch_add(1);
    Resume(context, expected.resume);
}

static void APS5_VABI Handler(int signum, void* context) {
    Check(signum, context, expected);
    observedErr = Field<std::uint64_t>(context, ErrOffset);
    handlerThread.store(std::this_thread::get_id());
    const int call = calls.fetch_add(1) + 1;
    if (mode == Mode::Retry && call < RetryCalls) return;
    if (mode == Mode::Unprotect) {
        MakeWritable(unprotectPage);
        return;
    }
    if (mode == Mode::Remove) {
        MarkHandled();
        Require(sceKernelRemoveExceptionHandler(signum) == 0);
        return;
    }
    if (mode == Mode::Jump) longjmp_nid_postfix(jumpBuffer, JumpValue);
    if (mode == Mode::Nested) Require(FaultTestIllegal() == Resumed);
    if (mode == Mode::Registers) {
        Require(Field<std::uint64_t>(context, R13Offset) == 0x5555666677778888ULL);
        Require(Field<std::uint64_t>(context, RbxOffset) == reinterpret_cast<std::uint64_t>(registerState));
        Require(std::memcmp(static_cast<unsigned char*>(context) + Xmm8Offset, registerState->vectorIn, 16) == 0);
        SetField<std::uint64_t>(context, R12Offset, PatchedR12);
        SetField<std::uint64_t>(context, RipOffset, expected.resume);
        if (registerState->avx != 0) asm volatile("vzeroall" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
        return;
    }
    Resume(context, expected.resume);
}

static void Arm(Mode next, const Expected& want) {
    mode = next;
    expected = want;
    calls.store(0);
    nestedCalls.store(0);
    observedErr = 0;
}

static void* Pages(std::size_t bytes, bool writable) {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, writable ? PAGE_READWRITE : PAGE_READONLY);
    Require(pages != nullptr);
#else
    void* pages = mmap(nullptr, bytes, writable ? PROT_READ | PROT_WRITE : PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(pages != MAP_FAILED);
#endif
    return pages;
}

static void FreePages(void* pages, std::size_t bytes) {
#ifdef _WIN32
    (void)bytes;
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, bytes) == 0);
#endif
}

static void TestRead() {
    Arm(Mode::Resume, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestReadSite), Address(FaultTestReadResume)});
    Require(FaultTestRead(NullAddress) == Resumed);
    Require(calls.load() == 1);
    Require((observedErr & PGEX_U) != 0);
    Require((observedErr & (PGEX_W | PGEX_P)) == 0);
    Require(handlerThread.load() == std::this_thread::get_id());
}

static void TestWriteToReadOnly() {
    void* page = Pages(PageBytes, false);
    const auto address = reinterpret_cast<std::uintptr_t>(page) + 8;
    Require(*reinterpret_cast<volatile std::uint64_t*>(address) == 0);
    Arm(Mode::Resume, {GUEST_SIGSEGV, T_PAGEFLT, address, Address(FaultTestWriteSite), Address(FaultTestWriteResume)});
    Require(FaultTestWrite(address, 1) == Resumed);
    Require(calls.load() == 1);
    Require((observedErr & (PGEX_U | PGEX_W | PGEX_P)) == (PGEX_U | PGEX_W | PGEX_P));
    Require(*reinterpret_cast<volatile std::uint64_t*>(address) == 0);
    FreePages(page, PageBytes);
}

static void TestIllegal() {
    Arm(Mode::Resume, {GUEST_SIGILL, T_PRIVINFLT, 0, Address(FaultTestIllegalSite), Address(FaultTestIllegalResume)});
    Require(FaultTestIllegal() == Resumed);
    Require(calls.load() == 1);
}

static void TestDivide() {
    Arm(Mode::Resume, {GUEST_SIGFPE, T_DIVIDE, 0, Address(FaultTestDivideSite), Address(FaultTestDivideResume)});
    Require(FaultTestDivide(1, 0) == Resumed);
    Require(calls.load() == 1);
    Arm(Mode::Resume, {GUEST_SIGFPE, T_DIVIDE, 0, Address(FaultTestDivideSite), Address(FaultTestDivideResume)});
    Require(FaultTestDivide(INT_MIN, -1) == Resumed);
    Require(calls.load() == 1);
}

static void TestSimdDivide() {
    Arm(Mode::Resume, {GUEST_SIGFPE, T_XMMFLT, 0, Address(FaultTestSimdDivideSite), Address(FaultTestSimdDivideResume)});
    Require(FaultTestSimdDivide() == Resumed);
    Require(calls.load() == 1);
}

static void TestNonCanonical() {
    Arm(Mode::Resume, {GUEST_SIGBUS, T_PROTFLT, 0, Address(FaultTestReadSite), Address(FaultTestReadResume)});
    Require(FaultTestRead(NonCanonicalAddress) == Resumed);
    Require(calls.load() == 1);
}

static void TestRegisters() {
    RegisterState state{};
    for (std::size_t i = 0; i < sizeof(state.vectorIn); ++i) state.vectorIn[i] = static_cast<std::uint8_t>(0xa0 + i);
    state.avx = __builtin_cpu_supports("avx") ? 1 : 0;
    registerState = &state;
    Arm(Mode::Registers, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestRegistersSite), Address(FaultTestRegistersResume)});
    FaultTestRegisters(NullAddress, &state);
    Require(calls.load() == 1);
    Require(state.r12 == PatchedR12);
    Require(state.r13 == 0x5555666677778888ULL);
    Require(std::memcmp(state.vectorOut, state.vectorIn, state.avx != 0 ? 32 : 16) == 0);
    registerState = nullptr;
}

static void TestNested() {
    Arm(Mode::Nested, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestReadSite), Address(FaultTestReadResume)});
    Require(FaultTestRead(NullAddress) == Resumed);
    Require(calls.load() == 1);
    Require(nestedCalls.load() == 1);
}

static void TestJumpOut() {
    for (int round = 0; round < 3; ++round) {
        Arm(Mode::Jump, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestJumpSite), 0});
        Require(FaultTestJump(NullAddress, jumpBuffer) == JumpValue);
        Require(calls.load() == 1);
    }
    TestRead();
}

static void TestWatchedStore() {
    using namespace GuestWriteWatch;
    if (!GuestWriteWatchAvailable_nid_postfix()) {
        std::puts("write watch unavailable: watched store not tested");
        return;
    }
    void* watched = Pages(WatchedBytes, true);
    const auto begin = reinterpret_cast<std::uintptr_t>(watched);
    GuestWriteWatchRegister_nid_postfix(watched, WatchedBytes);
    const auto collect = [&] {
        std::uintptr_t written = 0;
        Require(GuestWriteWatchCollect_nid_postfix(begin, WatchedBytes, [](void* context, std::uintptr_t first, std::uintptr_t end) {
            auto& address = *static_cast<std::uintptr_t*>(context);
            if (address == 0) address = first;
            (void)end;
        }, &written));
        return written;
    };
    collect();
    Require(collect() == 0);
    const auto address = begin + WatchedBytes / 2 + 8;
    Arm(Mode::Resume, {GUEST_SIGSEGV, T_PAGEFLT, address, Address(FaultTestWriteSite), Address(FaultTestWriteResume)});
    Require(FaultTestWrite(address, 5) == 0);
    Require(calls.load() == 0);
    Require(*reinterpret_cast<volatile std::uint64_t*>(address) == 5);
    const auto written = collect();
    Require(written != 0 && written <= address && address < begin + WatchedBytes);
    GuestWriteWatchUnregister_nid_postfix(watched, WatchedBytes);
    FreePages(watched, WatchedBytes);
}

static void TestReturnUnchanged() {
    Arm(Mode::Retry, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestReadSite), Address(FaultTestReadResume)});
    Require(FaultTestRead(NullAddress) == Resumed);
    Require(calls.load() == RetryCalls);
}

static void TestFixAndReturn() {
    void* page = Pages(PageBytes, false);
    const auto address = reinterpret_cast<std::uintptr_t>(page) + 8;
    unprotectPage = page;
    Arm(Mode::Unprotect, {GUEST_SIGSEGV, T_PAGEFLT, address, Address(FaultTestWriteSite), 0});
    Require(FaultTestWrite(address, 9) == 0);
    Require(calls.load() == 1);
    Require(*reinterpret_cast<volatile std::uint64_t*>(address) == 9);
    unprotectPage = nullptr;
    FreePages(page, PageBytes);
}

static int RemoveAndReturnChild() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
#endif
    Require(sceKernelInstallExceptionHandler(GUEST_SIGSEGV, reinterpret_cast<void*>(&Handler)) == 0);
    Arm(Mode::Remove, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestReadSite), 0});
    FaultTestRead(NullAddress);
    return EXIT_SUCCESS;
}

static void TestRemoveAndReturn(const char* self) {
    std::string output;
#ifdef _WIN32
    (void)self;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    Require(CreatePipe(&readEnd, &writeEnd, &attributes, 0) != 0);
    Require(SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0) != 0);
    wchar_t path[MAX_PATH]{};
    Require(GetModuleFileNameW(nullptr, path, MAX_PATH) != 0);
    std::wstring command = L"\"" + std::wstring(path) + L"\" remove-and-return";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writeEnd;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION child{};
    Require(CreateProcessW(path, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &child) != 0);
    CloseHandle(writeEnd);
    const DWORD waited = WaitForSingleObject(child.hProcess, ChildTimeoutSeconds * 1000);
    if (waited != WAIT_OBJECT_0) TerminateProcess(child.hProcess, 1);
    Require(waited == WAIT_OBJECT_0);
    DWORD code = 0;
    Require(GetExitCodeProcess(child.hProcess, &code) != 0);
    Require(code == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
    char buffer[64];
    DWORD bytes = 0;
    while (ReadFile(readEnd, buffer, sizeof(buffer), &bytes, nullptr) != 0 && bytes != 0) output.append(buffer, bytes);
    CloseHandle(readEnd);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
#else
    int pipeEnds[2];
    Require(pipe(pipeEnds) == 0);
    posix_spawn_file_actions_t actions;
    Require(posix_spawn_file_actions_init(&actions) == 0);
    Require(posix_spawn_file_actions_adddup2(&actions, pipeEnds[1], STDOUT_FILENO) == 0);
    Require(posix_spawn_file_actions_addclose(&actions, pipeEnds[0]) == 0);
    std::string program = self;
    std::string argument = RemoveAndReturnMode;
    char* arguments[] = {program.data(), argument.data(), nullptr};
    pid_t child = 0;
    Require(posix_spawn(&child, "/proc/self/exe", &actions, nullptr, arguments, environ) == 0);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeEnds[1]);
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(ChildTimeoutSeconds);
    pid_t waited = 0;
    while ((waited = waitpid(child, &status, WNOHANG)) == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (waited == 0) {
        kill(child, SIGKILL);
        waitpid(child, &status, 0);
    }
    Require(waited == child);
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
    char buffer[64];
    ssize_t bytes = 0;
    while ((bytes = read(pipeEnds[0], buffer, sizeof(buffer))) > 0) output.append(buffer, static_cast<std::size_t>(bytes));
    close(pipeEnds[0]);
#endif
    const auto first = output.find(HandledMarker);
    Require(first != std::string::npos && output.find(HandledMarker, first + 1) == std::string::npos);
}

static void TestOtherThread() {
    Arm(Mode::Resume, {GUEST_SIGSEGV, T_PAGEFLT, NullAddress, Address(FaultTestReadSite), Address(FaultTestReadResume)});
    std::thread::id worker;
    std::uint64_t result = 0;
    std::thread thread([&] {
        worker = std::this_thread::get_id();
        result = FaultTestRead(NullAddress);
    });
    thread.join();
    Require(result == Resumed);
    Require(calls.load() == 1);
    Require(handlerThread.load() == worker);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], RemoveAndReturnMode) == 0) return RemoveAndReturnChild();
    __builtin_cpu_init();
    Require(sceKernelInstallExceptionHandler(GUEST_SIGSEGV, reinterpret_cast<void*>(&Handler)) == 0);
    Require(sceKernelInstallExceptionHandler(GUEST_SIGBUS, reinterpret_cast<void*>(&Handler)) == 0);
    Require(sceKernelInstallExceptionHandler(GUEST_SIGFPE, reinterpret_cast<void*>(&Handler)) == 0);
    Require(sceKernelInstallExceptionHandler(GUEST_SIGILL, reinterpret_cast<void*>(&IllegalHandler)) == 0);
    TestRead();
    TestWriteToReadOnly();
    TestIllegal();
    TestDivide();
    TestSimdDivide();
    TestNonCanonical();
    TestRegisters();
    TestNested();
    TestJumpOut();
    TestOtherThread();
    TestWatchedStore();
    TestReturnUnchanged();
    TestFixAndReturn();
    TestRemoveAndReturn(argv[0]);
    for (const int signum : {GUEST_SIGSEGV, GUEST_SIGBUS, GUEST_SIGFPE, GUEST_SIGILL}) Require(sceKernelRemoveExceptionHandler(signum) == 0);
}
