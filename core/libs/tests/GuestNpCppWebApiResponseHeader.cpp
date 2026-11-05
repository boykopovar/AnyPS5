#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <source_location>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

constexpr std::size_t kHeaderSize = 0x10;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kReferenceCountOffset = 0x00;
constexpr std::size_t kHeadersOffset = 0x08;
constexpr int kBodyMissing = static_cast<int>(0x80553512);
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kHeaderSize + kGuardSize];
};

struct IntrusivePtrStorage {
    alignas(8) unsigned char bytes[sizeof(IntrusivePtr) + kGuardSize];
};

template <typename TValue>
TValue Load(const Storage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
}

bool GuardIntact(const Storage& storage) {
    for (std::size_t index = kHeaderSize; index < sizeof(storage.bytes); ++index) {
        if (storage.bytes[index] != kFill) return false;
    }
    return true;
}

void TestEmptyHeaderHasNoValue() {
    Storage storage;
    IntrusivePtrStorage value;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));
    std::memset(value.bytes, kFill, sizeof(value.bytes));

    _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseC1Ev(reinterpret_cast<CommonResponseHeaderBase*>(storage.bytes));
    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(Load<void*>(storage, kHeadersOffset) == nullptr);
    Require(GuardIntact(storage));

    Require(_ZN3sce2Np9CppWebApi6Common18ResponseHeaderBase14getHeaderValueEPKcRNS2_12IntrusivePtrINS2_6StringEEE(
                reinterpret_cast<const CommonResponseHeaderBase*>(storage.bytes), "Content-Type",
                reinterpret_cast<IntrusivePtr*>(value.bytes)) == kBodyMissing);
    Require(value.bytes[0] == kFill && value.bytes[sizeof(IntrusivePtr)] == kFill);

    _ZN3sce2Np9CppWebApi6Common18ResponseHeaderBaseD1Ev(reinterpret_cast<CommonResponseHeaderBase*>(storage.bytes));
    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(Load<void*>(storage, kHeadersOffset) == nullptr);
    Require(GuardIntact(storage));
}

}

int main() {
    TestEmptyHeaderHasNoValue();
    return 0;
}
