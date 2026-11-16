/* Derived from Microsoft STL stl/inc/xlocale (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/CodecvtUtfConversion.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstddef>
#include <cstdint>

namespace {

constexpr int Ok = 0;
constexpr int Partial = 1;
constexpr int Error = 2;
constexpr std::uint32_t GenerateHeader = 2;
constexpr std::uint32_t ConsumeHeader = 4;

struct UtfCodecvtFacet {
    GuestLocale::Facet base;
    std::uint64_t maxcode;
    std::uint32_t mode;
};

static_assert(offsetof(UtfCodecvtFacet, maxcode) == 0x10);
static_assert(offsetof(UtfCodecvtFacet, mode) == 0x18);

template<typename TElement>
using InFunction = int (APS5_VABI*)(const void* self, void* state, const char* first1, const char* last1, const char** mid1, TElement* first2, TElement* last2, TElement** mid2);

const UtfCodecvtFacet& FacetOf(const void* self) {
    return *static_cast<const UtfCodecvtFacet*>(self);
}

template<typename TElement>
int VirtualIn(const void* self, void* state, const char* first1, const char* last1, const char** mid1, TElement* first2, TElement* last2, TElement** mid2) {
    const auto* vtable = reinterpret_cast<const GuestLocale::CodecvtVtable*>(FacetOf(self).base.vtable);
    return reinterpret_cast<InFunction<TElement>>(vtable->in)(self, state, first1, last1, mid1, first2, last2, mid2);
}

}

extern "C" {

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2) {
    const UtfCodecvtFacet& facet = FacetOf(self);
    auto* pending = static_cast<std::uint16_t*>(state);
    *mid1 = first1;
    *mid2 = first2;
    while (*mid1 != last1 && *mid2 != last2) {
        unsigned char byte = static_cast<unsigned char>(**mid1);
        std::uint64_t value;
        int extra;
        int skip;
        if (*pending > 1) {
            if (byte < 0x80 || 0xc0 <= byte) return Error;
            ++*mid1;
            *(*mid2)++ = static_cast<char16_t>(*pending | (byte & 0x3f));
            *pending = 1;
            continue;
        }
        if (byte < 0x80) {
            value = byte;
            extra = 0;
        } else if (byte < 0xc0) {
            ++*mid1;
            return Error;
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
        first1 = *mid1;
        if (extra == 0) {
            ++*mid1;
        } else if (last1 - *mid1 < extra + 1 - skip) {
            break;
        } else {
            for (++*mid1; skip < extra; --extra, ++*mid1) {
                if ((byte = static_cast<unsigned char>(**mid1)) < 0x80 || 0xc0 <= byte) return Error;
                value = value << 6 | (byte & 0x3f);
            }
        }
        if (0 < skip) value <<= 6;
        if ((facet.maxcode < 0x10ffff ? facet.maxcode : 0x10ffff) < value) return Error;
        if (0xffff < value) {
            *(*mid2)++ = static_cast<char16_t>(0xd800 | ((value >> 10) - 0x0040));
            *pending = static_cast<std::uint16_t>(0xdc00 | (value & 0x03ff));
            continue;
        }
        if (skip != 0) {
            if (*mid1 == last1) {
                *mid1 = first1;
                break;
            }
            if ((byte = static_cast<unsigned char>(*(*mid1)++)) < 0x80 || 0xc0 <= byte) return Error;
            value |= byte & 0x3f;
        }
        if (*pending == 0) {
            *pending = 1;
            if ((facet.mode & ConsumeHeader) != 0 && value == 0xfeff) {
                const int result = VirtualIn<char16_t>(self, state, *mid1, last1, mid1, first2, last2, mid2);
                if (result == Partial) {
                    *pending = 0;
                    *mid1 = first1;
                }
                return result;
            }
        }
        *(*mid2)++ = static_cast<char16_t>(value);
    }
    return first1 == *mid1 ? Partial : Ok;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2) {
    const UtfCodecvtFacet& facet = FacetOf(self);
    auto* pending = static_cast<std::uint16_t*>(state);
    *mid1 = first1;
    *mid2 = first2;
    while (*mid1 != last1 && *mid2 != last2) {
        std::uint64_t value;
        const auto word = static_cast<std::uint16_t>(**mid1);
        bool save = false;
        if (1 < *pending) {
            if (word < 0xdc00 || 0xe000 <= word) return Error;
            value = static_cast<std::uint64_t>((*pending << 10) | (word - 0xdc00));
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
        if (last2 - *mid2 < count) break;
        if (*pending == 0 && (facet.mode & GenerateHeader) != 0) {
            if (last2 - *mid2 < 3 + count) break;
            *(*mid2)++ = '\xef';
            *(*mid2)++ = '\xbb';
            *(*mid2)++ = '\xbf';
        }
        ++*mid1;
        if (save || extra < 3) {
            *(*mid2)++ = lead;
            --count;
        }
        for (; 0 < count; --count) *(*mid2)++ = static_cast<char>((value >> 6 * --extra & 0x3f) | 0x80);
        *pending = static_cast<std::uint16_t>(save ? value >> 10 : 1);
    }
    return first1 == *mid1 ? Partial : Ok;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2) {
    const UtfCodecvtFacet& facet = FacetOf(self);
    auto* started = static_cast<char*>(state);
    *mid1 = first1;
    *mid2 = first2;
    while (*mid1 != last1 && *mid2 != last2) {
        unsigned char byte = static_cast<unsigned char>(**mid1);
        std::uint64_t value;
        int extra;
        if (byte < 0x80) {
            value = byte;
            extra = 0;
        } else if (byte < 0xc0) {
            ++*mid1;
            return Error;
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
            ++*mid1;
        } else if (last1 - *mid1 < extra + 1) {
            break;
        } else {
            for (++*mid1; 0 < extra; --extra, ++*mid1) {
                if ((byte = static_cast<unsigned char>(**mid1)) < 0x80 || 0xc0 <= byte) return Error;
                value = value << 6 | (byte & 0x3f);
            }
        }
        if (*started == 0) {
            *started = 1;
            if ((facet.mode & ConsumeHeader) != 0 && value == 0xfeff) {
                const int result = VirtualIn<char32_t>(self, state, *mid1, last1, mid1, first2, last2, mid2);
                if (result == Partial) {
                    *started = 0;
                    *mid1 = first1;
                }
                return result;
            }
        }
        if (facet.maxcode < value) return Error;
        *(*mid2)++ = static_cast<char32_t>(value);
    }
    return first1 == *mid1 ? Partial : Ok;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2) {
    const UtfCodecvtFacet& facet = FacetOf(self);
    auto* started = static_cast<char*>(state);
    *mid1 = first1;
    *mid2 = first2;
    while (*mid1 != last1 && *mid2 != last2) {
        char lead;
        int extra;
        const auto value = static_cast<std::uint64_t>(**mid1);
        if (facet.maxcode < value) return Error;
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
        if (*started == 0) {
            *started = 1;
            if ((facet.mode & GenerateHeader) != 0) {
                if (last2 - *mid2 < 3 + 1 + extra) return Partial;
                *(*mid2)++ = '\xef';
                *(*mid2)++ = '\xbb';
                *(*mid2)++ = '\xbf';
            }
        }
        if (last2 - *mid2 < 1 + extra) break;
        ++*mid1;
        for (*(*mid2)++ = lead; 0 < extra;) *(*mid2)++ = static_cast<char>((value >> 6 * --extra & 0x3f) | 0x80);
    }
    return first1 == *mid1 ? Partial : Ok;
}

}
