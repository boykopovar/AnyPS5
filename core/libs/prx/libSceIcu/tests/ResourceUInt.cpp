#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

constexpr std::size_t ResourceOffset = 172;
constexpr std::uint32_t IntegerType = 7;
constexpr std::uint32_t ValueMask = 0x0fffffff;
constexpr std::uint32_t Failure = UINT32_MAX;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t ResourceTypeMismatch = 17;

static_assert(std::is_same_v<decltype(&ures_getUInt_nid_postfix), std::uint32_t (APS5_VABI*)(const UResourceBundle*, std::int32_t*)>);

struct GuardedBundle {
    std::array<std::uint8_t, 32> before;
    std::array<std::uint8_t, 207> storage;
    std::array<std::uint8_t, 32> after;
};

struct GuardedStatus {
    std::array<std::uint8_t, 16> before;
    std::int32_t value;
    std::array<std::uint8_t, 16> after;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU resource uint: %s\n", message);
    std::abort();
}

void Fill(GuardedBundle& bundle, std::uint8_t value) {
    bundle.before.fill(value);
    bundle.storage.fill(value);
    bundle.after.fill(value);
}

void SetResource(GuardedBundle& bundle, std::size_t shift, std::uint32_t resource) {
    std::memcpy(bundle.storage.data() + shift + ResourceOffset, &resource, sizeof(resource));
}

const UResourceBundle* Bundle(const GuardedBundle& bundle, std::size_t shift) {
    return reinterpret_cast<const UResourceBundle*>(bundle.storage.data() + shift);
}

void RequireBundleUnchanged(const GuardedBundle& actual, const GuardedBundle& expected) {
    Require(actual.before == expected.before, "leading bundle guard changed");
    Require(actual.storage == expected.storage, "bundle storage changed");
    Require(actual.after == expected.after, "trailing bundle guard changed");
}

void RequireStatusGuards(const GuardedStatus& status, std::uint8_t value) {
    for (const auto byte : status.before) Require(byte == value, "leading status guard changed");
    for (const auto byte : status.after) Require(byte == value, "trailing status guard changed");
}

