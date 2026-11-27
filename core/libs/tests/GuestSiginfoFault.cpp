#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

extern "C" {
std::uint64_t APS5_VABI SiginfoTestRead(std::uintptr_t address);
void SiginfoTestReadSite();
void SiginfoTestReadResume();
std::uint64_t APS5_VABI SiginfoTestWrite(std::uintptr_t address, std::uint64_t value);
void SiginfoTestWriteSite();
}

asm(".text\n"
    ".globl " APS5_ASM_SYMBOL("SiginfoTestRead") "\n"
    APS5_ASM_SYMBOL("SiginfoTestRead") ":\n"
    "    xorl %eax, %eax\n"
    ".globl " APS5_ASM_SYMBOL("SiginfoTestReadSite") "\n"
    APS5_ASM_SYMBOL("SiginfoTestReadSite") ":\n"
    "    movq (%rdi), %rax\n"
    ".globl " APS5_ASM_SYMBOL("SiginfoTestReadResume") "\n"
    APS5_ASM_SYMBOL("SiginfoTestReadResume") ":\n"
    "    ret\n"
    ".globl " APS5_ASM_SYMBOL("SiginfoTestWrite") "\n"
    APS5_ASM_SYMBOL("SiginfoTestWrite") ":\n"
    ".globl " APS5_ASM_SYMBOL("SiginfoTestWriteSite") "\n"
    APS5_ASM_SYMBOL("SiginfoTestWriteSite") ":\n"
    "    movq %rsi, (%rdi)\n"
    "    movq (%rdi), %rax\n"
    "    ret\n");

static void RequireFailed(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "[guest_siginfo_fault] Require failed: %s at %s:%d\n", expression, file, line);
    std::fflush(stderr);
    std::abort();
}

#define Require(...) do { if (!(__VA_ARGS__)) RequireFailed(#__VA_ARGS__, __FILE__, __LINE__); } while (false)

static constexpr int GuestSigsegv = 11;
static constexpr int SaNodefer = 0x10;
static constexpr int SaSiginfo = 0x40;
static constexpr std::int32_t SegvMaperr = 1;
static constexpr std::int32_t SegvAccerr = 2;
static constexpr std::uint32_t TrapPage = 12;
static constexpr std::uint64_t PageProtection = 0x1;
static constexpr std::uint64_t PageWrite = 0x2;
static constexpr std::size_t InfoCode = 8;
static constexpr std::size_t InfoAddress = 24;
static constexpr std::size_t InfoTrap = 40;
static constexpr std::size_t RaxOffset = 0x78;
static constexpr std::size_t TrapnoOffset = 0xc0;
static constexpr std::size_t AddrOffset = 0xc8;
static constexpr std::size_t ErrOffset = 0xd8;
static constexpr std::size_t RipOffset = 0xe0;
static constexpr std::uintptr_t NullAddress = 0x10;
static constexpr std::uint64_t Resumed = 0x600dcafe0000beefULL;
static constexpr std::uint64_t Stored = 0x0123456789abcdefULL;
static constexpr std::size_t PageBytes = 4096;
static constexpr std::uint64_t AnyErr = ~0ULL;
static constexpr int GuestSigalrm = 14;
static constexpr int GuestSigusr1 = 30;
static constexpr int GuestSigblock = 1;
static constexpr int GuestSigsetmask = 3;

static std::uint32_t SignalBit(int guest) { return 1u << (guest - 1); }

static std::uint32_t BlockedWord() {
    std::uint32_t blocked[4]{};
    Require(sigprocmask_nid_postfix(GuestSigblock, nullptr, blocked) == 0);
    return blocked[0];
}

template <typename TValue>
static TValue Field(const void* base, std::size_t offset) {
    TValue value{};
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(value));
    return value;
}

template <typename TValue>
static void SetField(void* base, std::size_t offset, TValue value) {
    std::memcpy(static_cast<unsigned char*>(base) + offset, &value, sizeof(value));
}

struct Expected {
    std::int32_t code;
    std::uintptr_t address;
    std::uintptr_t site;
    std::uintptr_t resume;
    std::uint64_t err;
    bool editMask;
};

static Expected expected{};
static void* readOnlyPage = nullptr;
static std::atomic<int> calls{0};

static void MakeWritable(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, PageBytes, PAGE_READWRITE, &previous) != 0);
#else
    Require(mprotect(page, PageBytes, PROT_READ | PROT_WRITE) == 0);
#endif
}

static void* ReadOnlyPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, PageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, PageBytes, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

static void* NoAccessPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, PageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, PageBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

