#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <algorithm>
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


struct alignas(16) ActivityModel {
    std::array<std::byte, 0x10> header;
    std::array<std::byte, 0x20> activityId;
    std::array<std::byte, 0x20> name;
    bool hidden;
    std::array<std::byte, 7> reserved;
    std::array<std::byte, 0x20> matchId;
    bool matchIdSet;
    std::array<std::byte, 7> guard;
};

static_assert(offsetof(ActivityModel, activityId) == 0x10);
static_assert(offsetof(ActivityModel, name) == 0x30);
static_assert(offsetof(ActivityModel, hidden) == 0x50);
static_assert(offsetof(ActivityModel, matchId) == 0x58);
static_assert(offsetof(ActivityModel, matchIdSet) == 0x78);
static_assert(sizeof(ActivityModel) == 0x80);

void TestActivityModelAccessors() {
    std::array<ActivityModel, 4> activities{};
    const std::array getters{
        &_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity13getActivityIdEv,
        &_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity7getNameEv,
        &_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity10getMatchIdEv,
    };
    for (std::size_t index = 0; index < activities.size(); ++index) {
        auto& activity = activities[index];
        std::memset(&activity, 0xa5, sizeof(activity));
        activity.hidden = (index & 1u) != 0;
        activity.matchIdSet = (index & 2u) != 0;
        const auto before = activities;
        const std::array<std::byte*, 3> members{activity.activityId.data(), activity.name.data(), activity.matchId.data()};
        for (std::size_t field = 0; field < getters.size(); ++field) {
            const auto* reference = getters[field](&activity);
            Require(reference == members[field]);
        }
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity9getHiddenEv(&activity) == activity.hidden);
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity12matchIdIsSetEv(&activity) == activity.matchIdSet);
        Require(std::memcmp(activities.data(), before.data(), sizeof(activities)) == 0);
        for (std::size_t field = 0; field < getters.size(); ++field) {
            const auto* reference = getters[field](&activity);
            const auto value = std::byte{static_cast<unsigned char>(0x31u + index * 3u + field)};
            std::fill(members[field], members[field] + 0x20, value);
            const auto updated = activities;
            Require(std::memcmp(reference, members[field], 0x20) == 0);
            Require(getters[field](&activity) == reference);
            Require(std::memcmp(activities.data(), updated.data(), sizeof(activities)) == 0);
        }
        activity.hidden = !activity.hidden;
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity9getHiddenEv(&activity) == activity.hidden);
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity12matchIdIsSetEv(&activity) == activity.matchIdSet);
        activity.matchIdSet = !activity.matchIdSet;
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity9getHiddenEv(&activity) == activity.hidden);
        Require(_ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity12matchIdIsSetEv(&activity) == activity.matchIdSet);
    }
}

void TestRequestIsSignedOut() {
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi17getUserActivitiesEiRKNS5_28ParameterToGetUserActivitiesERNS1_6Common11TransactionINS9_12IntrusivePtrINS4_26GetUsersActivitiesResponseEEENSB_INS9_18ResponseHeaderBaseEEEEE(
                0, nullptr, nullptr) == kNotSignedIn);
}

void TestGetUsersCopiesTheVectorHandle() {
    alignas(8) std::array<std::uint8_t, 0x50> response;
    response.fill(0xa5);
    alignas(8) std::array<std::uint8_t, 0x40> vector;
    vector.fill(0);
    const std::uint32_t before = 7;
    std::memcpy(vector.data() + 0x30, &before, sizeof(before));
    IntrusivePtr handle{};
    handle.object = vector.data();
    handle.deleter = nullptr;
    handle.libContext = reinterpret_cast<void*>(0x1234);
    std::memcpy(response.data() + 0x30, &handle, sizeof(handle));
    IntrusivePtr out{};
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities26GetUsersActivitiesResponse8getUsersEv(&out, response.data()) == &out);
    Require(out.object == vector.data());
    Require(out.deleter == nullptr);
    Require(out.libContext == reinterpret_cast<void*>(0x1234));
    std::uint32_t after = 0;
    std::memcpy(&after, vector.data() + 0x30, sizeof(after));
    Require(after == before + 1);
    for (std::size_t index = 0; index < 0x30; ++index) Require(response[index] == 0xa5);
    for (std::size_t index = 0x48; index < response.size(); ++index) Require(response[index] == 0xa5);

    alignas(8) std::array<std::uint8_t, 0x50> empty{};
    IntrusivePtr none{};
    none.object = reinterpret_cast<void*>(0x1);
    none.deleter = nullptr;
    none.libContext = nullptr;
    Require(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities26GetUsersActivitiesResponse8getUsersEv(&none, empty.data()) == &none);
    Require(none.object == nullptr);
    Require(none.deleter == nullptr);
}

void TestSharedInstantiations() {
    constexpr int kTransactionNotStarted = static_cast<int>(0x80553508);
    alignas(8) unsigned char transaction[0x10];
    std::memset(transaction, kFill, sizeof(transaction));
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(transaction) == 0);
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(transaction) == 0);
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() == 0);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() == kTransactionNotStarted);
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() == kNotSignedIn);

    IntrusivePtr empty{};
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEptEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_10Activities2V114UserActivities26GetUsersActivitiesResponseEEptEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEptEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_10Activities2V114UserActivities14UserActivitiesEEEEEEdeEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_10Activities2V114UserActivities14UserActivitiesEEEEEEptEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_10Activities2V114UserActivities8ActivityEEEEEEdeEv(&empty) == nullptr);
    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEE4sizeEv(&empty) == 0);

    alignas(8) unsigned char elements[2 * 0x18] = {};
    CommonIterator cursor{0, elements};
    Require(_ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEEppEv(&cursor) == &cursor && cursor.element == elements + 0x18);
    Require(_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEEdeEv(&cursor) == elements + 0x18);
    CommonIterator other{0, elements};
    Require(_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEEneERKSA_(&cursor, &other));
    CommonIterator activityCursor{0, elements};
    Require(_ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEEppEv(&activityCursor) == &activityCursor && activityCursor.element == elements + 0x18);
    Require(_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEEdeEv(&activityCursor) == elements + 0x18);
    CommonIterator activityOther{0, elements};
    Require(_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEEneERKSA_(&activityCursor, &activityOther));

    CommonIterator first{0x7f, elements};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEE5beginEv(&first, &empty) == &first && first.referenceCount == 0 && first.element == nullptr);
    CommonIterator last{0x7f, elements};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEE3endEv(&last, &empty) == &last && last.referenceCount == 0 && last.element == nullptr);
    CommonIterator activityFirst{0x7f, elements};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEE5beginEv(&activityFirst, &empty) == &activityFirst && activityFirst.element == nullptr);
    CommonIterator activityLast{0x7f, elements};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEE3endEv(&activityLast, &empty) == &activityLast && activityLast.element == nullptr);

    CommonConstIterator constCursor{0, elements};
    Require(_ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEED2Ev(&constCursor) == 0);
    CommonConstIterator activityConstCursor{0, elements};
    Require(_ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEED2Ev(&activityConstCursor) == 0);

    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_10Activities2V114UserActivities14UserActivitiesEEEEEED1Ev(&empty);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_10Activities2V114UserActivities8ActivityEEEEEED1Ev(&empty);
    Require(empty.object == nullptr && empty.deleter == nullptr);
}

}

int main() {
    TestParameterLifecycle();
    TestRequestIsSignedOut();
    TestActivityModelAccessors();
    TestGetUsersCopiesTheVectorHandle();
    TestSharedInstantiations();
    return 0;
}
