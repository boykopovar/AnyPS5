#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UtextClose {


static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::size_t StructSize = 144;

int closed = 0;
UText* closedText = nullptr;

void APS5_VABI CountClose(UText* text) {
    ++closed;
    closedText = text;
}

void* released[4]{};
int releases = 0;
UText* reenter = nullptr;

void* APS5_VABI NoAllocate(const void*, std::size_t) {
    return nullptr;
}

void* APS5_VABI NoReallocate(const void*, void*, std::size_t) {
    return nullptr;
}

void APS5_VABI RecordRelease(const void*, void* memory) {
    if (releases < 4) released[releases] = memory;
    ++releases;
    if (reenter != nullptr) {
        UText* text = reenter;
        reenter = nullptr;
        Require(utext_close(text) == text);
    }
}

template <typename T>
T Read(const void* base, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(value));
    return value;
}

template <typename T>
void Write(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

void Run() {
    UText* text = nullptr;
    Require(utext_close(nullptr) == nullptr);
    Require(utext_close(text) == nullptr);
    Require(text == nullptr);

    alignas(8) std::uint8_t dummy[16]{};
    Require(utext_close(reinterpret_cast<UText*>(dummy)) == reinterpret_cast<UText*>(dummy));
    for (const auto byte : dummy) Require(byte == 0);

    alignas(8) std::uint8_t closedOne[StructSize]{};
    Write(closedOne, 0, Magic);
    Write(closedOne, 4, HeapAllocated | ExtraHeapAllocated);
    std::uint8_t before[StructSize];
    std::memcpy(before, closedOne, sizeof(before));
    Require(utext_close(reinterpret_cast<UText*>(closedOne)) == reinterpret_cast<UText*>(closedOne));
    Require(std::memcmp(before, closedOne, sizeof(before)) == 0);

    int marker = 0;
    std::int32_t status = 0;
    u_cleanup_nid_postfix();
    u_setMemoryFunctions_nid_postfix(&marker, NoAllocate, NoReallocate, RecordRelease, &status);
    Require(status == 0);
    alignas(8) std::uint8_t extra[32]{};
    alignas(8) std::uint8_t functions[96]{};
    auto* close = &CountClose;
    std::memcpy(functions + 80, &close, sizeof(close));
    alignas(8) std::uint8_t open[StructSize]{};
    Write(open, 0, Magic);
    Write(open, 4, Open | ExtraHeapAllocated);
    Write<std::int32_t>(open, 24, 32);
    Write<const void*>(open, 56, functions);
    Write<void*>(open, 64, extra);
    reenter = reinterpret_cast<UText*>(open);
    Require(utext_close(reinterpret_cast<UText*>(open)) == reinterpret_cast<UText*>(open));
    Require(reenter == nullptr && closed == 1 && closedText == reinterpret_cast<UText*>(open));
    Require(Read<std::uint32_t>(open, 0) == Magic && Read<std::int32_t>(open, 4) == 0);
    Require(Read<std::int32_t>(open, 24) == 0 && Read<const void*>(open, 56) == nullptr && Read<void*>(open, 64) == nullptr);
    Require(releases == 1 && released[0] == extra);
    Require(utext_close(reinterpret_cast<UText*>(open)) == reinterpret_cast<UText*>(open) && closed == 1 && releases == 1);

    alignas(8) std::uint8_t heap[StructSize]{};
    alignas(8) std::uint8_t noClose[96]{};
    Write(heap, 0, Magic);
    Write(heap, 4, Open | HeapAllocated);
    Write<const void*>(heap, 56, noClose);
    Require(utext_close(reinterpret_cast<UText*>(heap)) == nullptr && closed == 1);
    Require(releases == 2 && released[1] == heap && Read<std::uint32_t>(heap, 0) == 0 && Read<const void*>(heap, 56) == nullptr);
    u_cleanup_nid_postfix();
}

} // namespace UtextClose

void CheckIcuUText() {
    UtextClose::Run();
}
