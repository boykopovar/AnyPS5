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

constexpr std::size_t kParameterSize = 0x10;
constexpr std::size_t kGuardSize = 0x10;
constexpr std::size_t kBaseOffset = 0x00;
constexpr std::size_t kStateOffset = 0x08;
constexpr int kInvalidArgument = static_cast<int>(0x80553503);
constexpr int kAlreadyInitialized = static_cast<int>(0x8055350f);
constexpr int kNotInitialized = static_cast<int>(0x80553510);
constexpr int kNotSignedIn = static_cast<int>(0x80552907);
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kParameterSize + kGuardSize];
};

template <typename TValue>
TValue Load(const Storage& storage, std::size_t offset) {
    TValue value;
    std::memcpy(&value, storage.bytes + offset, sizeof(value));
    return value;
}

bool GuardIntact(const Storage& storage) {
    for (std::size_t index = kParameterSize; index < sizeof(storage.bytes); ++index) {
        if (storage.bytes[index] != kFill) return false;
    }
    return true;
}

void TestParameterLifecycle() {
    std::uint64_t libContext = 1;
    Storage storage;
    std::memset(storage.bytes, kFill, sizeof(storage.bytes));

    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesC1Ev(storage.bytes) == 0);
    Require(Load<void*>(storage, kBaseOffset) == nullptr);
    Require(Load<void*>(storage, kStateOffset) == nullptr);
    Require(GuardIntact(storage));

    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities10initializeEPNS1_6Common10LibContextENS7_12IntrusivePtrINS7_6VectorINS7_6StringEEEEE(
                storage.bytes, nullptr, nullptr) == kInvalidArgument);
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities10initializeEPNS1_6Common10LibContextENS7_12IntrusivePtrINS7_6VectorINS7_6StringEEEEE(
                storage.bytes, &libContext, nullptr) == 0);
    Require(Load<void*>(storage, kBaseOffset) == &libContext);
    Require(Load<void*>(storage, kStateOffset) != nullptr);
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities10initializeEPNS1_6Common10LibContextENS7_12IntrusivePtrINS7_6VectorINS7_6StringEEEEE(
                storage.bytes, &libContext, nullptr) == kAlreadyInitialized);

    _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities8setlimitEi(storage.bytes, 12);
    _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities17setnpServiceLabelEi(storage.bytes, 3);
    Require(GuardIntact(storage));

    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities9terminateEv(storage.bytes) == 0);
    Require(Load<void*>(storage, kBaseOffset) == nullptr);
    Require(Load<void*>(storage, kStateOffset) == nullptr);
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities9terminateEv(storage.bytes) == kNotInitialized);
    Require(GuardIntact(storage));

    _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesD1Ev(storage.bytes);
    Require(GuardIntact(storage));

    Storage copyStorage;
    std::memset(copyStorage.bytes, kFill, sizeof(copyStorage.bytes));
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesC2Ev(copyStorage.bytes) == 0);
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesD2Ev(copyStorage.bytes) == 0);
    Require(GuardIntact(copyStorage));
}

void TestRequestIsSignedOut() {
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi17getUserActivitiesEiRKNS5_28ParameterToGetUserActivitiesERNS1_6Common11TransactionINS9_12IntrusivePtrINS4_26GetUsersActivitiesResponseEEENSB_INS9_18ResponseHeaderBaseEEEEE(
                0, nullptr, nullptr) == kNotSignedIn);
}

}

int main() {
    TestParameterLifecycle();
    TestRequestIsSignedOut();
    return 0;
}
