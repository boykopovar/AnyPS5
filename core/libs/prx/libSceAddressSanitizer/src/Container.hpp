#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"

namespace AsanRuntime {

inline constexpr std::uintptr_t kGranularity = 8;
inline constexpr std::uint8_t kContainerOverflowMagic = 0xfc;
inline constexpr std::uintptr_t kMaxContainerStorage = 1ull << 40;

inline std::uint8_t* ShadowByte_nid_no_patch(std::uintptr_t address) noexcept {
    return reinterpret_cast<std::uint8_t*>(ShadowBase + (address >> 3));
}

inline std::uintptr_t RoundDown_nid_no_patch(std::uintptr_t value) noexcept {
    return value & ~(kGranularity - 1);
}

inline std::uintptr_t RoundUp_nid_no_patch(std::uintptr_t value) noexcept {
    return (value + kGranularity - 1) & ~(kGranularity - 1);
}

inline bool Aligned_nid_no_patch(std::uintptr_t value) noexcept {
    return (value & (kGranularity - 1)) == 0;
}

inline bool ShadowPoisoned_nid_no_patch(std::uintptr_t address) noexcept {
    const auto value = static_cast<std::int8_t>(*ShadowByte_nid_no_patch(address));
    return value != 0 && static_cast<std::int8_t>(address & 7) >= value;
}

inline void PoisonShadow_nid_no_patch(std::uintptr_t address, std::uintptr_t size, std::uint8_t value) noexcept {
    if (size != 0) {
        std::memset(ShadowByte_nid_no_patch(address), value, size >> 3);
    }
}

inline const void* FindBadAddress_nid_no_patch(std::uintptr_t begin, std::uintptr_t end, bool poisoned) noexcept {
    constexpr std::uintptr_t kMaxRangeToCheck = 32;
    if (end - begin > kMaxRangeToCheck * 2) {
        if (const auto* bad = FindBadAddress_nid_no_patch(begin, begin + kMaxRangeToCheck, poisoned)) {
            return bad;
        }
        if (const auto* bad = FindBadAddress_nid_no_patch(end - kMaxRangeToCheck, end, poisoned)) {
            return bad;
        }
    }
    for (auto i = begin; i < end; ++i) {
        if (ShadowPoisoned_nid_no_patch(i) != poisoned) {
            return reinterpret_cast<const void*>(i);
        }
    }
    return nullptr;
}

inline void FixUnalignedStorage_nid_no_patch(std::uintptr_t storageBeg, std::uintptr_t storageEnd, std::uintptr_t& oldBeg, std::uintptr_t& oldEnd, std::uintptr_t& newBeg, std::uintptr_t& newEnd) noexcept {
    if (!Aligned_nid_no_patch(storageEnd)) {
        const auto endDown = RoundDown_nid_no_patch(storageEnd);
        if ((((oldEnd != newEnd) && std::max(oldEnd, newEnd) > endDown) || ((oldBeg != newBeg) && std::max(oldBeg, newBeg) > endDown)) && !ShadowPoisoned_nid_no_patch(storageEnd)) {
            oldBeg = std::min(endDown, oldBeg);
            oldEnd = std::min(endDown, oldEnd);
            newBeg = std::min(endDown, newBeg);
            newEnd = std::min(endDown, newEnd);
        }
    }
    if (!Aligned_nid_no_patch(storageBeg)) {
        const auto begUp = RoundUp_nid_no_patch(storageBeg);
        if ((newBeg == newEnd || newBeg >= begUp) && oldBeg != oldEnd && oldBeg < begUp) {
            const auto begDown = RoundDown_nid_no_patch(storageBeg);
            *ShadowByte_nid_no_patch(begDown) = static_cast<std::uint8_t>(storageBeg - begDown);
            oldBeg = std::max(begUp, oldBeg);
            oldEnd = std::max(begUp, oldEnd);
            newBeg = std::max(begUp, newBeg);
            newEnd = std::max(begUp, newEnd);
        }
    }
}

inline void AnnotateContiguous_nid_no_patch(std::uintptr_t storageBeg, std::uintptr_t storageEnd, std::uintptr_t oldEnd, std::uintptr_t newEnd) noexcept {
    auto oldBeg = storageBeg;
    auto newBeg = storageBeg;
    if (oldEnd == newEnd) {
        return;
    }
    FixUnalignedStorage_nid_no_patch(storageBeg, storageEnd, oldBeg, oldEnd, newBeg, newEnd);
    const auto a = RoundDown_nid_no_patch(std::min(oldEnd, newEnd));
    const auto c = RoundUp_nid_no_patch(std::max(oldEnd, newEnd));
    const auto b1 = RoundDown_nid_no_patch(newEnd);
    const auto b2 = RoundUp_nid_no_patch(newEnd);
    if (b1 > a) {
        PoisonShadow_nid_no_patch(a, b1 - a, 0);
    } else if (c > b2) {
        PoisonShadow_nid_no_patch(b2, c - b2, kContainerOverflowMagic);
    }
    if (b1 != b2) {
        *ShadowByte_nid_no_patch(b1) = static_cast<std::uint8_t>(newEnd - b1);
    }
}

inline void AnnotateDoubleEnded_nid_no_patch(std::uintptr_t storageBeg, std::uintptr_t storageEnd, std::uintptr_t oldBeg, std::uintptr_t oldEnd, std::uintptr_t newBeg, std::uintptr_t newEnd) noexcept {
    if ((oldBeg == oldEnd && newBeg == newEnd) || (oldBeg == newBeg && oldEnd == newEnd)) {
        return;
    }
    FixUnalignedStorage_nid_no_patch(storageBeg, storageEnd, oldBeg, oldEnd, newBeg, newEnd);
    if (oldBeg == oldEnd || newBeg == newEnd || newEnd <= oldBeg || oldEnd <= newBeg) {
        if (oldBeg != oldEnd) {
            const auto a = RoundDown_nid_no_patch(oldBeg);
            const auto b = RoundUp_nid_no_patch(oldEnd);
            PoisonShadow_nid_no_patch(a, b - a, kContainerOverflowMagic);
        }
        if (newBeg != newEnd) {
            const auto a = RoundDown_nid_no_patch(newBeg);
            const auto b = RoundDown_nid_no_patch(newEnd);
            PoisonShadow_nid_no_patch(a, b - a, 0);
            if (!Aligned_nid_no_patch(newEnd)) {
                *ShadowByte_nid_no_patch(b) = static_cast<std::uint8_t>(newEnd - b);
            }
        }
        return;
    }
    if (newBeg < oldBeg) {
        const auto a = RoundDown_nid_no_patch(newBeg);
        const auto c = RoundDown_nid_no_patch(oldBeg);
        PoisonShadow_nid_no_patch(a, c - a, 0);
    } else if (newBeg > oldBeg) {
        const auto a = RoundDown_nid_no_patch(oldBeg);
        const auto c = RoundDown_nid_no_patch(newBeg);
        PoisonShadow_nid_no_patch(a, c - a, kContainerOverflowMagic);
    }
    if (newEnd > oldEnd) {
        const auto a = RoundDown_nid_no_patch(oldEnd);
        const auto c = RoundDown_nid_no_patch(newEnd);
        PoisonShadow_nid_no_patch(a, c - a, 0);
        if (!Aligned_nid_no_patch(newEnd)) {
            *ShadowByte_nid_no_patch(c) = static_cast<std::uint8_t>(newEnd - c);
        }
    } else if (newEnd < oldEnd) {
        const auto a2 = RoundUp_nid_no_patch(newEnd);
        const auto c2 = RoundUp_nid_no_patch(oldEnd);
        PoisonShadow_nid_no_patch(a2, c2 - a2, kContainerOverflowMagic);
        if (!Aligned_nid_no_patch(newEnd)) {
            const auto a = RoundDown_nid_no_patch(newEnd);
            *ShadowByte_nid_no_patch(a) = static_cast<std::uint8_t>(newEnd - a);
        }
    }
}

inline const void* ContiguousFindBad_nid_no_patch(std::uintptr_t beg, std::uintptr_t mid, std::uintptr_t end) noexcept {
    const auto annotationsEnd = (!Aligned_nid_no_patch(end) && !ShadowPoisoned_nid_no_patch(end)) ? RoundDown_nid_no_patch(end) : end;
    beg = std::min(beg, annotationsEnd);
    mid = std::min(mid, annotationsEnd);
    if (const auto* bad = FindBadAddress_nid_no_patch(beg, mid, false)) {
        return bad;
    }
    if (const auto* bad = FindBadAddress_nid_no_patch(mid, annotationsEnd, true)) {
        return bad;
    }
    return FindBadAddress_nid_no_patch(annotationsEnd, end, false);
}

inline const void* DoubleEndedFindBad_nid_no_patch(std::uintptr_t storageBeg, std::uintptr_t beg, std::uintptr_t end, std::uintptr_t storageEnd) noexcept {
    if (beg != end) {
        beg = std::max(storageBeg, RoundDown_nid_no_patch(beg));
    }
    const auto annotationsEnd = (!Aligned_nid_no_patch(storageEnd) && !ShadowPoisoned_nid_no_patch(storageEnd)) ? RoundDown_nid_no_patch(storageEnd) : storageEnd;
    storageBeg = std::min(storageBeg, annotationsEnd);
    beg = std::min(beg, annotationsEnd);
    end = std::min(end, annotationsEnd);
    if (const auto* bad = FindBadAddress_nid_no_patch(storageBeg, beg, true)) {
        return bad;
    }
    if (const auto* bad = FindBadAddress_nid_no_patch(beg, end, false)) {
        return bad;
    }
    if (const auto* bad = FindBadAddress_nid_no_patch(end, annotationsEnd, true)) {
        return bad;
    }
    return FindBadAddress_nid_no_patch(annotationsEnd, storageEnd, false);
}

}  // namespace AsanRuntime
