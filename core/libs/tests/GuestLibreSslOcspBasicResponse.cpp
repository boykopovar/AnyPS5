#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>

struct alignas(8) Guest {
    std::array<std::byte, 32> basic;
    std::array<std::byte, 40> data;
    std::array<std::byte, 32> stack;
    std::array<std::byte, 40> decoyData;
    std::array<std::byte, 32> decoyStack;
    std::array<void*, 4> entries;
    std::array<void*, 8> decoyEntries;
};

static constexpr std::int32_t MinIndex = std::numeric_limits<std::int32_t>::min();
static constexpr std::int32_t MaxIndex = std::numeric_limits<std::int32_t>::max();
static constexpr std::int32_t Indices[] {0, 1, 2, 3, -1, MinIndex, MaxIndex};

static int first;
static int third;
static int beyond;

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, message);
}

template<std::size_t TSize, typename TValue> static void Store(std::array<std::byte, TSize>& object, std::size_t offset, TValue value) {
    Require(offset + sizeof(value) <= TSize, "store stays inside the object");
    std::memcpy(object.data() + offset, &value, sizeof(value));
}

static void Build(Guest& guest, std::int32_t count) {
    std::memset(&guest, 0xa5, sizeof(guest));
    guest.entries = {&first, nullptr, &third, &beyond};
    for (auto& entry : guest.decoyEntries) entry = &entry;
    Store(guest.decoyStack, 0, std::int32_t{7});
    Store(guest.decoyStack, 8, guest.decoyEntries.data());
    for (std::size_t offset = 0; offset < 40; offset += 8) Store(guest.decoyData, offset, guest.decoyStack.data());
    for (std::size_t offset = 8; offset < 32; offset += 8) Store(guest.basic, offset, guest.decoyData.data());
    Store(guest.basic, 0, guest.data.data());
    for (std::size_t offset = 0; offset < 40; offset += 8) Store(guest.data, offset, guest.decoyStack.data());
    Store(guest.data, 24, guest.stack.data());
    Store(guest.stack, 0, count);
    Store(guest.stack, 8, guest.entries.data());
    Store(guest.stack, 16, std::int32_t{5});
    Store(guest.stack, 20, std::int32_t{2});
    Store(guest.stack, 24, guest.decoyEntries.data());
}

static int Count(Guest& guest) {
    return OCSP_resp_count_nid_postfix(guest.basic.data());
}

static void* Get(Guest& guest, std::int32_t index) {
    return OCSP_resp_get0_nid_postfix(guest.basic.data(), index);
}

int main() {
    Require(OCSP_resp_count_nid_postfix(nullptr) == -1, "a null basic response counts -1");
    for (const auto index : Indices)
        Require(OCSP_resp_get0_nid_postfix(nullptr, index) == nullptr, "a null basic response has no entries");

    Guest guest;
    Build(guest, 3);
    const Guest snapshot = guest;
    Require(Count(guest) == 3, "the count is the 32-bit size of the stack at offset 24 of the response data");
    Require(Get(guest, 0) == &first, "index 0 returns the first entry");
    Require(Get(guest, 1) == nullptr, "a null entry is returned as stored");
    Require(Get(guest, 2) == &third, "the last index returns the last entry");
    Require(Get(guest, 3) == nullptr, "an index equal to the count returns null");
    Require(Get(guest, -1) == nullptr, "a negative index returns null");
    Require(Get(guest, MinIndex) == nullptr, "the minimum index returns null");
    Require(Get(guest, MaxIndex) == nullptr, "the maximum index returns null");
    Require(std::memcmp(&guest, &snapshot, sizeof(guest)) == 0, "queries do not write the objects");

    Store(guest.data, 24, static_cast<const void*>(nullptr));
    Require(Count(guest) == -1, "a null responses stack counts -1");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "a null responses stack has no entries");

    Build(guest, 0);
    Require(Count(guest) == 0, "an empty stack counts 0");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "an empty stack has no entries");

    Build(guest, -2);
    Require(Count(guest) == -2, "a negative stack size is returned unchanged");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "a negative stack size has no entries");

    Build(guest, MaxIndex);
    Require(Count(guest) == MaxIndex, "the maximum stack size is returned unchanged");
    Require(Get(guest, 0) == &first, "index 0 is below the maximum stack size");
    Require(Get(guest, 3) == &beyond, "the stack size alone bounds the index");
    Require(Get(guest, MaxIndex) == nullptr, "an index equal to the maximum stack size returns null");
    Require(Get(guest, -1) == nullptr, "a negative index returns null for the maximum stack size");
    Require(Get(guest, MinIndex) == nullptr, "the minimum index returns null for the maximum stack size");

    Build(guest, 3);
    Store(guest.stack, 8, static_cast<const void*>(nullptr));
    Require(Count(guest) == 3, "the count does not read the entry array");
    Require(Get(guest, 3) == nullptr, "an index equal to the count returns null before the entry array is read");
    Require(Get(guest, -1) == nullptr, "a negative index returns null before the entry array is read");
    RequireThrows([&] { Get(guest, 0); }, "a null entry array with index 0 throws");
    RequireThrows([&] { Get(guest, 2); }, "a null entry array with the last index throws");

    Build(guest, 3);
    Store(guest.basic, 0, static_cast<const void*>(nullptr));
    RequireThrows([&] { Count(guest); }, "a null response data pointer throws in the count");
    for (const auto index : Indices)
        RequireThrows([&] { Get(guest, index); }, "a null response data pointer throws for every index");
    return 0;
}
