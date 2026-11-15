#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::JscAbi::FreeCell;
using Jsc::JscAbi::FreeList;
using InitializeListFunction = void (APS5_VABI*)(FreeList*, FreeCell*, std::uintptr_t, unsigned);
using InitializeBumpFunction = void (APS5_VABI*)(FreeList*, char*, unsigned);

static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj), InitializeListFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeBumpEPcj), InitializeBumpFunction>);

constexpr unsigned char kPoison = 0xA5;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeList check failed at line %d\n", line);
        std::abort();
    }
}

struct alignas(8) ListBytes {
    unsigned char bytes[sizeof(FreeList)];

    ListBytes() { std::memset(bytes, kPoison, sizeof(bytes)); }
    FreeList* List() { return reinterpret_cast<FreeList*>(bytes); }
    template <typename T>
    T Read(std::size_t offset) const {
        T value{};
        std::memcpy(&value, bytes + offset, sizeof(T));
        return value;
    }
};

std::uint32_t PoisonWord() {
    std::uint32_t word;
    std::memset(&word, kPoison, sizeof(word));
    return word;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void InitializeListScramblesHeadWithSecret() {
    alignas(16) unsigned char cells[64] = {};
    auto* head = reinterpret_cast<FreeCell*>(cells + 16);
    const std::uintptr_t secret = static_cast<std::uintptr_t>(0x5DEECE66D1234567ull);
    ListBytes list;
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(list.List(), head, secret, 4096);
    Check(list.Read<std::uintptr_t>(0) == (reinterpret_cast<std::uintptr_t>(head) ^ secret), __LINE__);
    Check((list.Read<std::uintptr_t>(0) ^ list.Read<std::uintptr_t>(8)) == reinterpret_cast<std::uintptr_t>(head), __LINE__);
    Check(list.Read<std::uintptr_t>(8) == secret, __LINE__);
    Check(list.Read<char*>(16) == nullptr, __LINE__);
    Check(list.Read<std::uint32_t>(24) == 0, __LINE__);
    Check(list.Read<std::uint32_t>(28) == 4096, __LINE__);
    Check(list.Read<std::uint32_t>(32) == PoisonWord(), __LINE__);
}

void InitializeListWithNullHeadStoresTheSecret() {
    ListBytes list;
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(list.List(), nullptr, 0x77, 0xFFFFFFFFu);
    Check(list.Read<std::uintptr_t>(0) == 0x77, __LINE__);
    Check(list.Read<std::uintptr_t>(8) == 0x77, __LINE__);
    Check(list.Read<char*>(16) == nullptr, __LINE__);
    Check(list.Read<std::uint32_t>(24) == 0, __LINE__);
    Check(list.Read<std::uint32_t>(28) == 0xFFFFFFFFu, __LINE__);
    Check(list.Read<std::uint32_t>(32) == PoisonWord(), __LINE__);
}

void InitializeBumpClearsTheListAndKeepsTheCellSize() {
    char payload[256] = {};
    char* payloadEnd = payload + sizeof(payload);
    ListBytes list;
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(list.List(), reinterpret_cast<FreeCell*>(payload), 0x1234, 64);
    _ZN3JSC8FreeList14initializeBumpEPcj(list.List(), payloadEnd, 192);
    Check(list.Read<std::uintptr_t>(0) == 0, __LINE__);
    Check(list.Read<std::uintptr_t>(8) == 0, __LINE__);
    Check(list.Read<char*>(16) == payloadEnd, __LINE__);
    Check(list.Read<std::uint32_t>(24) == 192, __LINE__);
    Check(list.Read<std::uint32_t>(28) == 192, __LINE__);
    Check(list.Read<std::uint32_t>(32) == PoisonWord(), __LINE__);
}

void InitializeListAfterBumpClearsTheBumpRange() {
    char payload[128] = {};
    alignas(16) unsigned char cells[32] = {};
    ListBytes list;
    _ZN3JSC8FreeList14initializeBumpEPcj(list.List(), payload + sizeof(payload), 128);
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(list.List(), reinterpret_cast<FreeCell*>(cells), 0, 32);
    Check(list.Read<std::uintptr_t>(0) == reinterpret_cast<std::uintptr_t>(cells), __LINE__);
    Check(list.Read<std::uintptr_t>(8) == 0, __LINE__);
    Check(list.Read<char*>(16) == nullptr, __LINE__);
    Check(list.Read<std::uint32_t>(24) == 0, __LINE__);
    Check(list.Read<std::uint32_t>(28) == 32, __LINE__);
    Check(list.Read<std::uint32_t>(32) == PoisonWord(), __LINE__);
}

void InitializeBumpWithEmptyRange() {
    ListBytes list;
    _ZN3JSC8FreeList14initializeBumpEPcj(list.List(), nullptr, 0);
    for (std::size_t offset = 0; offset < 32; offset += 4) Check(list.Read<std::uint32_t>(offset) == 0, __LINE__);
    Check(list.Read<std::uint32_t>(32) == PoisonWord(), __LINE__);
}

void NullListThrows() {
    char payload[16] = {};
    Check(Throws([] { _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(nullptr, nullptr, 0, 0); }), __LINE__);
    Check(Throws([&] { _ZN3JSC8FreeList14initializeBumpEPcj(nullptr, payload + sizeof(payload), 16); }), __LINE__);
}

} // namespace

int main() {
    InitializeListScramblesHeadWithSecret();
    InitializeListWithNullHeadStoresTheSecret();
    InitializeBumpClearsTheListAndKeepsTheCellSize();
    InitializeListAfterBumpClearsTheBumpRange();
    InitializeBumpWithEmptyRange();
    NullListThrows();
    return 0;
}
