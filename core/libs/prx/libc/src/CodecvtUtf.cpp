/* Derived from Microsoft STL stl/inc/xlocale (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestWindow.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr int Ok = 0;
constexpr int Partial = 1;
constexpr int Error = 2;
constexpr int NoConv = 3;
constexpr std::uint32_t GenerateHeader = 2;
constexpr std::uint32_t ConsumeHeader = 4;
constexpr std::uintptr_t MaxcodeOffset = 0x10;
constexpr std::uintptr_t ModeOffset = 0x18;

struct UtfCodecvtFacet {
    GuestLocale::Facet base;
    std::uint64_t maxcode;
    std::uint32_t mode;
};

static_assert(offsetof(UtfCodecvtFacet, maxcode) == MaxcodeOffset);
static_assert(offsetof(UtfCodecvtFacet, mode) == ModeOffset);

template<typename TElement>
using InFunction = int (APS5_VABI*)(const void* self, void* state, const char* first1, const char* last1, const char** mid1, TElement* first2, TElement* last2, TElement** mid2);

template<typename TValue>
TValue ReadAt(std::uintptr_t address, std::uintptr_t offset, const char* message) {
    if (address > std::numeric_limits<std::uintptr_t>::max() - offset)
        throw std::invalid_argument("UTF codecvt: invalid facet address");
    TValue value = 0;
    GuestCopyInOrThrow(&value, reinterpret_cast<const void*>(address + offset), sizeof(value), message);
    return value;
}

template<typename TElement>
int VirtualIn(const void* self, void* state, const char* first1, const char* last1, const char** mid1, TElement* first2, TElement* last2, TElement** mid2) {
    const auto vtable = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(self), 0, "UTF codecvt: unreadable facet vtable");
    const auto in = ReadAt<std::uintptr_t>(vtable, offsetof(GuestLocale::CodecvtVtable, in), "UTF codecvt: unreadable facet vtable");
    return reinterpret_cast<InFunction<TElement>>(in)(self, state, first1, last1, mid1, first2, last2, mid2);
}

template<typename TValue>
TValue ReadMode(const void* self) {
    return ReadAt<TValue>(reinterpret_cast<std::uintptr_t>(self), ModeOffset, "UTF codecvt: unreadable facet mode");
}

std::uint64_t ReadMaxcode(const void* self) {
    return ReadAt<std::uint64_t>(reinterpret_cast<std::uintptr_t>(self), MaxcodeOffset, "UTF codecvt: unreadable facet maximum code");
}

template<typename TValue>
TValue ReadState(const void* state) {
    TValue value = 0;
    GuestCopyInOrThrow(&value, state, sizeof(value), "UTF codecvt: unreadable shift state");
    return value;
}

template<typename TValue>
void WriteState(void* state, TValue value) {
    GuestCopyOutOrThrow(state, &value, sizeof(value), "UTF codecvt: unwritable shift state");
}

template<typename TElement>
void WriteNext(TElement** next, TElement* first) {
    GuestCopyOutOrThrow(next, &first, sizeof(first), "UTF codecvt: unwritable next pointer");
}

template<typename TSource, typename TDestination>
void CheckRanges(const TSource* first1, const TSource* last1, const TDestination* first2, const TDestination* last2) {
    if (reinterpret_cast<std::uintptr_t>(last1) < reinterpret_cast<std::uintptr_t>(first1) || reinterpret_cast<std::uintptr_t>(last2) < reinterpret_cast<std::uintptr_t>(first2))
        throw std::invalid_argument("UTF codecvt: range ends before it starts");
}

template<typename TElement, typename TIn>
int CodecvtUtfLength(const void* self, void* state, const char* first, const char* last, std::size_t count, TIn in) {
    if (self == nullptr || state == nullptr || (first == nullptr) != (last == nullptr) || (first != nullptr && last < first))
        throw std::invalid_argument("UTF codecvt: invalid length arguments");
    if (first == nullptr) return 0;
    const char* cursor = first;
    while (count > 0 && cursor != last) {
        const char* mid1 = nullptr;
        TElement element{};
        TElement* mid2 = nullptr;
        const int result = in(self, state, cursor, last, &mid1, &element, &element + 1, &mid2);
        if (result == NoConv) {
            const std::size_t remaining = static_cast<std::size_t>(last - cursor);
            cursor += remaining < count ? remaining : count;
            break;
        }
        if (result != Ok) break;
        if (mid2 == &element + 1) --count;
        if (mid1 == cursor && mid2 == &element) break;
        cursor = mid1;
    }
    const std::ptrdiff_t consumed = cursor - first;
    constexpr auto maxResult = static_cast<std::ptrdiff_t>(std::numeric_limits<int>::max());
    return static_cast<int>(consumed > maxResult ? maxResult : consumed);
}

}

extern "C" {

void APS5_VABI CodecvtUtf16Destroy_nid_no_patch(void* self) {
    if (self == nullptr) APS5_INVALID_ARG_EX;
}
APS5_EXPORT("Oeo9tUbzW7s", CodecvtUtf16Destroy_nid_no_patch);

void APS5_VABI CodecvtUtf16Delete_nid_no_patch(void* self) {
    CodecvtUtf16Destroy_nid_no_patch(self);
    _ZdlPv_nid_postfix(self);
}

bool APS5_VABI CodecvtUtf16AlwaysNoconv_nid_no_patch(const void*) {
    return false;
}

int APS5_VABI CodecvtUtf16MaxLength_nid_no_patch(const void* self) {
    const auto mode = ReadMode<std::uint32_t>(self);
    return (mode & ConsumeHeader) != 0 ? 9 : (mode & GenerateHeader) != 0 ? 7 : 6;
}
APS5_EXPORT("BvRS0cGTd6I", CodecvtUtf16MaxLength_nid_no_patch);

int APS5_VABI CodecvtUtf16Encoding_nid_no_patch(const void*) {
    return 0;
}

int APS5_VABI CodecvtUtf16In_nid_no_patch(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2) {
    WriteNext(mid1, first1);
    WriteNext(mid2, first2);
    if (first1 == last1 || first2 == last2) return Partial;
    CheckRanges(first1, last1, first2, last2);
    const std::uint64_t maxcode = ReadMaxcode(self);
    const auto mode = ReadMode<std::uint32_t>(self);
    const auto initial = ReadState<std::uint16_t>(state);
    std::uint16_t pending = initial;
    const char* const begin = first1;
    GuestSourceWindow<char> source(begin, static_cast<std::size_t>(last1 - begin), "UTF codecvt: unreadable source range");
    GuestDestinationWindow<char16_t> destination(first2, "UTF codecvt: unwritable destination range");
    const char* next1 = first1;
    char16_t* next2 = first2;
    const auto read = [&](const char* position) { return static_cast<unsigned char>(source[static_cast<std::size_t>(position - begin)]); };
    const auto put = [&](char16_t value) {
        destination.Put(value);
        ++next2;
    };
    const auto finish = [&](int result) {
        destination.Flush();
        if (pending != initial) WriteState(state, pending);
        WriteNext(mid1, next1);
        WriteNext(mid2, next2);
        return result;
    };
    while (next1 != last1 && next2 != last2) {
        unsigned char byte = read(next1);
        std::uint64_t value;
        int extra;
        int skip;
        if (pending > 1) {
            if (byte < 0x80 || 0xc0 <= byte) return finish(Error);
            ++next1;
            put(static_cast<char16_t>(pending | (byte & 0x3f)));
            pending = 1;
            continue;
        }
        if (byte < 0x80) {
            value = byte;
            extra = 0;
        } else if (byte < 0xc0) {
            ++next1;
            return finish(Error);
        } else if (byte < 0xe0) {
            value = byte & 0x1f;
            extra = 1;
        } else if (byte < 0xf0) {
            value = byte & 0x0f;
            extra = 2;
        } else if (byte < 0xf8) {
            value = byte & 0x07;
            extra = 3;
        } else {
            value = byte & 0x03;
            extra = byte < 0xfc ? 4 : 5;
        }
        skip = extra < 3 ? 0 : 1;
        first1 = next1;
        if (extra == 0) {
            ++next1;
        } else if (last1 - next1 < extra + 1 - skip) {
            break;
        } else {
            for (++next1; skip < extra; --extra, ++next1) {
                if ((byte = read(next1)) < 0x80 || 0xc0 <= byte) return finish(Error);
                value = value << 6 | (byte & 0x3f);
            }
        }
        if (0 < skip) value <<= 6;
        if ((maxcode < 0x10ffff ? maxcode : 0x10ffff) < value) return finish(Error);
        if (0xffff < value) {
            put(static_cast<char16_t>(0xd800 | ((value >> 10) - 0x0040)));
            pending = static_cast<std::uint16_t>(0xdc00 | (value & 0x03ff));
            continue;
        }
        if (skip != 0) {
            if (next1 == last1) {
                next1 = first1;
                break;
            }
            if ((byte = read(next1++)) < 0x80 || 0xc0 <= byte) return finish(Error);
            value |= byte & 0x3f;
        }
        if (pending == 0) {
            pending = 1;
            if ((mode & ConsumeHeader) != 0 && value == 0xfeff) {
                finish(Ok);
                const int result = VirtualIn<char16_t>(self, state, next1, last1, mid1, first2, last2, mid2);
                if (result == Partial) {
                    WriteState<std::uint16_t>(state, 0);
                    WriteNext(mid1, first1);
                }
                return result;
            }
        }
        put(static_cast<char16_t>(value));
    }
    return finish(first1 == next1 ? Partial : Ok);
}

int APS5_VABI CodecvtUtf16Out_nid_no_patch(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2) {
    WriteNext(mid1, first1);
    WriteNext(mid2, first2);
    if (first1 == last1 || first2 == last2) return Partial;
    CheckRanges(first1, last1, first2, last2);
    const auto mode = ReadMode<std::uint32_t>(self);
    const auto initial = ReadState<std::uint16_t>(state);
    std::uint16_t pending = initial;
    GuestSourceWindow<char16_t> source(first1, static_cast<std::size_t>(last1 - first1), "UTF codecvt: unreadable source range");
    GuestDestinationWindow<char> destination(first2, "UTF codecvt: unwritable destination range");
    const char16_t* next1 = first1;
    char* next2 = first2;
    const auto put = [&](char value) {
        destination.Put(value);
        ++next2;
    };
    const auto finish = [&](int result) {
        destination.Flush();
        if (pending != initial) WriteState(state, pending);
        WriteNext(mid1, next1);
        WriteNext(mid2, next2);
        return result;
    };
    while (next1 != last1 && next2 != last2) {
        std::uint64_t value;
        const auto word = static_cast<std::uint16_t>(source[static_cast<std::size_t>(next1 - first1)]);
        bool save = false;
        if (1 < pending) {
            if (word < 0xdc00 || 0xe000 <= word) return finish(Error);
            value = static_cast<std::uint64_t>((pending << 10) | (word - 0xdc00));
        } else if (0xd800 <= word && word < 0xdc00) {
            value = static_cast<std::uint64_t>((word - 0xd800 + 0x0040) << 10);
            save = true;
        } else {
            value = word;
        }
        char lead;
        int extra;
        if (value < 0x0080) {
            lead = static_cast<char>(value);
            extra = 0;
        } else if (value < 0x0800) {
            lead = static_cast<char>(0xc0 | value >> 6);
            extra = 1;
        } else if (value < 0x10000) {
            lead = static_cast<char>(0xe0 | value >> 12);
            extra = 2;
        } else {
            lead = static_cast<char>(0xf0 | value >> 18);
            extra = 3;
        }
        int count = extra < 3 ? extra + 1 : save ? 1 : 3;
        if (last2 - next2 < count) break;
        if (pending == 0 && (mode & GenerateHeader) != 0) {
            if (last2 - next2 < 3 + count) break;
            put('\xef');
            put('\xbb');
            put('\xbf');
        }
        ++next1;
        if (save || extra < 3) {
            put(lead);
            --count;
        }
        for (; 0 < count; --count) put(static_cast<char>((value >> 6 * --extra & 0x3f) | 0x80));
        pending = static_cast<std::uint16_t>(save ? value >> 10 : 1);
    }
    return finish(first1 == next1 ? Partial : Ok);
}

int APS5_VABI CodecvtUtf16Unshift_nid_no_patch(const void*, void* state, char* first, char*, char** next) {
    WriteNext(next, first);
    return ReadState<std::uint16_t>(state) > 1 ? Error : Ok;
}
APS5_EXPORT("RYTHR81Y-Mc", CodecvtUtf16Unshift_nid_no_patch);

int APS5_VABI CodecvtUtf16Length_nid_no_patch(const void* self, void* state, const char* first, const char* last, std::size_t count) {
    return CodecvtUtfLength<char16_t>(self, state, first, last, count, CodecvtUtf16In_nid_no_patch);
}
APS5_EXPORT("lynApfiP6Lw", CodecvtUtf16Length_nid_no_patch);

void APS5_VABI CodecvtUtf32Destroy_nid_no_patch(void* self) {
    if (self == nullptr) APS5_INVALID_ARG_EX;
}

void APS5_VABI CodecvtUtf32Delete_nid_no_patch(void* self) {
    CodecvtUtf32Destroy_nid_no_patch(self);
    _ZdlPv_nid_postfix(self);
}

bool APS5_VABI CodecvtUtf32AlwaysNoconv_nid_no_patch(const void*) {
    return false;
}

int APS5_VABI CodecvtUtf32MaxLength_nid_no_patch(const void* self) {
    return (ReadMode<std::uint8_t>(self) & (GenerateHeader | ConsumeHeader)) != 0 ? 9 : 6;
}
APS5_EXPORT("E5NdzqEmWuY", CodecvtUtf32MaxLength_nid_no_patch);

int APS5_VABI CodecvtUtf32Encoding_nid_no_patch(const void* self) {
    return (ReadMode<std::uint8_t>(self) & (GenerateHeader | ConsumeHeader)) != 0 ? -1 : 0;
}
APS5_EXPORT("DDnr3lDwW8I", CodecvtUtf32Encoding_nid_no_patch);

int APS5_VABI CodecvtUtf32In_nid_no_patch(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2) {
    WriteNext(mid1, first1);
    WriteNext(mid2, first2);
    if (first1 == last1 || first2 == last2) return Partial;
    CheckRanges(first1, last1, first2, last2);
    const std::uint64_t maxcode = ReadMaxcode(self);
    const auto mode = ReadMode<std::uint32_t>(self);
    const auto initial = ReadState<char>(state);
    char started = initial;
    GuestSourceWindow<char> source(first1, static_cast<std::size_t>(last1 - first1), "UTF codecvt: unreadable source range");
    GuestDestinationWindow<char32_t> destination(first2, "UTF codecvt: unwritable destination range");
    const char* next1 = first1;
    char32_t* next2 = first2;
    const auto read = [&](const char* position) { return static_cast<unsigned char>(source[static_cast<std::size_t>(position - first1)]); };
    const auto finish = [&](int result) {
        destination.Flush();
        if (started != initial) WriteState(state, started);
        WriteNext(mid1, next1);
        WriteNext(mid2, next2);
        return result;
    };
    while (next1 != last1 && next2 != last2) {
        unsigned char byte = read(next1);
        std::uint64_t value;
        int extra;
        if (byte < 0x80) {
            value = byte;
            extra = 0;
        } else if (byte < 0xc0) {
            ++next1;
            return finish(Error);
        } else if (byte < 0xe0) {
            value = byte & 0x1f;
            extra = 1;
        } else if (byte < 0xf0) {
            value = byte & 0x0f;
            extra = 2;
        } else if (byte < 0xf8) {
            value = byte & 0x07;
            extra = 3;
        } else {
            value = byte & 0x03;
            extra = byte < 0xfc ? 4 : 5;
        }
        if (extra == 0) {
            ++next1;
        } else if (last1 - next1 < extra + 1) {
            break;
        } else {
            for (++next1; 0 < extra; --extra, ++next1) {
                if ((byte = read(next1)) < 0x80 || 0xc0 <= byte) return finish(Error);
                value = value << 6 | (byte & 0x3f);
            }
        }
        if (started == 0) {
            started = 1;
            if ((mode & ConsumeHeader) != 0 && value == 0xfeff) {
                finish(Ok);
                const int result = VirtualIn<char32_t>(self, state, next1, last1, mid1, first2, last2, mid2);
                if (result == Partial) {
                    WriteState<char>(state, 0);
                    WriteNext(mid1, first1);
                }
                return result;
            }
        }
        if (maxcode < value) return finish(Error);
        destination.Put(static_cast<char32_t>(value));
        ++next2;
    }
    return finish(first1 == next1 ? Partial : Ok);
}

int APS5_VABI CodecvtUtf32Out_nid_no_patch(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2) {
    WriteNext(mid1, first1);
    WriteNext(mid2, first2);
    if (first1 == last1 || first2 == last2) return Partial;
    CheckRanges(first1, last1, first2, last2);
    const std::uint64_t maxcode = ReadMaxcode(self);
    const auto mode = ReadMode<std::uint32_t>(self);
    const auto initial = ReadState<char>(state);
    char started = initial;
    GuestSourceWindow<char32_t> source(first1, static_cast<std::size_t>(last1 - first1), "UTF codecvt: unreadable source range");
    GuestDestinationWindow<char> destination(first2, "UTF codecvt: unwritable destination range");
    const char32_t* next1 = first1;
    char* next2 = first2;
    const auto put = [&](char value) {
        destination.Put(value);
        ++next2;
    };
    const auto finish = [&](int result) {
        destination.Flush();
        if (started != initial) WriteState(state, started);
        WriteNext(mid1, next1);
        WriteNext(mid2, next2);
        return result;
    };
    while (next1 != last1 && next2 != last2) {
        char lead;
        int extra;
        const auto value = static_cast<std::uint64_t>(source[static_cast<std::size_t>(next1 - first1)]);
        if (maxcode < value) return finish(Error);
        if (value < 0x0080) {
            lead = static_cast<char>(value);
            extra = 0;
        } else if (value < 0x0800) {
            lead = static_cast<char>(0xc0 | value >> 6);
            extra = 1;
        } else if (value < 0x00010000) {
            lead = static_cast<char>(0xe0 | value >> 12);
            extra = 2;
        } else if (value < 0x00200000) {
            lead = static_cast<char>(0xf0 | value >> 18);
            extra = 3;
        } else if (value < 0x04000000) {
            lead = static_cast<char>(0xf8 | value >> 24);
            extra = 4;
        } else {
            lead = static_cast<char>(0xfc | (value >> 30 & 0x03));
            extra = 5;
        }
        if (started == 0) {
            started = 1;
            if ((mode & GenerateHeader) != 0) {
                if (last2 - next2 < 3 + 1 + extra) return finish(Partial);
                put('\xef');
                put('\xbb');
                put('\xbf');
            }
        }
        if (last2 - next2 < 1 + extra) break;
        ++next1;
        for (put(lead); 0 < extra;) put(static_cast<char>((value >> 6 * --extra & 0x3f) | 0x80));
    }
    return finish(first1 == next1 ? Partial : Ok);
}

int APS5_VABI CodecvtUtf32Unshift_nid_no_patch(const void*, void*, char* first, char*, char** next) {
    WriteNext(next, first);
    return Ok;
}
APS5_EXPORT("c6Lyc6xOp4o", CodecvtUtf32Unshift_nid_no_patch);

int APS5_VABI CodecvtUtf32Length_nid_no_patch(const void* self, void* state, const char* first, const char* last, std::size_t count) {
    return CodecvtUtfLength<char32_t>(self, state, first, last, count, CodecvtUtf32In_nid_no_patch);
}

void APS5_VABI _ZNSt7codecvtIDsc9_MbstatetED0Ev_nid_postfix(void* self) {
    CodecvtUtf16Delete_nid_no_patch(self);
}

bool APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv_nid_postfix(const void* self) {
    return CodecvtUtf16AlwaysNoconv_nid_no_patch(self);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv_nid_postfix(const void* self) {
    return CodecvtUtf16Encoding_nid_no_patch(self);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2) {
    return CodecvtUtf16In_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2) {
    return CodecvtUtf16Out_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED1Ev_nid_postfix(void* self) {
    CodecvtUtf32Destroy_nid_no_patch(self);
}

void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED0Ev_nid_postfix(void* self) {
    CodecvtUtf32Delete_nid_no_patch(self);
}

bool APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv_nid_postfix(const void* self) {
    return CodecvtUtf32AlwaysNoconv_nid_no_patch(self);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2) {
    return CodecvtUtf32In_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2) {
    return CodecvtUtf32Out_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(const void* self, void* state, const char* first, const char* last, std::size_t count) {
    return CodecvtUtf32Length_nid_no_patch(self, state, first, last, count);
}

}
