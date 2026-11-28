#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace Asan::Runtime {

using uptr = std::uintptr_t;
using sptr = std::intptr_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

inline constexpr uptr ShadowScale = 3;
inline constexpr uptr Granularity = uptr{1} << ShadowScale;
inline constexpr uptr ShadowOffset = uptr{1} << 40;
inline constexpr uptr MemoryEnd = uptr{1} << 47;
inline constexpr uptr ShadowBegin = ShadowOffset;
inline constexpr uptr ShadowEnd = ShadowOffset + (MemoryEnd >> ShadowScale);
inline constexpr uptr ChunkBytes = uptr{1} << 30;
inline constexpr std::size_t ChunkCount = (ShadowEnd - ShadowBegin) / ChunkBytes;
inline constexpr uptr AllocaRedzoneSize = 32;
inline constexpr uptr AllocaRedzoneMask = AllocaRedzoneSize - 1;
inline constexpr uptr MaxExpectedCleanupSize = uptr{64} << 20;
inline constexpr uptr StackPageSize = 0x4000;
inline constexpr std::size_t BuggyPcPoolSize = 25;
inline constexpr std::size_t GlobalsPerDescription = 4;
inline constexpr std::size_t GuestStringMaximum = 4096;

inline constexpr u8 HeapLeftRedzoneMagic = 0xfa;
inline constexpr u8 HeapFreeMagic = 0xfd;
inline constexpr u8 StackLeftRedzoneMagic = 0xf1;
inline constexpr u8 StackMidRedzoneMagic = 0xf2;
inline constexpr u8 StackRightRedzoneMagic = 0xf3;
inline constexpr u8 StackAfterReturnMagic = 0xf5;
inline constexpr u8 InitializationOrderMagic = 0xf6;
inline constexpr u8 UserPoisonedMemoryMagic = 0xf7;
inline constexpr u8 ContiguousContainerOOBMagic = 0xfc;
inline constexpr u8 StackUseAfterScopeMagic = 0xf8;
inline constexpr u8 GlobalRedzoneMagic = 0xf9;
inline constexpr u8 InternalHeapMagic = 0xfe;
inline constexpr u8 ArrayCookieMagic = 0xac;
inline constexpr u8 IntraObjectRedzone = 0xbb;
inline constexpr u8 AllocaLeftMagic = 0xca;
inline constexpr u8 AllocaRightMagic = 0xcb;

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
    bool poisonHeap = true;
    bool checkMallocUsableSize = true;
    bool allocatorMayReturnNull = false;
    bool allocatorFreesAndReturnsNullOnReallocZero = true;
    bool detectContainerOverflow = true;
    bool detectStackUseAfterReturn = false;
    int reportGlobals = 1;
    int exitCode = 1;
    int quarantineSizeMb = -1;
    int redzone = 16;
    int maxRedzone = 2048;
    int maxMallocFillSize = 0x1000;
    int mallocFillByte = 0xbe;
    int maxFreeFillSize = 0;
    int freeFillByte = 0x55;
    int minUarStackSizeLog = 16;
    int maxUarStackSizeLog = 20;
    int maxAllocationSizeMb = 0;
};

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

using ErrorReportCallback = void(APS5_VABI*)(const char*);
using DeathCallback = void(APS5_VABI*)();

extern std::atomic<ErrorReportCallback> errorReportCallback;
extern std::atomic<DeathCallback> deathCallback;
extern CurrentError currentError;
extern std::mutex reportMutex;
extern thread_local bool reportingOnThisThread;

const Flags& GetFlags();
void EnsureShadow();
bool ShadowReady();
bool ShadowOwned(uptr shadow);
u8* ShadowOf(uptr address);
uptr MemoryOf(uptr shadow);
bool AddrIsInMem(uptr address);
bool Aligned(uptr value);
uptr RoundUp(uptr value, uptr boundary);
uptr RoundDown(uptr value, uptr boundary);
void Require(bool condition, const char* message);
void RequireShadow(uptr address, uptr size);
void FastPoisonShadow(uptr address, uptr size, u8 value);
void PoisonShadow(uptr address, uptr size, u8 value);
void FastPoisonShadowPartialRightRedzone(uptr alignedAddress, uptr size, uptr redzoneSize, u8 value);
bool AddressIsPoisoned(uptr address);
uptr RegionIsPoisoned(uptr begin, uptr size);

std::string Format(const char* format, ...);
std::string Pointer(uptr value);
std::string ReportLine(const std::string& text);
std::string StackLine(uptr pc);
std::string Summary(const char* bugType);
std::string ShadowMemoryForAddress(uptr address);
std::string GuestString(uptr address);
std::string DescribeAccessedAddress(uptr address, uptr accessSize);
std::string DescribeAddress(uptr address);
void Emit(const std::string& text);
[[noreturn]] void Die();

class ScopedErrorReport {
public:
    explicit ScopedErrorReport(bool fatal);
    ScopedErrorReport(const ScopedErrorReport&) = delete;
    ScopedErrorReport& operator=(const ScopedErrorReport&) = delete;
    ~ScopedErrorReport();

    void Append(const std::string& more) { text += more; }
    void Record(const CurrentError& error) { currentError = error; }
    void Finish();

private:
    bool halt;
    std::unique_lock<std::mutex> lock;
    std::string text;
};

bool DescribeHeapAddress(uptr address, uptr accessSize, std::string* text);
bool DescribeStackAddress(uptr address, uptr accessSize, std::string* text);
bool HeapRegionForAddress(uptr address, uptr* begin, uptr* size);
bool StackRegionForAddress(uptr address, char* name, uptr nameSize, uptr* begin, uptr* size);
bool GlobalRegionForAddress(uptr address, char* name, uptr nameSize, uptr* begin, uptr* size);
void FakeStackHandleNoReturn();
bool CurrentStackBounds(uptr* bottom, uptr* top);

}