std::uint32_t Next(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void CheckNoAccessEarlyReturns() {
    const auto* invalid = reinterpret_cast<const UResourceBundle*>(std::uintptr_t{1});
    errno = 73;
    Require(ures_getUInt_nid_postfix(invalid, nullptr) == Failure, "null status result");
    Require(errno == 73, "null status errno");

    for (const auto incoming : {std::int32_t{1}, std::int32_t{17}, std::numeric_limits<std::int32_t>::max()}) {
        GuardedStatus status;
        status.before.fill(0x5a);
        status.value = incoming;
        status.after.fill(0x5a);
        errno = 73;
        Require(ures_getUInt_nid_postfix(invalid, &status.value) == Failure, "prefailed status result");
        Require(status.value == incoming, "prefailed status changed");
        RequireStatusGuards(status, 0x5a);
        Require(errno == 73, "prefailed status errno");
    }
}

void CheckNullBundle() {
    for (const auto incoming : {std::int32_t{0}, std::int32_t{-1}, std::int32_t{-124}}) {
        GuardedStatus status;
        status.before.fill(0xa5);
        status.value = incoming;
        status.after.fill(0xa5);
        errno = 73;
        Require(ures_getUInt_nid_postfix(nullptr, &status.value) == Failure, "null bundle result");
        Require(status.value == IllegalArgument, "null bundle status");
        RequireStatusGuards(status, 0xa5);
        Require(errno == 73, "null bundle errno");
    }
}

void CheckAllTypes() {
    for (std::uint32_t type = 0; type != 16; ++type) {
        GuardedBundle bundle;
        Fill(bundle, 0x5a);
        const std::uint32_t payload = 0x00c0ffee;
        SetResource(bundle, 0, (type << 28) | payload);
        const auto expected = bundle;
        GuardedStatus status;
        status.before.fill(0xa5);
        status.value = 0;
        status.after.fill(0xa5);
        errno = 73;
        const auto result = ures_getUInt_nid_postfix(Bundle(bundle, 0), &status.value);
        Require(result == (type == IntegerType ? payload : Failure), "resource type result");
        Require(status.value == (type == IntegerType ? 0 : ResourceTypeMismatch), "resource type status");
        RequireBundleUnchanged(bundle, expected);
        RequireStatusGuards(status, 0xa5);
        Require(errno == 73, "resource type errno");
    }
}

void CheckPayload(std::uint32_t payload, std::int32_t incoming, std::size_t shift) {
    GuardedBundle bundle;
    Fill(bundle, 0x3c);
    SetResource(bundle, shift, (IntegerType << 28) | payload);
    const auto expected = bundle;
    GuardedStatus status;
    status.before.fill(0xc3);
    status.value = incoming;
    status.after.fill(0xc3);
    errno = 73;
    Require(ures_getUInt_nid_postfix(Bundle(bundle, shift), &status.value) == payload, "integer payload result");
    Require(status.value == incoming, "success status changed");
    RequireBundleUnchanged(bundle, expected);
    RequireStatusGuards(status, 0xc3);
    Require(errno == 73, "integer payload errno");
}

void CheckPayloadsAndAlignment() {
    constexpr std::array<std::uint32_t, 8> boundaries{0, 1, 2, 0x07fffffe, 0x07ffffff, 0x08000000, 0x0ffffffe, ValueMask};
    constexpr std::array<std::int32_t, 4> passingStatuses{0, -1, -124, -128};
    for (std::size_t shift = 0; shift != 8; ++shift) {
        for (const auto payload : boundaries) {
            for (const auto status : passingStatuses) CheckPayload(payload, status, shift);
        }
    }

    std::uint32_t state = 0x68c0ffee;
    for (std::size_t index = 0; index != 1024; ++index) {
        const auto payload = Next(state) & ValueMask;
        CheckPayload(payload, passingStatuses[index % passingStatuses.size()], index % 8);
    }
}

void CheckStatusAlignment() {
    GuardedBundle bundle;
    Fill(bundle, 0x5a);
    SetResource(bundle, 0, (IntegerType << 28) | 0x01234567);
    const auto expectedBundle = bundle;
    for (std::size_t shift = 0; shift != 8; ++shift) {
        std::array<std::uint8_t, 32> statusStorage;
        statusStorage.fill(0xa5);
        constexpr std::int32_t initial = -124;
        std::memcpy(statusStorage.data() + 8 + shift, &initial, sizeof(initial));
        const auto expectedStatus = statusStorage;
        auto* status = reinterpret_cast<std::int32_t*>(statusStorage.data() + 8 + shift);
        errno = 73;
        Require(ures_getUInt_nid_postfix(Bundle(bundle, 0), status) == 0x01234567, "misaligned status result");
        Require(statusStorage == expectedStatus, "misaligned status changed");
        RequireBundleUnchanged(bundle, expectedBundle);
        Require(errno == 73, "misaligned status errno");
    }
}

void CheckMismatchOverwritesWarnings() {
    for (const auto incoming : {std::int32_t{0}, std::int32_t{-1}, std::int32_t{-124}}) {
        GuardedBundle bundle;
        Fill(bundle, 0x96);
        SetResource(bundle, 3, 0xf1234567);
        const auto expected = bundle;
        GuardedStatus status;
        status.before.fill(0x69);
        status.value = incoming;
        status.after.fill(0x69);
        errno = 73;
        Require(ures_getUInt_nid_postfix(Bundle(bundle, 3), &status.value) == Failure, "mismatch result");
        Require(status.value == ResourceTypeMismatch, "mismatch status");
        RequireBundleUnchanged(bundle, expected);
        RequireStatusGuards(status, 0x69);
        Require(errno == 73, "mismatch errno");
    }
}

}

int main() {
    CheckNoAccessEarlyReturns();
    CheckNullBundle();
    CheckAllTypes();
    CheckPayloadsAndAlignment();
    CheckStatusAlignment();
    CheckMismatchOverwritesWarnings();
}
