#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(void* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringD1Ev(void* self);
const char* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(const void* self);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::size_t kStringSize = 0x20;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kReferenceCountOffset = 0x00;
constexpr std::size_t kBufferOffset = 0x08;
constexpr std::size_t kBufferSizeOffset = 0x10;
constexpr std::size_t kLibContextOffset = 0x18;
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kStringSize + kGuardSize];
};

template <typename TValue>
TValue Load(const Storage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
}

template <typename TValue>
void Store(Storage& storage, std::size_t offset, TValue value) {
    std::memcpy(storage.bytes + offset, &value, sizeof(value));
}

bool GuardIntact(const Storage& storage) {
    for (std::size_t i = kStringSize; i < sizeof(storage.bytes); i++) {
        if (storage.bytes[i] != kFill) return false;
    }
    return true;
}

bool DestructorThrows(Storage& storage) {
    try {
        _ZN3sce2Np9CppWebApi6Common6StringD1Ev(storage.bytes);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestConstructedStringIsEmpty() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(storage.bytes, &libContext);

    Require(Load<std::int32_t>(storage, kReferenceCountOffset) == 0);
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(Load<std::uint64_t>(storage, kBufferSizeOffset) == 0);
    Require(Load<void*>(storage, kLibContextOffset) == &libContext);
    Require(GuardIntact(storage));

    const char* text = _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(storage.bytes);
    Require(text != nullptr && text[0] == '\0');

    Storage before = storage;
    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(storage.bytes);
    Require(std::memcmp(before.bytes, storage.bytes, sizeof(storage.bytes)) == 0);
}

void TestNullLibContextIsStored() {
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(storage.bytes, nullptr);

    Require(Load<void*>(storage, kLibContextOffset) == nullptr);
    Require(Load<char*>(storage, kBufferOffset) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(storage.bytes)[0] == '\0');
    _ZN3sce2Np9CppWebApi6Common6StringD1Ev(storage.bytes);
}

void TestBufferIsReturnedAndNotDroppedSilently() {
    std::uint64_t libContext = 1;
    char text[] = "ranking";
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(storage.bytes, &libContext);
    Store<char*>(storage, kBufferOffset, text);
    Store<std::uint64_t>(storage, kBufferSizeOffset, sizeof(text));

    Require(_ZNK3sce2Np9CppWebApi6Common6String5c_strEv(storage.bytes) == text);
    Require(DestructorThrows(storage));
    Require(std::strcmp(text, "ranking") == 0);
}

}

int main() {
    TestConstructedStringIsEmpty();
    TestNullLibContextIsStored();
    TestBufferIsReturnedAndNotDroppedSilently();
}
