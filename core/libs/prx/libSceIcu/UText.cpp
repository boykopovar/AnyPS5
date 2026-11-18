#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint32_t UtextMagic = 0x345ad82c;
constexpr std::int32_t UtextHeapAllocated = 1;
constexpr std::int32_t UtextExtraHeapAllocated = 2;
constexpr std::int32_t UtextOpen = 4;
constexpr std::size_t MagicOffset = 0;
constexpr std::size_t FlagsOffset = 4;
constexpr std::size_t ExtraSizeOffset = 24;
constexpr std::size_t FunctionsOffset = 56;
constexpr std::size_t ExtraOffset = 64;
constexpr std::size_t CloseOffset = 80;

using CloseFunction = void (APS5_VABI*)(UText*);

template <typename T>
T Field(const UText* text, std::size_t offset) {
    T value{};
    std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(text) + offset, sizeof(value));
    return value;
}

template <typename T>
void SetField(UText* text, std::size_t offset, T value) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(text) + offset, &value, sizeof(value));
}

}

extern "C" {

UText* APS5_VABI utext_close(UText* ut) {
    if (!ut) return nullptr;
    if (Field<std::uint32_t>(ut, MagicOffset) != UtextMagic || (Field<std::int32_t>(ut, FlagsOffset) & UtextOpen) == 0) return ut;
    const auto* functions = Field<const std::uint8_t*>(ut, FunctionsOffset);
    CloseFunction close = nullptr;
    std::memcpy(&close, functions + CloseOffset, sizeof(close));
    if (close != nullptr) close(ut);
    std::int32_t flags = Field<std::int32_t>(ut, FlagsOffset) & ~UtextOpen;
    SetField<std::int32_t>(ut, FlagsOffset, flags);
    if ((flags & UtextExtraHeapAllocated) != 0) {
        IcuReleaseMemory_nid_no_patch(Field<void*>(ut, ExtraOffset));
        SetField<void*>(ut, ExtraOffset, nullptr);
        flags = Field<std::int32_t>(ut, FlagsOffset) & ~UtextExtraHeapAllocated;
        SetField<std::int32_t>(ut, FlagsOffset, flags);
        SetField<std::int32_t>(ut, ExtraSizeOffset, 0);
    }
    SetField<const void*>(ut, FunctionsOffset, nullptr);
    if ((flags & UtextHeapAllocated) != 0) {
        SetField<std::uint32_t>(ut, MagicOffset, 0);
        IcuReleaseMemory_nid_no_patch(ut);
        return nullptr;
    }
    return ut;
}

}
