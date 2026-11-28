#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PAL::fileSizeDescription check failed at line %u\n", location.line());
        std::abort();
    }
}

using FileSizeDescription = NKWebKit::WtfAbi::String (APS5_VABI *)(std::uint64_t);
static_assert(std::is_same_v<decltype(&_ZN3PAL19fileSizeDescriptionEm), FileSizeDescription>);

std::string Describe(std::uint64_t size) {
    NKWebKit::WtfAbi::String value = _ZN3PAL19fileSizeDescriptionEm(size);
    Require(value.impl != nullptr);
    Require((value.impl->hashAndFlags & 4) != 0);
    Require(value.impl->refCount == 2);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

void TestBytes() {
    Require(Describe(0) == "0 bytes");
    Require(Describe(1) == "1 bytes");
    Require(Describe(999) == "999 bytes");
    std::printf("PAL::fileSizeDescription bytes: sizes below 1000 passed\n");
}

void TestKilobytes() {
    Require(Describe(1000) == "1.0 KB");
    Require(Describe(1049) == "1.0 KB");
    Require(Describe(1050) == "1.1 KB");
    Require(Describe(1150) == "1.1 KB");
    Require(Describe(1250) == "1.3 KB");
    Require(Describe(1350) == "1.4 KB");
    Require(Describe(999949) == "999.9 KB");
    Require(Describe(999950) == "1000.0 KB");
    Require(Describe(999999) == "1000.0 KB");
    std::printf("PAL::fileSizeDescription kilobytes: boundaries and exact-value rounding passed\n");
}

void TestMegabytes() {
    Require(Describe(1000000) == "1.0 MB");
    Require(Describe(1250000) == "1.3 MB");
    Require(Describe(1500000) == "1.5 MB");
    Require(Describe(999949999) == "999.9 MB");
    Require(Describe(999999999) == "1000.0 MB");
    std::printf("PAL::fileSizeDescription megabytes: boundaries and rounding passed\n");
}

void TestGigabytes() {
    Require(Describe(1000000000) == "1.0 GB");
    Require(Describe(1250000000) == "1.3 GB");
    Require(Describe(123456789012) == "123.5 GB");
    Require(Describe(UINT64_MAX) == "18446744073.7 GB");
    std::printf("PAL::fileSizeDescription gigabytes: boundary, rounding and maximum size passed\n");
}

}

int main() {
    TestBytes();
    TestKilobytes();
    TestMegabytes();
    TestGigabytes();
}