static void APS5_VABI SiginfoHandler(int signum, void* info, void* context) {
    calls.fetch_add(1);
    Require(signum == GuestSigsegv);
    Require(Field<std::int32_t>(info, 0) == GuestSigsegv);
    Require(Field<std::int32_t>(info, InfoCode) == expected.code);
    Require(Field<std::uintptr_t>(info, InfoAddress) == expected.address);
    Require(Field<std::int32_t>(info, InfoTrap) == static_cast<std::int32_t>(TrapPage));
    Require(Field<std::uint32_t>(context, TrapnoOffset) == TrapPage);
    Require(Field<std::uint64_t>(context, AddrOffset) == expected.address);
    if (expected.err != AnyErr) Require((Field<std::uint64_t>(context, ErrOffset) & (PageProtection | PageWrite)) == expected.err);
    if (expected.editMask) {
        Require((Field<std::uint32_t>(context, 0) & SignalBit(GuestSigalrm)) != 0);
        SetField<std::uint32_t>(context, 0, Field<std::uint32_t>(context, 0) | SignalBit(GuestSigusr1));
    }
    Require(Field<std::uint64_t>(context, RipOffset) == expected.site);
    if (expected.resume != 0) {
        SetField<std::uint64_t>(context, RipOffset, expected.resume);
        SetField<std::uint64_t>(context, RaxOffset, Resumed);
        return;
    }
    MakeWritable(readOnlyPage);
}

static void Install() {
    GuestSigaction action{};
    action.handler = reinterpret_cast<std::uintptr_t>(&SiginfoHandler);
    action.flags = SaSiginfo | SaNodefer;
    Require(sigaction_nid_postfix(GuestSigsegv, &action, nullptr) == 0);
    GuestSigaction installed{};
    Require(sigaction_nid_postfix(GuestSigsegv, nullptr, &installed) == 0);
    Require(installed.handler == action.handler && installed.flags == action.flags);
}

static void TestUnmappedRead() {
    calls.store(0);
    expected = {SegvMaperr, NullAddress, reinterpret_cast<std::uintptr_t>(&SiginfoTestReadSite), reinterpret_cast<std::uintptr_t>(&SiginfoTestReadResume), 0, false};
    Require(SiginfoTestRead(NullAddress) == Resumed);
    Require(calls.load() == 1);
}

static void TestWriteToReadOnly() {
    calls.store(0);
    readOnlyPage = ReadOnlyPage();
    const auto address = reinterpret_cast<std::uintptr_t>(readOnlyPage);
    Require(*static_cast<volatile const std::uint64_t*>(readOnlyPage) == 0);
    expected = {SegvAccerr, address, reinterpret_cast<std::uintptr_t>(&SiginfoTestWriteSite), 0, PageProtection | PageWrite, false};
    Require(SiginfoTestWrite(address, Stored) == Stored);
    Require(calls.load() == 1);
}

static void TestWriteToUntouchedReadOnly() {
    calls.store(0);
    readOnlyPage = ReadOnlyPage();
    const auto address = reinterpret_cast<std::uintptr_t>(readOnlyPage);
    expected = {SegvAccerr, address, reinterpret_cast<std::uintptr_t>(&SiginfoTestWriteSite), 0, AnyErr, false};
    Require(SiginfoTestWrite(address, Stored) == Stored);
    Require(calls.load() == 1);
}

static void TestReadNoAccess() {
    calls.store(0);
    const auto address = reinterpret_cast<std::uintptr_t>(NoAccessPage());
    expected = {SegvAccerr, address, reinterpret_cast<std::uintptr_t>(&SiginfoTestReadSite), reinterpret_cast<std::uintptr_t>(&SiginfoTestReadResume), AnyErr, false};
    Require(SiginfoTestRead(address) == Resumed);
    Require(calls.load() == 1);
}

static void TestContextMask() {
    std::uint32_t original[4]{};
    Require(sigprocmask_nid_postfix(GuestSigblock, nullptr, original) == 0);
    std::uint32_t alarm[4]{SignalBit(GuestSigalrm), 0, 0, 0};
    Require(sigprocmask_nid_postfix(GuestSigblock, alarm, nullptr) == 0);
    calls.store(0);
    expected = {SegvMaperr, NullAddress, reinterpret_cast<std::uintptr_t>(&SiginfoTestReadSite), reinterpret_cast<std::uintptr_t>(&SiginfoTestReadResume), 0, true};
    Require(SiginfoTestRead(NullAddress) == Resumed);
    Require(calls.load() == 1);
    Require((BlockedWord() & (SignalBit(GuestSigalrm) | SignalBit(GuestSigusr1))) == (SignalBit(GuestSigalrm) | SignalBit(GuestSigusr1)));
    Require(sigprocmask_nid_postfix(GuestSigsetmask, original, nullptr) == 0);
}

int main() {
    Install();
    TestUnmappedRead();
    TestWriteToReadOnly();
    TestWriteToUntouchedReadOnly();
    TestReadNoAccess();
    TestContextMask();
    GuestSigaction reset{};
    Require(sigaction_nid_postfix(GuestSigsegv, &reset, nullptr) == 0);
    std::puts("guest siginfo fault tests passed");
    return 0;
}
