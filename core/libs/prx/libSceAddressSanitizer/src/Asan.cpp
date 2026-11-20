/* Derived from LLVM compiler-rt lib/asan (asan_poisoning.cpp, asan_rtl.cpp, asan_report.cpp, asan_errors.cpp, asan_descriptions.cpp, asan_globals.cpp, asan_fake_stack.cpp, asan_interceptors_memintrinsics.cpp, asan_debugging.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/Asan.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <unistd.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define APS5_GUEST_ASAN_HOST_SANITIZED 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(ANYPS5_ENABLE_ASAN)
#define APS5_GUEST_ASAN_HOST_SANITIZED 1
#endif

namespace {

using uptr = std::uintptr_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

constexpr uptr ShadowScale = 3;
constexpr uptr Granularity = uptr{1} << ShadowScale;
constexpr uptr ShadowOffset = uptr{1} << 40;
constexpr uptr MemoryEnd = uptr{1} << 47;
constexpr uptr ShadowBegin = ShadowOffset;
constexpr uptr ShadowEnd = ShadowOffset + (MemoryEnd >> ShadowScale);
constexpr uptr ChunkBytes = uptr{1} << 30;
constexpr std::size_t ChunkCount = (ShadowEnd - ShadowBegin) / ChunkBytes;
constexpr uptr AllocaRedzoneSize = 32;
constexpr uptr AllocaRedzoneMask = AllocaRedzoneSize - 1;
constexpr uptr MaxExpectedCleanupSize = uptr{64} << 20;
constexpr uptr StackPageSize = 0x4000;
constexpr std::size_t BuggyPcPoolSize = 25;
constexpr std::size_t GlobalsPerDescription = 4;
constexpr std::size_t GuestStringMaximum = 4096;

constexpr u8 HeapLeftRedzoneMagic = 0xfa;
constexpr u8 HeapFreeMagic = 0xfd;
constexpr u8 StackLeftRedzoneMagic = 0xf1;
constexpr u8 StackMidRedzoneMagic = 0xf2;
constexpr u8 StackRightRedzoneMagic = 0xf3;
constexpr u8 StackAfterReturnMagic = 0xf5;
constexpr u8 InitializationOrderMagic = 0xf6;
constexpr u8 UserPoisonedMemoryMagic = 0xf7;
constexpr u8 ContiguousContainerOOBMagic = 0xfc;
constexpr u8 StackUseAfterScopeMagic = 0xf8;
constexpr u8 GlobalRedzoneMagic = 0xf9;
constexpr u8 InternalHeapMagic = 0xfe;
constexpr u8 ArrayCookieMagic = 0xac;
constexpr u8 IntraObjectRedzone = 0xbb;
constexpr u8 AllocaLeftMagic = 0xca;
constexpr u8 AllocaRightMagic = 0xcb;

struct Flags {
    bool haltOnError = true;
    bool allowUserPoisoning = true;
    bool poisonPartial = true;
    bool printLegend = true;
    bool poisonArrayCookie = true;
    bool checkInitializationOrder = false;
    bool strictInitOrder = false;
    bool suppressEqualPcs = true;
    bool printSummary = true;
    int reportGlobals = 1;
    int exitCode = 1;
};

bool ParseBool(std::string_view value, std::string_view name) {
    if (value == "1" || value == "true" || value == "yes") return true;
    if (value == "0" || value == "false" || value == "no") return false;
    throw std::invalid_argument("guest AddressSanitizer: option " + std::string(name) + " expects a boolean, got " + std::string(value));
}

int ParseInt(std::string_view value, std::string_view name) {
    const std::string text(value);
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || *end != '\0') throw std::invalid_argument("guest AddressSanitizer: option " + std::string(name) + " expects an integer, got " + text);
    return static_cast<int>(parsed);
}

Flags ReadFlags() {
    Flags flags;
    const char* environment = std::getenv("APS5_GUEST_ASAN_OPTIONS");
    if (environment == nullptr) return flags;
    std::string_view options(environment);
    while (!options.empty()) {
        const std::size_t separator = options.find_first_of(": ,\t\n");
        const std::string_view entry = options.substr(0, separator);
        options = separator == std::string_view::npos ? std::string_view{} : options.substr(separator + 1);
        if (entry.empty()) continue;
        const std::size_t equals = entry.find('=');
        if (equals == std::string_view::npos) throw std::invalid_argument("guest AddressSanitizer: option without a value: " + std::string(entry));
        const std::string_view name = entry.substr(0, equals);
        const std::string_view value = entry.substr(equals + 1);
        if (name == "halt_on_error") flags.haltOnError = ParseBool(value, name);
        else if (name == "allow_user_poisoning") flags.allowUserPoisoning = ParseBool(value, name);
        else if (name == "poison_partial") flags.poisonPartial = ParseBool(value, name);
        else if (name == "print_legend") flags.printLegend = ParseBool(value, name);
        else if (name == "poison_array_cookie") flags.poisonArrayCookie = ParseBool(value, name);
        else if (name == "check_initialization_order") flags.checkInitializationOrder = ParseBool(value, name);
        else if (name == "strict_init_order") flags.strictInitOrder = ParseBool(value, name);
        else if (name == "suppress_equal_pcs") flags.suppressEqualPcs = ParseBool(value, name);
        else if (name == "print_summary") flags.printSummary = ParseBool(value, name);
        else if (name == "report_globals") flags.reportGlobals = ParseInt(value, name);
        else if (name == "exitcode") flags.exitCode = ParseInt(value, name);
        else throw std::invalid_argument("guest AddressSanitizer: unsupported option " + std::string(name));
    }
    return flags;
}

const Flags& GetFlags() {
    static const Flags flags = ReadFlags();
    return flags;
}

std::array<std::atomic<bool>, ChunkCount> ownedChunks{};
std::atomic<bool> shadowReady{false};
std::once_flag shadowOnce;

#if defined(_WIN32)
constexpr uptr CommitBytes = uptr{1} << 16;

bool ReserveShadow(uptr address, uptr bytes) {
    return VirtualAlloc(reinterpret_cast<void*>(address), bytes, MEM_RESERVE, PAGE_READWRITE) == reinterpret_cast<void*>(address);
}

bool ShadowOwned(uptr shadow);

LONG CALLBACK CommitShadowOnFault(EXCEPTION_POINTERS* information) {
    const EXCEPTION_RECORD* record = information->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const uptr address = static_cast<uptr>(record->ExceptionInformation[1]);
    if (!ShadowOwned(address)) return EXCEPTION_CONTINUE_SEARCH;
    void* const page = reinterpret_cast<void*>(address & ~(CommitBytes - 1));
    if (VirtualAlloc(page, CommitBytes, MEM_COMMIT, PAGE_READWRITE) == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_CONTINUE_EXECUTION;
}
#elif defined(__APPLE__)
bool ReserveShadow(uptr address, uptr bytes) {
    mach_vm_address_t wanted = address;
    return mach_vm_allocate(mach_task_self(), &wanted, bytes, VM_FLAGS_FIXED) == KERN_SUCCESS && wanted == address;
}
#else
bool ReserveShadow(uptr address, uptr bytes) {
    void* const wanted = reinterpret_cast<void*>(address);
    void* const placed = mmap(wanted, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (placed == wanted) return true;
    if (placed != MAP_FAILED) munmap(placed, bytes);
    return false;
}
#endif

bool ShadowOwned(uptr shadow) {
    return shadow >= ShadowBegin && shadow < ShadowEnd && ownedChunks[(shadow - ShadowBegin) / ChunkBytes].load(std::memory_order_acquire);
}

void MapShadow() {
#if defined(APS5_GUEST_ASAN_HOST_SANITIZED)
    throw std::runtime_error("guest AddressSanitizer: the host is built with AddressSanitizer, whose own shadow covers the PS shadow range at 1 << 40");
#else
    if (ReserveShadow(ShadowBegin, ShadowEnd - ShadowBegin)) {
        for (auto& chunk : ownedChunks) chunk.store(true, std::memory_order_relaxed);
    } else {
        for (std::size_t index = 0; index < ChunkCount; ++index)
            if (ReserveShadow(ShadowBegin + index * ChunkBytes, ChunkBytes)) ownedChunks[index].store(true, std::memory_order_relaxed);
    }
    if (std::none_of(ownedChunks.begin(), ownedChunks.end(), [](const std::atomic<bool>& chunk) { return chunk.load(std::memory_order_relaxed); }))
        throw std::runtime_error("guest AddressSanitizer: cannot reserve any of the shadow range at 1 << 40");
#if defined(_WIN32)
    if (AddVectoredExceptionHandler(1, CommitShadowOnFault) == nullptr) throw std::runtime_error("guest AddressSanitizer: cannot install the shadow commit handler");
#endif
    shadowReady.store(true, std::memory_order_release);
#endif
}

void EnsureShadow() {
    if (!shadowReady.load(std::memory_order_acquire)) std::call_once(shadowOnce, MapShadow);
}

u8* ShadowOf(uptr address) {
    return reinterpret_cast<u8*>((address >> ShadowScale) + ShadowOffset);
}

uptr MemoryOf(uptr shadow) {
    return (shadow - ShadowOffset) << ShadowScale;
}

bool AddrIsInMem(uptr address) {
    if (address >= MemoryEnd) return false;
    if (address >= ShadowBegin && address < ShadowEnd) return false;
    return ShadowOwned(reinterpret_cast<uptr>(ShadowOf(address)));
}

bool Aligned(uptr value) {
    return (value & (Granularity - 1)) == 0;
}

uptr RoundUp(uptr value, uptr boundary) {
    return (value + boundary - 1) & ~(boundary - 1);
}

uptr RoundDown(uptr value, uptr boundary) {
    return value & ~(boundary - 1);
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}

void RequireShadow(uptr address, uptr size) {
    Require(AddrIsInMem(address) && AddrIsInMem(address + size - 1), "guest AddressSanitizer: range has no shadow");
}

void FastPoisonShadow(uptr address, uptr size, u8 value) {
    if (size == 0) return;
    RequireShadow(address, size);
    u8* const first = ShadowOf(address);
    u8* const last = ShadowOf(address + size - Granularity);
    std::memset(first, value, static_cast<std::size_t>(last - first) + 1);
}

void PoisonShadow(uptr address, uptr size, u8 value) {
    Require(Aligned(address) && Aligned(address + size), "guest AddressSanitizer: poisoned range is not 8-byte aligned");
    FastPoisonShadow(address, size, value);
}

void FastPoisonShadowPartialRightRedzone(uptr alignedAddress, uptr size, uptr redzoneSize, u8 value) {
    if (redzoneSize == 0) return;
    RequireShadow(alignedAddress, redzoneSize);
    u8* shadow = ShadowOf(alignedAddress);
    for (uptr offset = 0; offset < redzoneSize; offset += Granularity, ++shadow) {
        if (offset + Granularity <= size) *shadow = 0;
        else if (offset >= size) *shadow = value;
        else *shadow = GetFlags().poisonPartial ? static_cast<u8>(size - offset) : 0;
    }
}

bool AddressIsPoisoned(uptr address) {
    if (!AddrIsInMem(address)) return false;
    const s8 value = static_cast<s8>(*ShadowOf(address));
    if (value == 0) return false;
    const int lastAccessed = static_cast<int>(address & (Granularity - 1));
    return lastAccessed >= value;
}

uptr RegionIsPoisoned(uptr begin, uptr size) {
    if (size == 0) return 0;
    const uptr end = begin + size;
    if (!AddrIsInMem(begin)) return begin;
    if (!AddrIsInMem(end)) return end;
    Require(begin < end, "guest AddressSanitizer: region wraps around");
    const uptr alignedBegin = RoundUp(begin, Granularity);
    const uptr alignedEnd = RoundDown(end, Granularity);
    const u8* const shadowBegin = ShadowOf(alignedBegin);
    const u8* const shadowEnd = ShadowOf(alignedEnd);
    const bool middleClear = shadowEnd <= shadowBegin || std::all_of(shadowBegin, shadowEnd, [](u8 value) { return value == 0; });
    if (!AddressIsPoisoned(begin) && !AddressIsPoisoned(end - 1) && middleClear) return 0;
    for (uptr address = begin; address < end; ++address)
        if (AddressIsPoisoned(address)) return address;
    throw std::logic_error("guest AddressSanitizer: shadow scan found a poisoned byte but no poisoned address");
}

struct ShadowSegmentEndpoint {
    u8* chunk;
    s8 offset;
    s8 value;

    explicit ShadowSegmentEndpoint(uptr address)
        : chunk(ShadowOf(address)), offset(static_cast<s8>(address & (Granularity - 1))), value(static_cast<s8>(*chunk)) {}
};

std::string Format(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        va_end(arguments);
        throw std::runtime_error("guest AddressSanitizer: report formatting failed");
    }
    std::string text(static_cast<std::size_t>(length), '\0');
    std::vsnprintf(text.data(), text.size() + 1, format, arguments);
    va_end(arguments);
    return text;
}

std::string Pointer(uptr value) {
    return Format("0x%012zx", static_cast<std::size_t>(value));
}

int ProcessId() {
#if defined(_WIN32)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

std::string ReportLine(const std::string& text) {
    return Format("==%d==", ProcessId()) + text;
}

using ErrorReportCallback = void(APS5_VABI*)(const char*);
using DeathCallback = void(APS5_VABI*)();

std::atomic<ErrorReportCallback> errorReportCallback{nullptr};
std::atomic<DeathCallback> deathCallback{nullptr};
std::array<std::atomic<uptr>, BuggyPcPoolSize> buggyPcs{};

[[noreturn]] void Die() {
    if (const DeathCallback callback = deathCallback.load(std::memory_order_acquire)) callback();
    LibcExitWithoutHandlers_nid_no_patch(GetFlags().exitCode);
}

bool SuppressErrorReport(uptr pc) {
    if (!GetFlags().suppressEqualPcs) return false;
    for (auto& slot : buggyPcs) {
        uptr seen = slot.load(std::memory_order_relaxed);
        if (seen == 0 && slot.compare_exchange_strong(seen, pc, std::memory_order_relaxed)) return false;
        if (seen == pc) return true;
    }
    Die();
}

struct CurrentError {
    bool present = false;
    bool generic = false;
    uptr pc = 0;
    uptr bp = 0;
    uptr sp = 0;
    uptr address = 0;
    int isWrite = 0;
    uptr accessSize = 0;
    const char* description = "";
};

CurrentError currentError;
std::mutex reportMutex;
thread_local bool reportingOnThisThread = false;

struct Global {
    uptr beg = 0;
    uptr size = 0;
    uptr sizeWithRedzone = 0;
    uptr name = 0;
    uptr moduleName = 0;
    uptr hasDynamicInit = 0;
    uptr location = 0;
    uptr odrIndicator = 0;
};

static_assert(sizeof(Global) == 64);

constexpr std::size_t GlobalSizeV6 = 56;

struct DynamicInitGlobal {
    Global global;
    bool initialized = false;
};

std::mutex globalsMutex;
std::vector<Global> registeredGlobals;
std::vector<DynamicInitGlobal> dynamicInitGlobals;
thread_local int moduleAbiVersion = 8;

std::string GuestString(uptr address) {
    if (address == 0) return "<null>";
    std::string text;
    if (CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(address), GuestStringMaximum, text) != 0) return "<unreadable>";
    return text;
}

std::string GlobalNameIfAscii(const Global& global) {
    if (global.size == 0) return {};
    std::vector<unsigned char> bytes(global.size);
    if (!GuestCopyIn_nid_no_patch(bytes.data(), reinterpret_cast<const void*>(global.beg), bytes.size())) return {};
    for (std::size_t index = 0; index + 1 < bytes.size(); ++index)
        if (bytes[index] == 0 || bytes[index] > 0x7f) return {};
    if (bytes.back() != 0) return {};
    return Format("  '%s' is ascii string '%s'\n", GuestString(global.name).c_str(), reinterpret_cast<const char*>(bytes.data()));
}

bool IsAddressNearGlobal(uptr address, const Global& global) {
    if (address <= global.beg - 64) return false;
    if (address >= global.beg + global.sizeWithRedzone) return false;
    return true;
}

std::vector<Global> GlobalsForAddress(uptr address) {
    std::vector<Global> found;
    if (GetFlags().reportGlobals == 0) return found;
    std::lock_guard lock(globalsMutex);
    for (auto entry = registeredGlobals.rbegin(); entry != registeredGlobals.rend() && found.size() < GlobalsPerDescription; ++entry)
        if (IsAddressNearGlobal(address, *entry)) found.push_back(*entry);
    return found;
}

std::string DescribeRelativeToGlobal(uptr address, uptr accessSize, const Global& global) {
    std::string text;
    if (address < global.beg) {
        text = Format("%s is located %zd bytes before", Pointer(address).c_str(), static_cast<std::ptrdiff_t>(global.beg - address));
    } else if (address + accessSize > global.beg + global.size) {
        if (address < global.beg + global.size) address = global.beg + global.size;
        text = Format("%s is located %zd bytes after", Pointer(address).c_str(), static_cast<std::ptrdiff_t>(address - (global.beg + global.size)));
    } else {
        text = Format("%s is located %zd bytes inside of", Pointer(address).c_str(), static_cast<std::ptrdiff_t>(address - global.beg));
    }
    text += Format(" global variable '%s' defined in '%s' (0x%zx) of size %zu\n", GuestString(global.name).c_str(), GuestString(global.moduleName).c_str(),
                   static_cast<std::size_t>(global.beg), static_cast<std::size_t>(global.size));
    return text + GlobalNameIfAscii(global);
}

std::string DescribeAccessedAddress(uptr address, uptr accessSize) {
    const auto globals = GlobalsForAddress(address);
    if (globals.empty())
        return Format("Address %s is a wild pointer inside of access range of size %s.\n", Pointer(address).c_str(), Pointer(accessSize).c_str());
    std::string text;
    for (const auto& global : globals) text += DescribeRelativeToGlobal(address, accessSize, global);
    return text;
}

std::string DescribeAddress(uptr address) {
    const auto globals = GlobalsForAddress(address);
    if (globals.empty()) return "AddressSanitizer can not describe address in more detail (wild memory access suspected).\n";
    std::string text;
    for (const auto& global : globals) text += DescribeRelativeToGlobal(address, 1, global);
    return text;
}

std::string StackLine(uptr pc) {
    return Format("    #0 %s\n\n", Pointer(pc).c_str());
}

std::string Summary(const char* bugType) {
    if (!GetFlags().printSummary) return {};
    return Format("SUMMARY: AddressSanitizer: %s (<unknown module>)\n", bugType);
}

std::string ShadowByte(const char* before, u8 value, const char* after = "\n") {
    return Format("%s%x%x%s", before, value >> 4, value & 15, after);
}

std::string Legend() {
    std::string text = Format("Shadow byte legend (one shadow byte represents %d application bytes):\n", static_cast<int>(Granularity));
    text += ShadowByte("  Addressable:           ", 0);
    text += "  Partially addressable: ";
    for (u8 value = 1; value < Granularity; ++value) text += ShadowByte("", value, " ");
    text += "\n";
    text += ShadowByte("  Heap left redzone:       ", HeapLeftRedzoneMagic);
    text += ShadowByte("  Freed heap region:       ", HeapFreeMagic);
    text += ShadowByte("  Stack left redzone:      ", StackLeftRedzoneMagic);
    text += ShadowByte("  Stack mid redzone:       ", StackMidRedzoneMagic);
    text += ShadowByte("  Stack right redzone:     ", StackRightRedzoneMagic);
    text += ShadowByte("  Stack after return:      ", StackAfterReturnMagic);
    text += ShadowByte("  Stack use after scope:   ", StackUseAfterScopeMagic);
    text += ShadowByte("  Global redzone:          ", GlobalRedzoneMagic);
    text += ShadowByte("  Global init order:       ", InitializationOrderMagic);
    text += ShadowByte("  Poisoned by user:        ", UserPoisonedMemoryMagic);
    text += ShadowByte("  Container overflow:      ", ContiguousContainerOOBMagic);
    text += ShadowByte("  Array cookie:            ", ArrayCookieMagic);
    text += ShadowByte("  Intra object redzone:    ", IntraObjectRedzone);
    text += ShadowByte("  ASan internal:           ", InternalHeapMagic);
    text += ShadowByte("  Left alloca redzone:     ", AllocaLeftMagic);
    text += ShadowByte("  Right alloca redzone:    ", AllocaRightMagic);
    return text;
}

std::string ShadowMemoryForAddress(uptr address) {
    if (!AddrIsInMem(address)) return {};
    constexpr uptr BytesPerRow = 16;
    const uptr shadow = reinterpret_cast<uptr>(ShadowOf(address));
    const uptr alignedShadow = shadow & ~(BytesPerRow - 1);
    std::string text = "Shadow bytes around the buggy address:\n";
    for (int row = -5; row <= 5; ++row) {
        const uptr rowShadow = alignedShadow + static_cast<uptr>(static_cast<std::intptr_t>(row) * static_cast<std::intptr_t>(BytesPerRow));
        if (!ShadowOwned(rowShadow)) continue;
        text += Format("%s%s:", row == 0 ? "=>" : "  ", Pointer(MemoryOf(rowShadow)).c_str());
        for (uptr index = 0; index < BytesPerRow; ++index) {
            const uptr byte = rowShadow + index;
            const char* before = byte == shadow ? "[" : (byte - 1 == shadow && index != 0) ? "" : " ";
            const char* after = byte == shadow ? "]" : "";
            text += ShadowByte(before, *reinterpret_cast<const u8*>(byte), after);
        }
        text += "\n";
    }
    if (GetFlags().printLegend) text += Legend();
    return text;
}

class ScopedErrorReport {
public:
    explicit ScopedErrorReport(bool fatal) : halt(fatal || GetFlags().haltOnError) {
        if (reportingOnThisThread) {
            const std::string nested = ReportLine("AddressSanitizer: nested bug in the same thread, aborting.\n");
            std::fwrite(nested.data(), 1, nested.size(), stderr);
            std::fflush(stderr);
            Die();
        }
        reportingOnThisThread = true;
        lock = std::unique_lock(reportMutex);
        text = "=================================================================\n";
    }

    ScopedErrorReport(const ScopedErrorReport&) = delete;
    ScopedErrorReport& operator=(const ScopedErrorReport&) = delete;

    void Append(const std::string& more) { text += more; }

    void Record(const CurrentError& error) { currentError = error; }

    void Finish() {
        if (halt) text += ReportLine("ABORTING\n");
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fflush(stderr);
        if (const ErrorReportCallback callback = errorReportCallback.load(std::memory_order_acquire)) callback(text.c_str());
        if (halt) Die();
        currentError = CurrentError{};
        lock.unlock();
        reportingOnThisThread = false;
    }

    ~ScopedErrorReport() {
        reportingOnThisThread = false;
    }

private:
    bool halt;
    std::unique_lock<std::mutex> lock;
    std::string text;
};

const char* GenericBugType(uptr address, uptr accessSize, u8* shadowValue) {
    *shadowValue = 0;
    if (accessSize == 0 || !AddrIsInMem(address)) return "unknown-crash";
    u8* shadow = ShadowOf(address);
    if (*shadow == 0 && accessSize > Granularity) ++shadow;
    if (*shadow > 0 && *shadow < 128) ++shadow;
    *shadowValue = *shadow;
    switch (*shadow) {
    case HeapLeftRedzoneMagic:
    case ArrayCookieMagic: return "heap-buffer-overflow";
    case HeapFreeMagic: return "heap-use-after-free";
    case StackLeftRedzoneMagic: return "stack-buffer-underflow";
    case InitializationOrderMagic: return "initialization-order-fiasco";
    case StackMidRedzoneMagic:
    case StackRightRedzoneMagic: return "stack-buffer-overflow";
    case StackAfterReturnMagic: return "stack-use-after-return";
    case UserPoisonedMemoryMagic: return "use-after-poison";
    case ContiguousContainerOOBMagic: return "container-overflow";
    case StackUseAfterScopeMagic: return "stack-use-after-scope";
    case GlobalRedzoneMagic: return "global-buffer-overflow";
    case IntraObjectRedzone: return "intra-object-overflow";
    case AllocaLeftMagic:
    case AllocaRightMagic: return "dynamic-stack-buffer-overflow";
    default: return "unknown-crash";
    }
}

void ReportGenericError(uptr pc, uptr bp, uptr sp, uptr address, bool isWrite, uptr accessSize, bool fatal) {
    if (!fatal && SuppressErrorReport(pc)) return;
    ScopedErrorReport report(fatal);
    u8 shadowValue = 0;
    const char* const bugType = GenericBugType(address, accessSize, &shadowValue);
    report.Record(CurrentError{true, true, pc, bp, sp, address, isWrite ? 1 : 0, accessSize, bugType});
    report.Append(ReportLine(Format("ERROR: AddressSanitizer: %s on address %s at pc %s bp %s sp %s\n", bugType, Pointer(address).c_str(), Pointer(pc).c_str(),
                                    Pointer(bp).c_str(), Pointer(sp).c_str())));
    report.Append(Format("%s of size %zu at %s thread T-1\n", accessSize ? (isWrite ? "WRITE" : "READ") : "ACCESS", static_cast<std::size_t>(accessSize),
                         Pointer(address).c_str()));
    report.Append(StackLine(pc));
    report.Append(DescribeAccessedAddress(address, accessSize));
    if (shadowValue == ContiguousContainerOOBMagic)
        report.Append("HINT: if you don't care about these errors you may set ASAN_OPTIONS=detect_container_overflow=0.\n"
                      "If you suspect a false positive see also: https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow.\n");
    report.Append(Summary(bugType));
    report.Append(ShadowMemoryForAddress(address));
    report.Finish();
}

void ReportRangesOverlap(const char* function, uptr first, uptr firstLength, uptr second, uptr secondLength, uptr pc, uptr bp, uptr sp) {
    ScopedErrorReport report(false);
    const std::string bugType = std::string(function) + "-param-overlap";
    static thread_local std::string description;
    description = bugType;
    report.Record(CurrentError{true, false, pc, bp, sp, first, 0, 0, description.c_str()});
    report.Append(ReportLine(Format("ERROR: AddressSanitizer: %s: memory ranges [%s,%s) and [%s, %s) overlap\n", bugType.c_str(), Pointer(first).c_str(),
                                    Pointer(first + firstLength).c_str(), Pointer(second).c_str(), Pointer(second + secondLength).c_str())));
    report.Append(StackLine(pc));
    report.Append(DescribeAccessedAddress(first, firstLength));
    report.Append(DescribeAccessedAddress(second, secondLength));
    report.Append(Summary(bugType.c_str()));
    report.Finish();
}

void ReportSizeOverflow(uptr offset, uptr size, uptr pc, uptr bp, uptr sp) {
    ScopedErrorReport report(false);
    report.Record(CurrentError{true, false, pc, bp, sp, offset, 0, 0, "negative-size-param"});
    report.Append(ReportLine(Format("ERROR: AddressSanitizer: negative-size-param: (size=%zd)\n", static_cast<std::ptrdiff_t>(size))));
    report.Append(StackLine(pc));
    report.Append(DescribeAccessedAddress(offset, size));
    report.Append(Summary("negative-size-param"));
    report.Finish();
}

void CheckAccess(uptr address, uptr size, bool isWrite, bool fatal, uptr pc, uptr bp, uptr sp) {
    EnsureShadow();
    if (!AddrIsInMem(address)) {
        ReportGenericError(pc, bp, sp, address, isWrite, size, fatal);
        return;
    }
    const u8* const shadow = ShadowOf(address);
    std::uint16_t value = *shadow;
    if (size > Granularity) std::memcpy(&value, shadow, sizeof(value));
    if (value == 0) return;
    if (size >= Granularity || static_cast<s8>((address & (Granularity - 1)) + size - 1) >= static_cast<s8>(value))
        ReportGenericError(pc, bp, sp, address, isWrite, size, fatal);
}

void CheckRegion(uptr address, uptr size, bool isWrite, bool fatal, uptr pc, uptr bp, uptr sp) {
    EnsureShadow();
    if (const uptr bad = RegionIsPoisoned(address, size)) ReportGenericError(pc, bp, sp, bad, isWrite, size, fatal);
}

bool QuickCheckForUnpoisonedRegion(uptr begin, uptr size) {
    if (size == 0 || size > sizeof(uptr) * Granularity) return size == 0;
    const uptr last = begin + size - 1;
    if (!AddrIsInMem(begin) || !AddrIsInMem(last)) return false;
    u8 shadow = AddressIsPoisoned(last) ? 1 : 0;
    for (const u8* byte = ShadowOf(begin); byte < ShadowOf(last); ++byte) shadow |= *byte;
    return shadow == 0;
}

void AccessMemoryRange(uptr offset, uptr size, bool isWrite, uptr pc, uptr bp, uptr sp) {
    if (offset > offset + size) ReportSizeOverflow(offset, size, pc, bp, sp);
    if (QuickCheckForUnpoisonedRegion(offset, size)) return;
    if (const uptr bad = RegionIsPoisoned(offset, size)) ReportGenericError(pc, bp, sp, bad, isWrite, size, false);
}

bool RangesOverlap(uptr first, uptr firstLength, uptr second, uptr secondLength) {
    if (firstLength == 0 || secondLength == 0) return false;
    return !(first + firstLength <= second || second + secondLength <= first);
}

void PoisonAlignedStackMemory(uptr address, uptr size, bool poison) {
    if (size == 0) return;
    EnsureShadow();
    const uptr alignedSize = size & ~(Granularity - 1);
    PoisonShadow(address, alignedSize, poison ? StackUseAfterScopeMagic : 0);
    if (size == alignedSize) return;
    const s8 endOffset = static_cast<s8>(size - alignedSize);
    RequireShadow(address + alignedSize, 1);
    s8* const shadowEnd = reinterpret_cast<s8*>(ShadowOf(address + alignedSize));
    const s8 endValue = *shadowEnd;
    if (poison) {
        if (endValue > 0 && endValue <= endOffset) *shadowEnd = static_cast<s8>(StackUseAfterScopeMagic);
    } else if (endValue != 0) {
        *shadowEnd = std::max(endValue, endOffset);
    }
}

void PoisonOrUnpoisonIntraObjectRedzone(uptr pointer, uptr size, bool poison) {
    EnsureShadow();
    const uptr end = pointer + size;
    Require(size != 0, "guest AddressSanitizer: intra-object redzone of size 0");
    Require(size <= 4096, "guest AddressSanitizer: intra-object redzone larger than 4096 bytes");
    Require(Aligned(end), "guest AddressSanitizer: intra-object redzone end is not 8-byte aligned");
    RequireShadow(pointer, size);
    if (!Aligned(pointer)) {
        *ShadowOf(pointer) = poison ? static_cast<u8>(pointer % Granularity) : 0;
        pointer |= Granularity - 1;
        ++pointer;
    }
    for (; pointer < end; pointer += Granularity) *ShadowOf(pointer) = poison ? IntraObjectRedzone : 0;
}

void SetShadow(uptr shadow, uptr size, u8 value) {
    if (size == 0) return;
    EnsureShadow();
    Require(ShadowOwned(shadow) && ShadowOwned(shadow + size - 1), "guest AddressSanitizer: __asan_set_shadow target is outside the shadow");
    std::memset(reinterpret_cast<void*>(shadow), value, size);
}

void PoisonRedZones(const Global& global) {
    const uptr alignedSize = RoundUp(global.size, Granularity);
    FastPoisonShadow(global.beg + alignedSize, global.sizeWithRedzone - alignedSize, GlobalRedzoneMagic);
    if (global.size != alignedSize)
        FastPoisonShadowPartialRightRedzone(global.beg + RoundDown(global.size, Granularity), global.size % Granularity, Granularity, GlobalRedzoneMagic);
}

Global ReadGlobal(uptr address, std::size_t stride) {
    Global global;
    if (!GuestCopyIn_nid_no_patch(&global, reinterpret_cast<const void*>(address), stride)) throw std::invalid_argument("guest AddressSanitizer: global descriptor is not readable");
    return global;
}

void RegisterGlobals(uptr globals, uptr count, std::size_t stride) {
    if (GetFlags().reportGlobals == 0 || count == 0) return;
    EnsureShadow();
    Require(globals != 0, "guest AddressSanitizer: null global descriptor array");
    std::lock_guard lock(globalsMutex);
    for (uptr index = 0; index < count; ++index) {
        const Global global = ReadGlobal(globals + index * stride, stride);
        RequireShadow(global.beg, 1);
        Require(Aligned(global.beg), "guest AddressSanitizer: global variable is not 8-byte aligned");
        Require(Aligned(global.sizeWithRedzone), "guest AddressSanitizer: global size with redzone is not 8-byte aligned");
        Require(global.sizeWithRedzone >= global.size, "guest AddressSanitizer: global redzone is smaller than the global");
        PoisonRedZones(global);
        registeredGlobals.push_back(global);
        if (global.hasDynamicInit) dynamicInitGlobals.push_back(DynamicInitGlobal{global, false});
    }
    PoisonShadow(globals, RoundUp(count * stride, Granularity), GlobalRedzoneMagic);
}

void UnregisterGlobals(uptr globals, uptr count, std::size_t stride) {
    if (GetFlags().reportGlobals == 0 || count == 0) return;
    EnsureShadow();
    Require(globals != 0, "guest AddressSanitizer: null global descriptor array");
    std::lock_guard lock(globalsMutex);
    PoisonShadow(globals, RoundUp(count * stride, Granularity), 0);
    for (uptr index = 0; index < count; ++index) {
        const Global global = ReadGlobal(globals + index * stride, stride);
        Require(Aligned(global.beg) && Aligned(global.sizeWithRedzone), "guest AddressSanitizer: global variable is not 8-byte aligned");
        PoisonShadow(global.beg, global.sizeWithRedzone, 0);
    }
}

std::size_t GlobalStride() {
    return moduleAbiVersion == 6 ? GlobalSizeV6 : sizeof(Global);
}

uptr ReadFlag(uptr* flag) {
    uptr value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, flag, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: globals flag is not readable");
    return value;
}

void WriteFlag(uptr* flag, uptr value) {
    if (!GuestCopyOut_nid_no_patch(flag, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: globals flag is not writable");
}

void UnpoisonStack(uptr bottom, uptr top) {
    if (top - bottom > MaxExpectedCleanupSize) {
        static std::atomic<bool> reportedWarning{false};
        if (reportedWarning.exchange(true)) return;
        const std::string warning = ReportLine(Format("WARNING: ASan is ignoring requested __asan_handle_no_return: stack type: default top: %s; bottom %s; size: %s (%zd)\n"
                                                      "False positive error reports may follow\n"
                                                      "For details see https://github.com/google/sanitizers/issues/189\n",
                                                      Pointer(top).c_str(), Pointer(bottom).c_str(), Pointer(top - bottom).c_str(), static_cast<std::ptrdiff_t>(top - bottom)));
        std::fwrite(warning.data(), 1, warning.size(), stderr);
        std::fflush(stderr);
        return;
    }
    PoisonShadow(bottom, RoundUp(top - bottom, Granularity), 0);
}

bool CurrentGuestStack(uptr* bottom, uptr* top) {
    const Pthread self = scePthreadSelf();
    if (self == nullptr) return false;
    PthreadAttr attribute = nullptr;
    if (scePthreadAttrInit(&attribute) != 0) return false;
    void* address = nullptr;
    std::size_t bytes = 0;
    const bool known = scePthreadAttrGet(self, &attribute) == 0 && scePthreadAttrGetstack(&attribute, &address, &bytes) == 0;
    scePthreadAttrDestroy(&attribute);
    if (!known || address == nullptr || bytes == 0) return false;
    *bottom = reinterpret_cast<uptr>(address);
    *top = *bottom + bytes;
    return true;
}

}

namespace Asan {

void CheckSmallRegion(std::uintptr_t address, std::size_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp) {
    if (!shadowReady.load(std::memory_order_acquire)) return;
    if (AddressIsPoisoned(address) || AddressIsPoisoned(address + size - 1)) {
        const uptr bad = RegionIsPoisoned(address, size);
        ReportGenericError(pc, bp, sp, bad, isWrite, size, GetFlags().haltOnError);
    }
}

}

#define APS5_ASAN_CALLER_PC_BP_SP                                                   \
    const uptr pc = reinterpret_cast<uptr>(__builtin_return_address(0));            \
    const uptr bp = reinterpret_cast<uptr>(__builtin_frame_address(0));             \
    volatile char stackMarker = 0;                                                  \
    const uptr sp = reinterpret_cast<uptr>(&stackMarker)

#define APS5_ASAN_ACCESS(type, isWrite, size)                                                       \
    __attribute__((noinline)) void APS5_VABI __asan_##type##size(std::uintptr_t address) {          \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                  \
        CheckAccess(address, size, isWrite, true, pc, bp, sp);                                      \
    }                                                                                               \
    __attribute__((noinline)) void APS5_VABI __asan_exp_##type##size(std::uintptr_t address, std::uint32_t) { \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                  \
        CheckAccess(address, size, isWrite, true, pc, bp, sp);                                      \
    }                                                                                               \
    __attribute__((noinline)) void APS5_VABI __asan_##type##size##_noabort(std::uintptr_t address) { \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                  \
        CheckAccess(address, size, isWrite, false, pc, bp, sp);                                     \
    }

#define APS5_ASAN_REPORT(type, isWrite, size)                                                              \
    __attribute__((noinline)) void APS5_VABI __asan_report_##type##size(std::uintptr_t address) {           \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                         \
        EnsureShadow();                                                                                    \
        ReportGenericError(pc, bp, sp, address, isWrite, size, true);                                      \
    }                                                                                                      \
    __attribute__((noinline)) void APS5_VABI __asan_report_exp_##type##size(std::uintptr_t address, std::uint32_t) { \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                         \
        EnsureShadow();                                                                                    \
        ReportGenericError(pc, bp, sp, address, isWrite, size, true);                                      \
    }                                                                                                      \
    __attribute__((noinline)) void APS5_VABI __asan_report_##type##size##_noabort(std::uintptr_t address) { \
        APS5_ASAN_CALLER_PC_BP_SP;                                                                         \
        EnsureShadow();                                                                                    \
        ReportGenericError(pc, bp, sp, address, isWrite, size, false);                                     \
    }

extern "C" {

APS5_ASAN_ACCESS(load, false, 1)
APS5_ASAN_ACCESS(load, false, 2)
APS5_ASAN_ACCESS(load, false, 4)
APS5_ASAN_ACCESS(load, false, 8)
APS5_ASAN_ACCESS(load, false, 16)
APS5_ASAN_ACCESS(store, true, 1)
APS5_ASAN_ACCESS(store, true, 2)
APS5_ASAN_ACCESS(store, true, 4)
APS5_ASAN_ACCESS(store, true, 8)
APS5_ASAN_ACCESS(store, true, 16)

APS5_ASAN_REPORT(load, false, 1)
APS5_ASAN_REPORT(load, false, 2)
APS5_ASAN_REPORT(load, false, 4)
APS5_ASAN_REPORT(load, false, 8)
APS5_ASAN_REPORT(load, false, 16)
APS5_ASAN_REPORT(store, true, 1)
APS5_ASAN_REPORT(store, true, 2)
APS5_ASAN_REPORT(store, true, 4)
APS5_ASAN_REPORT(store, true, 8)
APS5_ASAN_REPORT(store, true, 16)

__attribute__((noinline)) void APS5_VABI __asan_loadN(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, false, true, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_exp_loadN(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, false, true, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_loadN_noabort(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, false, false, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_storeN(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, true, true, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_exp_storeN(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, true, true, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_storeN_noabort(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    CheckRegion(address, size, true, false, pc, bp, sp);
}

__attribute__((noinline)) void APS5_VABI __asan_report_load_n(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, false, size, true);
}

__attribute__((noinline)) void APS5_VABI __asan_report_exp_load_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, false, size, true);
}

__attribute__((noinline)) void APS5_VABI __asan_report_load_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, false, size, false);
}

__attribute__((noinline)) void APS5_VABI __asan_report_store_n(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, true, size, true);
}

__attribute__((noinline)) void APS5_VABI __asan_report_exp_store_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, true, size, true);
}

__attribute__((noinline)) void APS5_VABI __asan_report_store_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, true, size, false);
}

void APS5_VABI __asan_report_error(std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp, std::uintptr_t address, int isWrite, std::uintptr_t accessSize, std::uint32_t) {
    EnsureShadow();
    ReportGenericError(pc, bp, sp, address, isWrite != 0, accessSize, GetFlags().haltOnError);
}

int APS5_VABI __asan_report_present() {
    return currentError.present ? 1 : 0;
}

std::uintptr_t APS5_VABI __asan_get_report_pc() {
    return currentError.present && currentError.generic ? currentError.pc : 0;
}

std::uintptr_t APS5_VABI __asan_get_report_bp() {
    return currentError.present && currentError.generic ? currentError.bp : 0;
}

std::uintptr_t APS5_VABI __asan_get_report_sp() {
    return currentError.present && currentError.generic ? currentError.sp : 0;
}

std::uintptr_t APS5_VABI __asan_get_report_address() {
    return currentError.present && currentError.generic ? currentError.address : 0;
}

int APS5_VABI __asan_get_report_access_type() {
    return currentError.present && currentError.generic ? currentError.isWrite : 0;
}

std::uintptr_t APS5_VABI __asan_get_report_access_size() {
    return currentError.present && currentError.generic ? currentError.accessSize : 0;
}

const char* APS5_VABI __asan_get_report_description() {
    return currentError.present ? currentError.description : "";
}

void APS5_VABI __asan_set_error_report_callback(void(APS5_VABI* callback)(const char*)) {
    errorReportCallback.store(callback, std::memory_order_release);
}

void APS5_VABI __sanitizer_set_death_callback(void(APS5_VABI* callback)()) {
    deathCallback.store(callback, std::memory_order_release);
}

void APS5_VABI __asan_describe_address(std::uintptr_t address) {
    EnsureShadow();
    const std::string text = DescribeAddress(address);
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fflush(stderr);
}

void APS5_VABI __asan_init() {
    EnsureShadow();
}

void APS5_VABI __asan_init_v5() {
    EnsureShadow();
}

void APS5_VABI __asan_version_mismatch_check_v6() {
    moduleAbiVersion = 6;
}

void APS5_VABI __asan_version_mismatch_check_v7() {
    moduleAbiVersion = 7;
}

void APS5_VABI __asan_version_mismatch_check_v8() {
    moduleAbiVersion = 8;
}

void APS5_VABI __asan_get_shadow_mapping(std::uintptr_t* shadowScale, std::uintptr_t* shadowOffset) {
    if (shadowScale != nullptr && !GuestCopyOut_nid_no_patch(shadowScale, &ShadowScale, sizeof(ShadowScale)))
        throw std::invalid_argument("guest AddressSanitizer: shadow scale output is not writable");
    if (shadowOffset != nullptr && !GuestCopyOut_nid_no_patch(shadowOffset, &ShadowOffset, sizeof(ShadowOffset)))
        throw std::invalid_argument("guest AddressSanitizer: shadow offset output is not writable");
}

__attribute__((noinline)) void APS5_VABI __asan_handle_no_return() {
    if (!shadowReady.load(std::memory_order_acquire)) return;
    uptr stackBottom = 0;
    uptr top = 0;
    if (!CurrentGuestStack(&stackBottom, &top)) return;
    volatile char localStack = 0;
    const uptr bottom = (reinterpret_cast<uptr>(&localStack) - StackPageSize) & ~(StackPageSize - 1);
    UnpoisonStack(bottom, top);
}

void APS5_VABI __asan_poison_memory_region(const volatile void* address, std::uintptr_t size) {
    if (!GetFlags().allowUserPoisoning || size == 0) return;
    EnsureShadow();
    const uptr beginAddress = reinterpret_cast<uptr>(address);
    const uptr endAddress = beginAddress + size;
    Require(endAddress > beginAddress, "guest AddressSanitizer: poisoned region wraps around");
    RequireShadow(beginAddress, size + 1);
    ShadowSegmentEndpoint begin(beginAddress);
    ShadowSegmentEndpoint end(endAddress);
    if (begin.chunk == end.chunk) {
        Require(begin.offset < end.offset && begin.value == end.value, "guest AddressSanitizer: inconsistent shadow in poisoned region");
        const s8 value = begin.value;
        if (value > 0 && value <= end.offset) *begin.chunk = begin.offset > 0 ? static_cast<u8>(std::min(value, begin.offset)) : UserPoisonedMemoryMagic;
        return;
    }
    if (begin.offset > 0) {
        *begin.chunk = begin.value == 0 ? static_cast<u8>(begin.offset) : static_cast<u8>(std::min(begin.value, begin.offset));
        ++begin.chunk;
    }
    std::memset(begin.chunk, UserPoisonedMemoryMagic, static_cast<std::size_t>(end.chunk - begin.chunk));
    if (end.value > 0 && end.value <= end.offset) *end.chunk = UserPoisonedMemoryMagic;
}

void APS5_VABI __asan_unpoison_memory_region(const volatile void* address, std::uintptr_t size) {
    if (!GetFlags().allowUserPoisoning || size == 0) return;
    EnsureShadow();
    const uptr beginAddress = reinterpret_cast<uptr>(address);
    const uptr endAddress = beginAddress + size;
    Require(endAddress > beginAddress, "guest AddressSanitizer: unpoisoned region wraps around");
    RequireShadow(beginAddress, size + 1);
    ShadowSegmentEndpoint begin(beginAddress);
    ShadowSegmentEndpoint end(endAddress);
    if (begin.chunk == end.chunk) {
        Require(begin.offset < end.offset && begin.value == end.value, "guest AddressSanitizer: inconsistent shadow in unpoisoned region");
        if (begin.value != 0) *begin.chunk = static_cast<u8>(std::max(begin.value, end.offset));
        return;
    }
    if (begin.offset > 0) {
        *begin.chunk = 0;
        ++begin.chunk;
    }
    std::memset(begin.chunk, 0, static_cast<std::size_t>(end.chunk - begin.chunk));
    if (end.offset > 0 && end.value != 0) *end.chunk = static_cast<u8>(std::max(end.value, end.offset));
}

int APS5_VABI __asan_address_is_poisoned(const volatile void* address) {
    EnsureShadow();
    return AddressIsPoisoned(reinterpret_cast<uptr>(address)) ? 1 : 0;
}

std::uintptr_t APS5_VABI __asan_region_is_poisoned(std::uintptr_t begin, std::uintptr_t size) {
    EnsureShadow();
    return RegionIsPoisoned(begin, size);
}

void APS5_VABI __asan_poison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    PoisonAlignedStackMemory(address, size, true);
}

void APS5_VABI __asan_unpoison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    PoisonAlignedStackMemory(address, size, false);
}

void APS5_VABI __asan_alloca_poison(std::uintptr_t address, std::uintptr_t size) {
    EnsureShadow();
    const uptr leftRedzone = address - AllocaRedzoneSize;
    const uptr partialRedzone = address + size;
    const uptr rightRedzone = (partialRedzone + AllocaRedzoneMask) & ~AllocaRedzoneMask;
    const uptr partialAligned = partialRedzone & ~(Granularity - 1);
    FastPoisonShadow(leftRedzone, AllocaRedzoneSize, AllocaLeftMagic);
    FastPoisonShadowPartialRightRedzone(partialAligned, partialRedzone % Granularity, rightRedzone - partialAligned, AllocaRightMagic);
    FastPoisonShadow(rightRedzone, AllocaRedzoneSize, AllocaRightMagic);
}

void APS5_VABI __asan_allocas_unpoison(std::uintptr_t top, std::uintptr_t bottom) {
    if (top == 0 || top > bottom) return;
    EnsureShadow();
    const uptr bytes = (bottom - top) / Granularity;
    if (bytes == 0) return;
    RequireShadow(top, bottom - top);
    std::memset(ShadowOf(top), 0, bytes);
}

void APS5_VABI __asan_poison_intra_object_redzone(std::uintptr_t pointer, std::uintptr_t size) {
    PoisonOrUnpoisonIntraObjectRedzone(pointer, size, true);
}

void APS5_VABI __asan_unpoison_intra_object_redzone(std::uintptr_t pointer, std::uintptr_t size) {
    PoisonOrUnpoisonIntraObjectRedzone(pointer, size, false);
}

void APS5_VABI __asan_poison_cxx_array_cookie(std::uintptr_t pointer) {
    if (!GetFlags().poisonArrayCookie) return;
    EnsureShadow();
    RequireShadow(pointer, 1);
    *ShadowOf(pointer) = ArrayCookieMagic;
}

std::uintptr_t APS5_VABI __asan_load_cxx_array_cookie(std::uintptr_t* pointer) {
    uptr cookie = 0;
    if (!GuestCopyIn_nid_no_patch(&cookie, pointer, sizeof(cookie))) throw std::invalid_argument("guest AddressSanitizer: array cookie is not readable");
    if (!GetFlags().poisonArrayCookie) return cookie;
    EnsureShadow();
    const uptr address = reinterpret_cast<uptr>(pointer);
    RequireShadow(address, 1);
    const u8 value = *ShadowOf(address);
    if (value == ArrayCookieMagic) return cookie;
    if (value == HeapFreeMagic) {
        const std::string warning = ReportLine("AddressSanitizer: loaded array cookie from free-d memory; expect a double-free report\n");
        std::fwrite(warning.data(), 1, warning.size(), stderr);
        std::fflush(stderr);
        return 0;
    }
    return cookie;
}

void APS5_VABI __asan_set_shadow_00(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x00); }
void APS5_VABI __asan_set_shadow_01(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x01); }
void APS5_VABI __asan_set_shadow_02(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x02); }
void APS5_VABI __asan_set_shadow_03(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x03); }
void APS5_VABI __asan_set_shadow_04(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x04); }
void APS5_VABI __asan_set_shadow_05(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x05); }
void APS5_VABI __asan_set_shadow_06(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x06); }
void APS5_VABI __asan_set_shadow_07(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0x07); }
void APS5_VABI __asan_set_shadow_f1(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0xf1); }
void APS5_VABI __asan_set_shadow_f2(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0xf2); }
void APS5_VABI __asan_set_shadow_f3(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0xf3); }
void APS5_VABI __asan_set_shadow_f5(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0xf5); }
void APS5_VABI __asan_set_shadow_f8(std::uintptr_t address, std::uintptr_t size) { SetShadow(address, size, 0xf8); }

void APS5_VABI __asan_register_globals(const void* globals, std::uintptr_t count) {
    RegisterGlobals(reinterpret_cast<uptr>(globals), count, GlobalStride());
}

void APS5_VABI __asan_unregister_globals(const void* globals, std::uintptr_t count) {
    UnregisterGlobals(reinterpret_cast<uptr>(globals), count, GlobalStride());
}

void APS5_VABI __asan_register_elf_globals(std::uintptr_t* flag, void* start, void* stop) {
    if (ReadFlag(flag) != 0 || start == nullptr) return;
    const uptr bytes = reinterpret_cast<uptr>(stop) - reinterpret_cast<uptr>(start);
    Require(bytes % sizeof(Global) == 0, "guest AddressSanitizer: ELF globals section is not a whole number of descriptors");
    RegisterGlobals(reinterpret_cast<uptr>(start), bytes / sizeof(Global), sizeof(Global));
    WriteFlag(flag, 1);
}

void APS5_VABI __asan_unregister_elf_globals(std::uintptr_t* flag, void* start, void* stop) {
    if (ReadFlag(flag) == 0 || start == nullptr) return;
    const uptr bytes = reinterpret_cast<uptr>(stop) - reinterpret_cast<uptr>(start);
    Require(bytes % sizeof(Global) == 0, "guest AddressSanitizer: ELF globals section is not a whole number of descriptors");
    UnregisterGlobals(reinterpret_cast<uptr>(start), bytes / sizeof(Global), sizeof(Global));
    WriteFlag(flag, 0);
}

void APS5_VABI __asan_before_dynamic_init(const char* moduleName) {
    if (!GetFlags().checkInitializationOrder) return;
    Require(moduleName != nullptr, "guest AddressSanitizer: null module name for dynamic initialization");
    EnsureShadow();
    std::lock_guard lock(globalsMutex);
    for (auto& entry : dynamicInitGlobals) {
        if (entry.initialized) continue;
        if (entry.global.moduleName != reinterpret_cast<uptr>(moduleName)) FastPoisonShadow(entry.global.beg, entry.global.sizeWithRedzone, InitializationOrderMagic);
        else if (!GetFlags().strictInitOrder) entry.initialized = true;
    }
}

void APS5_VABI __asan_after_dynamic_init() {
    if (!GetFlags().checkInitializationOrder) return;
    EnsureShadow();
    std::lock_guard lock(globalsMutex);
    for (const auto& entry : dynamicInitGlobals) {
        if (entry.initialized) continue;
        FastPoisonShadow(entry.global.beg, entry.global.sizeWithRedzone, 0);
        PoisonRedZones(entry.global);
    }
}

__attribute__((noinline)) void* APS5_VABI __asan_memcpy(void* to, const void* from, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    const uptr destination = reinterpret_cast<uptr>(to);
    const uptr source = reinterpret_cast<uptr>(from);
    if (to != from && RangesOverlap(destination, size, source, size)) ReportRangesOverlap("memcpy", destination, size, source, size, pc, bp, sp);
    AccessMemoryRange(source, size, false, pc, bp, sp);
    AccessMemoryRange(destination, size, true, pc, bp, sp);
    return std::memcpy(to, from, size);
}

__attribute__((noinline)) void* APS5_VABI __asan_memset(void* block, int value, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    AccessMemoryRange(reinterpret_cast<uptr>(block), size, true, pc, bp, sp);
    return std::memset(block, value, size);
}

__attribute__((noinline)) void* APS5_VABI __asan_memmove(void* to, const void* from, std::uintptr_t size) {
    APS5_ASAN_CALLER_PC_BP_SP;
    EnsureShadow();
    AccessMemoryRange(reinterpret_cast<uptr>(from), size, false, pc, bp, sp);
    AccessMemoryRange(reinterpret_cast<uptr>(to), size, true, pc, bp, sp);
    return std::memmove(to, from, size);
}

}
