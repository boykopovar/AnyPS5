#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringImpl;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "StringBuilderJSON guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template <typename Action>
bool ThrowsNotImplemented(Action&& action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "_ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE not implemented";
    }
    return false;
}

template <typename Action>
bool ThrowsInvalidArgument(Action&& action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

StringBuilder NewBuilder() {
    StringBuilder builder{};
    builder.buffer = nullptr;
    builder.characters8 = nullptr;
    builder.overflowed = false;
    builder.shouldCrashOnOverflow = true;
    builder.length = 0;
    builder.is8Bit = true;
    return builder;
}

}  // namespace

int main() {
    GuardedPages pages;
    const String valid = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("abc"), 3);

    Require(ThrowsInvalidArgument([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(nullptr, valid); }));
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(reinterpret_cast<StringBuilder*>(0x10), valid); }));
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(reinterpret_cast<StringBuilder*>(pages.ReadOnly()), valid); }));
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(reinterpret_cast<StringBuilder*>(pages.Guard()), valid); }));

    unsigned char* splitBuilder = pages.ReadOnly() - 16;
    std::memset(splitBuilder, 0x5A, 16);
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(reinterpret_cast<StringBuilder*>(splitBuilder), valid); }));
    for (std::size_t i = 0; i < 16; ++i) Require(splitBuilder[i] == 0x5A);

    StringBuilder builder = NewBuilder();
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, *reinterpret_cast<const String*>(pages.Guard())); }));
    Require(builder.length == 0 && builder.buffer == nullptr && !builder.overflowed);

    alignas(String) unsigned char badStringRaw[sizeof(String)]{};
    const auto* badImpl = reinterpret_cast<const StringImpl*>(pages.Guard());
    std::memcpy(badStringRaw, &badImpl, sizeof(badImpl));
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, *reinterpret_cast<const String*>(badStringRaw)); }));
    Require(builder.length == 0 && builder.buffer == nullptr);

    StringImpl splitDataImpl{2, 8, pages.Guard() - 4, 4};
    const auto* splitDataPtr = &splitDataImpl;
    std::memcpy(badStringRaw, &splitDataPtr, sizeof(splitDataPtr));
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, *reinterpret_cast<const String*>(badStringRaw)); }));
    Require(builder.length == 0 && builder.buffer == nullptr);

    StringImpl nullDataImpl{2, 4, nullptr, 4};
    const auto* nullDataPtr = &nullDataImpl;
    std::memcpy(badStringRaw, &nullDataPtr, sizeof(nullDataPtr));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, *reinterpret_cast<const String*>(badStringRaw)); }));
    Require(builder.length == 0 && builder.buffer == nullptr);

    StringImpl readOnlyBufferHeader{2, 64, pages.ReadOnly(), 4};
    StringBuilder readOnlyTarget = NewBuilder();
    readOnlyTarget.buffer = &readOnlyBufferHeader;
    readOnlyTarget.characters8 = pages.ReadOnly();
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&readOnlyTarget, valid); }));
    Require(readOnlyTarget.length == 0);

    StringBuilder guardBuffer = NewBuilder();
    guardBuffer.buffer = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&guardBuffer, valid); }));
    Require(guardBuffer.length == 0);

    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, valid);
    Require(builder.is8Bit && builder.length == 5 && builder.buffer != nullptr);
    Require(std::memcmp(builder.characters8, "\"abc\"", 5) == 0);
    NKWebKit::WtfAbi::Release(builder.buffer);
    builder.buffer = nullptr;
    return 0;
}
