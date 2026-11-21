/* Derived from LLVM compiler-rt lib/asan (asan_poisoning.cpp, asan_errors.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/AsanRuntime.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Asan::Runtime {

namespace {

constexpr uptr MaxContainerBytes = uptr{1} << 40;

[[noreturn]] void ReportBadParams(const std::string& text, const char* bugType, uptr pc) {
    {
        ScopedErrorReport report(false);
        report.Record(CurrentError{true, false, pc, 0, 0, 0, 0, 0, bugType});
        report.Append(text);
        report.Append(StackLine(pc));
        report.Append(Summary(bugType));
        report.Finish();
    }
    throw std::invalid_argument("guest AddressSanitizer: bad parameters to a container annotation");
}

void FixUnalignedStorage(uptr storageBegin, uptr storageEnd, uptr& oldBegin, uptr& oldEnd, uptr& newBegin, uptr& newEnd) {
    if (!Aligned(storageEnd)) {
        const uptr endDown = RoundDown(storageEnd, Granularity);
        if ((((oldEnd != newEnd) && std::max(oldEnd, newEnd) > endDown) || ((oldBegin != newBegin) && std::max(oldBegin, newBegin) > endDown)) && !AddressIsPoisoned(storageEnd)) {
            oldBegin = std::min(endDown, oldBegin);
            oldEnd = std::min(endDown, oldEnd);
            newBegin = std::min(endDown, newBegin);
            newEnd = std::min(endDown, newEnd);
        }
    }
    if (!Aligned(storageBegin)) {
        const uptr beginUp = RoundUp(storageBegin, Granularity);
        if ((newBegin == newEnd || newBegin >= beginUp) && oldBegin != oldEnd && oldBegin < beginUp) {
            const uptr beginDown = RoundDown(storageBegin, Granularity);
            *ShadowOf(beginDown) = static_cast<u8>(storageBegin - beginDown);
            oldBegin = std::max(beginUp, oldBegin);
            oldEnd = std::max(beginUp, oldEnd);
            newBegin = std::max(beginUp, newBegin);
            newEnd = std::max(beginUp, newEnd);
        }
    }
}

uptr FindBadAddress(uptr begin, uptr end, bool poisoned) {
    Require(begin <= end, "guest AddressSanitizer: container range is reversed");
    constexpr uptr MaxRangeToCheck = 32;
    if (end - begin > MaxRangeToCheck * 2) {
        if (const uptr bad = FindBadAddress(begin, begin + MaxRangeToCheck, poisoned)) return bad;
        if (const uptr bad = FindBadAddress(end - MaxRangeToCheck, end, poisoned)) return bad;
    }
    for (uptr address = begin; address < end; ++address)
        if (AddressIsPoisoned(address) != poisoned) return address;
    return 0;
}

uptr ContiguousFindBad(uptr begin, uptr middle, uptr end) {
    if (!GetFlags().detectContainerOverflow) return 0;
    EnsureShadow();
    Require(begin <= middle && middle <= end, "guest AddressSanitizer: container bounds are out of order");
    const uptr annotationsEnd = (!Aligned(end) && !AddressIsPoisoned(end)) ? RoundDown(end, Granularity) : end;
    begin = std::min(begin, annotationsEnd);
    middle = std::min(middle, annotationsEnd);
    if (const uptr bad = FindBadAddress(begin, middle, false)) return bad;
    if (const uptr bad = FindBadAddress(middle, annotationsEnd, true)) return bad;
    return FindBadAddress(annotationsEnd, end, false);
}

uptr DoubleEndedFindBad(uptr storageBegin, uptr begin, uptr end, uptr storageEnd) {
    if (!GetFlags().detectContainerOverflow) return 0;
    EnsureShadow();
    if (begin != end) begin = std::max(storageBegin, RoundDown(begin, Granularity));
    const uptr annotationsEnd = (!Aligned(storageEnd) && !AddressIsPoisoned(storageEnd)) ? RoundDown(storageEnd, Granularity) : storageEnd;
    storageBegin = std::min(storageBegin, annotationsEnd);
    begin = std::min(begin, annotationsEnd);
    end = std::min(end, annotationsEnd);
    if (const uptr bad = FindBadAddress(storageBegin, begin, true)) return bad;
    if (const uptr bad = FindBadAddress(begin, end, false)) return bad;
    if (const uptr bad = FindBadAddress(end, annotationsEnd, true)) return bad;
    return FindBadAddress(annotationsEnd, storageEnd, false);
}

}

}

using namespace Asan::Runtime;

extern "C" {

[[gnu::noinline]] void APS5_VABI __sanitizer_annotate_contiguous_container(const void* beginPointer, const void* endPointer, const void* oldMiddlePointer, const void* newMiddlePointer) {
    if (!GetFlags().detectContainerOverflow) return;
    EnsureShadow();
    const uptr pc = reinterpret_cast<uptr>(__builtin_return_address(0));
    const uptr storageBegin = reinterpret_cast<uptr>(beginPointer);
    const uptr storageEnd = reinterpret_cast<uptr>(endPointer);
    uptr oldEnd = reinterpret_cast<uptr>(oldMiddlePointer);
    uptr newEnd = reinterpret_cast<uptr>(newMiddlePointer);
    uptr oldBegin = storageBegin;
    uptr newBegin = storageBegin;
    if (!(storageBegin <= oldEnd && storageBegin <= newEnd && oldEnd <= storageEnd && newEnd <= storageEnd)) {
        std::string text = ReportLine(Format("ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_contiguous_container:\n"
                                             "      beg     : %s\n"
                                             "      end     : %s\n"
                                             "      old_mid : %s\n"
                                             "      new_mid : %s\n",
                                             Pointer(storageBegin).c_str(), Pointer(storageEnd).c_str(), Pointer(oldEnd).c_str(), Pointer(newEnd).c_str()));
        if (!Aligned(storageBegin)) text += ReportLine(Format("ERROR: beg is not aligned by %zu\n", static_cast<std::size_t>(Granularity)));
        ReportBadParams(text, "bad-__sanitizer_annotate_contiguous_container", pc);
    }
    Require(storageEnd - storageBegin <= MaxContainerBytes, "guest AddressSanitizer: container is larger than 1 << 40 bytes");
    if (oldEnd == newEnd) return;
    RequireShadow(RoundDown(storageBegin, Granularity), RoundUp(storageEnd, Granularity) - RoundDown(storageBegin, Granularity) + 1);
    FixUnalignedStorage(storageBegin, storageEnd, oldBegin, oldEnd, newBegin, newEnd);
    const uptr a = RoundDown(std::min(oldEnd, newEnd), Granularity);
    const uptr c = RoundUp(std::max(oldEnd, newEnd), Granularity);
    const uptr d1 = RoundDown(oldEnd, Granularity);
    if (a + Granularity <= d1) Require(*ShadowOf(a) == 0, "guest AddressSanitizer: container shadow below the old end is not addressable");
    const uptr b1 = RoundDown(newEnd, Granularity);
    const uptr b2 = RoundUp(newEnd, Granularity);
    if (b1 > a) PoisonShadow(a, b1 - a, 0);
    else if (c > b2) PoisonShadow(b2, c - b2, ContiguousContainerOOBMagic);
    if (b1 != b2) *ShadowOf(b1) = static_cast<u8>(newEnd - b1);
}

[[gnu::noinline]] void APS5_VABI __sanitizer_annotate_double_ended_contiguous_container(const void* storageBeginPointer, const void* storageEndPointer, const void* oldBeginPointer,
                                                                                      const void* oldEndPointer, const void* newBeginPointer, const void* newEndPointer) {
    if (!GetFlags().detectContainerOverflow) return;
    EnsureShadow();
    const uptr pc = reinterpret_cast<uptr>(__builtin_return_address(0));
    const uptr storageBegin = reinterpret_cast<uptr>(storageBeginPointer);
    const uptr storageEnd = reinterpret_cast<uptr>(storageEndPointer);
    uptr oldBegin = reinterpret_cast<uptr>(oldBeginPointer);
    uptr oldEnd = reinterpret_cast<uptr>(oldEndPointer);
    uptr newBegin = reinterpret_cast<uptr>(newBeginPointer);
    uptr newEnd = reinterpret_cast<uptr>(newEndPointer);
    if (!(oldBegin <= oldEnd && newBegin <= newEnd) || !(storageBegin <= newBegin && newEnd <= storageEnd) || !(storageBegin <= oldBegin && oldEnd <= storageEnd)) {
        std::string text = ReportLine(Format("ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_double_ended_contiguous_container:\n"
                                             "      storage_beg        : %s\n"
                                             "      storage_end        : %s\n"
                                             "      old_container_beg  : %s\n"
                                             "      old_container_end  : %s\n"
                                             "      new_container_beg  : %s\n"
                                             "      new_container_end  : %s\n",
                                             Pointer(storageBegin).c_str(), Pointer(storageEnd).c_str(), Pointer(oldBegin).c_str(), Pointer(oldEnd).c_str(), Pointer(newBegin).c_str(),
                                             Pointer(newEnd).c_str()));
        if (!Aligned(storageBegin)) text += ReportLine(Format("ERROR: storage_beg is not aligned by %zu\n", static_cast<std::size_t>(Granularity)));
        ReportBadParams(text, "bad-__sanitizer_annotate_double_ended_contiguous_container", pc);
    }
    Require(storageEnd - storageBegin <= MaxContainerBytes, "guest AddressSanitizer: container is larger than 1 << 40 bytes");
    if ((oldBegin == oldEnd && newBegin == newEnd) || (oldBegin == newBegin && oldEnd == newEnd)) return;
    RequireShadow(RoundDown(storageBegin, Granularity), RoundUp(storageEnd, Granularity) - RoundDown(storageBegin, Granularity) + 1);
    FixUnalignedStorage(storageBegin, storageEnd, oldBegin, oldEnd, newBegin, newEnd);
    if (oldBegin == oldEnd || newBegin == newEnd || newEnd <= oldBegin || oldEnd <= newBegin) {
        if (oldBegin != oldEnd) {
            const uptr a = RoundDown(oldBegin, Granularity);
            const uptr b = RoundUp(oldEnd, Granularity);
            PoisonShadow(a, b - a, ContiguousContainerOOBMagic);
        }
        if (newBegin != newEnd) {
            const uptr a = RoundDown(newBegin, Granularity);
            const uptr b = RoundDown(newEnd, Granularity);
            PoisonShadow(a, b - a, 0);
            if (!Aligned(newEnd)) *ShadowOf(b) = static_cast<u8>(newEnd - b);
        }
        return;
    }
    if (newBegin < oldBegin) {
        const uptr a = RoundDown(newBegin, Granularity);
        const uptr c = RoundDown(oldBegin, Granularity);
        PoisonShadow(a, c - a, 0);
    } else if (newBegin > oldBegin) {
        const uptr a = RoundDown(oldBegin, Granularity);
        const uptr c = RoundDown(newBegin, Granularity);
        PoisonShadow(a, c - a, ContiguousContainerOOBMagic);
    }
    if (newEnd > oldEnd) {
        const uptr a = RoundDown(oldEnd, Granularity);
        const uptr c = RoundDown(newEnd, Granularity);
        PoisonShadow(a, c - a, 0);
        if (!Aligned(newEnd)) *ShadowOf(c) = static_cast<u8>(newEnd - c);
    } else if (newEnd < oldEnd) {
        const uptr a2 = RoundUp(newEnd, Granularity);
        const uptr c2 = RoundUp(oldEnd, Granularity);
        PoisonShadow(a2, c2 - a2, ContiguousContainerOOBMagic);
        if (!Aligned(newEnd)) {
            const uptr a = RoundDown(newEnd, Granularity);
            *ShadowOf(a) = static_cast<u8>(newEnd - a);
        }
    }
}

const void* APS5_VABI __sanitizer_contiguous_container_find_bad_address(const void* begin, const void* middle, const void* end) {
    return reinterpret_cast<const void*>(ContiguousFindBad(reinterpret_cast<uptr>(begin), reinterpret_cast<uptr>(middle), reinterpret_cast<uptr>(end)));
}

int APS5_VABI __sanitizer_verify_contiguous_container(const void* begin, const void* middle, const void* end) {
    return ContiguousFindBad(reinterpret_cast<uptr>(begin), reinterpret_cast<uptr>(middle), reinterpret_cast<uptr>(end)) == 0 ? 1 : 0;
}

const void* APS5_VABI __sanitizer_double_ended_contiguous_container_find_bad_address(const void* storageBegin, const void* containerBegin, const void* containerEnd, const void* storageEnd) {
    return reinterpret_cast<const void*>(DoubleEndedFindBad(reinterpret_cast<uptr>(storageBegin), reinterpret_cast<uptr>(containerBegin), reinterpret_cast<uptr>(containerEnd),
                                                            reinterpret_cast<uptr>(storageEnd)));
}

int APS5_VABI __sanitizer_verify_double_ended_contiguous_container(const void* storageBegin, const void* containerBegin, const void* containerEnd, const void* storageEnd) {
    return DoubleEndedFindBad(reinterpret_cast<uptr>(storageBegin), reinterpret_cast<uptr>(containerBegin), reinterpret_cast<uptr>(containerEnd), reinterpret_cast<uptr>(storageEnd)) == 0 ? 1 : 0;
}

}
