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

constexpr std::size_t kBinarySize = 0x20;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kReferenceCountOffset = 0x00;
constexpr std::size_t kBufferOffset = 0x08;
constexpr std::size_t kDataSizeOffset = 0x10;
constexpr std::size_t kLibContextOffset = 0x18;
constexpr int kInvalidArgument = static_cast<int>(0x80553503);
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kBinarySize + kGuardSize];
};

template <typename TValue>
TValue Load(const Storage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
}

bool GuardIntact(const Storage& storage) {
    for (std::size_t index = kBinarySize; index < sizeof(storage.bytes); ++index) {
        if (storage.bytes[index] != kFill) return false;
    }
    return true;
}

void TestConstructedBinaryIsEmpty() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));
    _ZN3sce2Np9CppWebApi6Common6BinaryC1EPNS2_10LibContextE(reinterpret_cast<CommonBinary*>(storage.bytes), &libContext);
    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(Load<void*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 0);
    Require(Load<void*>(storage, kLibContextOffset) == &libContext);
    Require(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common6Binary4sizeEv(reinterpret_cast<const CommonBinary*>(storage.bytes)) == 0);
    Require(GuardIntact(storage));
    _ZN3sce2Np9CppWebApi6Common6BinaryD1Ev(reinterpret_cast<CommonBinary*>(storage.bytes));
    Require(GuardIntact(storage));
}

void TestSetAndAppend() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));
    auto* binary = reinterpret_cast<CommonBinary*>(storage.bytes);
    _ZN3sce2Np9CppWebApi6Common6BinaryC2EPNS2_10LibContextE(binary, &libContext);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary9setBinaryEPKvm(binary, "abc", 3) == 0);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 3);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "abc", 3) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common6Binary4sizeEv(reinterpret_cast<const CommonBinary*>(storage.bytes)) == 3);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary12appendBinaryEPKvm(binary, "de", 2) == 0);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 5);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "abcde", 5) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary12appendBinaryEPKvm(binary, nullptr, 1) == kInvalidArgument);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 5);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "abcde", 5) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary9setBinaryEPKvm(binary, nullptr, 1) == kInvalidArgument);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 5);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "abcde", 5) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary9setBinaryEPKvm(binary, nullptr, 0) == 0);
    Require(_ZN3sce2Np9CppWebApi6Common6Binary12appendBinaryEPKvm(binary, "xy", 2) == 0);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 2);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "xy", 2) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary9setBinaryEPKvm(binary, _ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), 2) == 0);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 2);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "xy", 2) == 0);

    Require(_ZN3sce2Np9CppWebApi6Common6Binary12appendBinaryEPKvm(binary, _ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), 2) == 0);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 4);
    Require(std::memcmp(_ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(reinterpret_cast<const CommonBinary*>(storage.bytes)), "xyxy", 4) == 0);

    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(_ZN3sce2Np9CppWebApi6Common6Binary21intrusive_ptr_add_refEPS3_(binary) == 1);
    Require(_ZN3sce2Np9CppWebApi6Common6Binary21intrusive_ptr_add_refEPS3_(binary) == 2);
    Require(_ZN3sce2Np9CppWebApi6Common6Binary21intrusive_ptr_sub_refEPS3_(binary) == 2);
    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 1);

    _ZN3sce2Np9CppWebApi6Common6Binary5clearEv(binary);
    Require(Load<void*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 0);
    Require(GuardIntact(storage));

    _ZN3sce2Np9CppWebApi6Common6BinaryD2Ev(binary);
    Require(Load<void*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kDataSizeOffset) == 0);
    Require(GuardIntact(storage));
}

}

int main() {
    TestConstructedBinaryIsEmpty();
    TestSetAndAppend();
    return 0;
}
