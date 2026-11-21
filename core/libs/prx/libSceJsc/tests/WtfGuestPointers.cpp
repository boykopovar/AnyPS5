#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

unsigned char* MapPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

struct alignas(16) Storage {
    std::size_t numBits;
    std::uintptr_t words[2];
};

void TestBitVector() {
    auto* unreadable = reinterpret_cast<BitVector*>(0x10);
    BitVector inlineVector{0xF0ULL | InlineFlag};
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9BitVector10filterSlowERKS0_(unreadable, &inlineVector); return 0; },
        "_ZN3WTF9BitVector10filterSlowERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9BitVector10filterSlowERKS0_(&inlineVector, unreadable); return 0; },
        "_ZN3WTF9BitVector10filterSlowERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9BitVector15resizeOutOfLineEm(unreadable, 128); return 0; },
        "_ZN3WTF9BitVector15resizeOutOfLineEm"));
    Require(inlineVector.bitsOrPointer == (0xF0ULL | InlineFlag));

    Storage storage{128, {0x3CULL, 0xFFULL}};
    const BitVector outOfLine{reinterpret_cast<std::uintptr_t>(&storage) >> 1};
    unsigned char* page = MapPage();
    auto* readOnly = reinterpret_cast<BitVector*>(page);
    *readOnly = inlineVector;
    ProtectReadOnly(page);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9BitVector10filterSlowERKS0_(readOnly, &outOfLine); return 0; },
        "_ZN3WTF9BitVector10filterSlowERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9BitVector15resizeOutOfLineEm(readOnly, 128); return 0; },
        "_ZN3WTF9BitVector15resizeOutOfLineEm"));
    Require(readOnly->bitsOrPointer == (0xF0ULL | InlineFlag));
    Require(storage.words[0] == 0x3CULL && storage.words[1] == 0xFFULL);

    BitVector target = outOfLine;
    _ZN3WTF9BitVector10filterSlowERKS0_(&target, readOnly);
    Require(target.bitsOrPointer == outOfLine.bitsOrPointer && storage.words[0] == 0x30ULL && storage.words[1] == 0xFFULL);
    UnmapPage(page);

    unsigned char* writable = MapPage();
    auto* guest = reinterpret_cast<BitVector*>(writable);
    *guest = BitVector{0x0FULL | InlineFlag};
    _ZN3WTF9BitVector10filterSlowERKS0_(guest, &outOfLine);
    Require(guest->bitsOrPointer == (0x00ULL | InlineFlag));
    *guest = BitVector{0x5ULL | InlineFlag};
    _ZN3WTF9BitVector15resizeOutOfLineEm(guest, 130);
    Require(!(guest->bitsOrPointer & InlineFlag));
    auto* bits = reinterpret_cast<std::size_t*>(guest->bitsOrPointer << 1);
    Require(bits[0] == 192 && bits[1] == 0x5ULL && bits[2] == 0 && bits[3] == 0);
    UnmapPage(writable);
}

void TestMediaTime() {
    MediaTime half{};
    half.timeValue = 1;
    half.timeScale = 2;
    half.timeFlags = MediaTime::Valid;
    auto* unreadable = reinterpret_cast<MediaTime*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimeneERKS0_(unreadable, &half); }, "_ZNK3WTF9MediaTimeneERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimeneERKS0_(&half, unreadable); }, "_ZNK3WTF9MediaTimeneERKS0_"));

    unsigned char* page = MapPage();
    auto* readOnly = reinterpret_cast<MediaTime*>(page);
    *readOnly = half;
    readOnly->timeValue = 2;
    readOnly->timeScale = 4;
    ProtectReadOnly(page);
    Require(!_ZNK3WTF9MediaTimeneERKS0_(readOnly, &half));
    Require(!_ZNK3WTF9MediaTimeneERKS0_(&half, readOnly));
    half.timeScale = 3;
    Require(_ZNK3WTF9MediaTimeneERKS0_(readOnly, &half));
    UnmapPage(page);
}

void TestTime() {
    auto* unreadableTime = reinterpret_cast<TimeWithDynamicClockType*>(0x10);
    auto* unreadableSeconds = reinterpret_cast<Seconds*>(0x10);
    const TimeWithDynamicClockType early{1.5, ClockType::Monotonic};
    const Seconds offset{0.25};
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(unreadableTime, &early); },
        "_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&early, unreadableTime); },
        "_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(unreadableSeconds, &early); },
        "_ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&offset, unreadableTime); },
        "_ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE"));

    unsigned char* page = MapPage();
    auto* late = reinterpret_cast<TimeWithDynamicClockType*>(page);
    *late = TimeWithDynamicClockType{2.5, ClockType::Wall};
    auto* step = reinterpret_cast<Seconds*>(page + 64);
    *step = Seconds{0.5};
    ProtectReadOnly(page);
    const TimeWithDynamicClockType wall{1.0, ClockType::Wall};
    Require(_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wall, late) && !_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(late, &wall));
    const TimeWithDynamicClockType sum = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(step, late);
    Require(sum.value == 3.0 && sum.type == ClockType::Wall);
    UnmapPage(page);
}

void TestStringBuilder() {
    Require(FaultsAsNotImplemented([] { return _ZNK3WTF13StringBuilder10isAllASCIIEv(reinterpret_cast<StringBuilder*>(0x10)); },
        "_ZNK3WTF13StringBuilder10isAllASCIIEv"));
    const char* text = "abc";
    StringImpl ascii{2, 3, text, StringImplFlag8Bit};
    unsigned char* page = MapPage();
    auto* builder = reinterpret_cast<StringBuilder*>(page);
    *builder = StringBuilder{&ascii, nullptr, nullptr, false, false, 3, true};
    ProtectReadOnly(page);
    Require(_ZNK3WTF13StringBuilder10isAllASCIIEv(builder));
    UnmapPage(page);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestBitVector();
    TestMediaTime();
    TestTime();
    TestStringBuilder();
    return 0;
}
