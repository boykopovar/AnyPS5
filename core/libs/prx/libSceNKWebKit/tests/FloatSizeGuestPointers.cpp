#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/FloatSizeExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FloatSize guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

struct GuardedPages {
    std::byte* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
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
        base = static_cast<std::byte*>(mapped);
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

    std::byte* Writable() const { return base; }
    std::byte* ReadOnly() const { return base + pageSize; }
    std::byte* Guard() const { return base + pageSize * 2; }
};

template <typename Function>
bool ThrowsInvalidArgument(Function&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Function>
bool ThrowsNotImplemented(Function&& function, const char* symbol) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

using Constructor = void(APS5_VABI*)(WebCore::FloatSize*, const WebCore::IntSize&);

void TestConstructor(Constructor construct, const char* symbol, const GuardedPages& pages) {
    const WebCore::IntSize source{-17, 23};
    WebCore::FloatSize out{};
    std::array<unsigned char, sizeof(WebCore::FloatSize)> sentinel{};
    sentinel.fill(0x5A);
    std::memcpy(&out, sentinel.data(), sentinel.size());

    Require(ThrowsInvalidArgument([&] { construct(nullptr, source); }));
    Require(ThrowsInvalidArgument([&] { construct(reinterpret_cast<WebCore::FloatSize*>(0x10), source); }));
    Require(ThrowsInvalidArgument([&] { construct(reinterpret_cast<WebCore::FloatSize*>(pages.ReadOnly()), source); }));
    Require(ThrowsInvalidArgument([&] { construct(reinterpret_cast<WebCore::FloatSize*>(pages.Guard()), source); }));

    auto* splitWritable = pages.ReadOnly() - 4;
    std::array<unsigned char, 4> prefixSentinel{0xA1, 0xB2, 0xC3, 0xD4};
    std::memcpy(splitWritable, prefixSentinel.data(), prefixSentinel.size());
    Require(ThrowsInvalidArgument([&] { construct(reinterpret_cast<WebCore::FloatSize*>(splitWritable), source); }));
    Require(std::memcmp(splitWritable, prefixSentinel.data(), prefixSentinel.size()) == 0);

    Require(ThrowsNotImplemented([&] { construct(&out, *reinterpret_cast<const WebCore::IntSize*>(0x10)); }, symbol));
    Require(std::memcmp(&out, sentinel.data(), sentinel.size()) == 0);
    Require(ThrowsNotImplemented([&] { construct(&out, *reinterpret_cast<const WebCore::IntSize*>(pages.Guard())); }, symbol));
    Require(std::memcmp(&out, sentinel.data(), sentinel.size()) == 0);
    Require(ThrowsNotImplemented([&] { construct(&out, *reinterpret_cast<const WebCore::IntSize*>(pages.Guard() - 4)); }, symbol));
    Require(std::memcmp(&out, sentinel.data(), sentinel.size()) == 0);

    construct(&out, source);
    Require(out.m_width == -17.0f && out.m_height == 23.0f);

    const WebCore::IntSize extremes{std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    construct(&out, extremes);
    Require(out.m_width == static_cast<float>(std::numeric_limits<int>::min()));
    Require(out.m_height == static_cast<float>(std::numeric_limits<int>::max()));

    union Aliased {
        WebCore::IntSize intSize;
        WebCore::FloatSize floatSize;
    } aliased{};
    aliased.intSize = {42, -9};
    construct(&aliased.floatSize, aliased.intSize);
    Require(aliased.floatSize.m_width == 42.0f && aliased.floatSize.m_height == -9.0f);
}

}  // namespace

int main() {
    GuardedPages pages;
    TestConstructor(&_ZN7WebCore9FloatSizeC1ERKNS_7IntSizeE, "_ZN7WebCore9FloatSizeC1ERKNS_7IntSizeE", pages);
    TestConstructor(&_ZN7WebCore9FloatSizeC2ERKNS_7IntSizeE, "_ZN7WebCore9FloatSizeC2ERKNS_7IntSizeE", pages);
    return 0;
}
