// Derived from Microsoft C++ Standard Library (stl/inc/xlocnum)
// https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xlocnum
//
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

extern "C" {
extern std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t IosEofBit = 0x1;
constexpr std::int32_t IosFailBit = 0x2;
constexpr std::int32_t IosOct = 0x20;
constexpr std::int32_t IosHex = 0x40;
constexpr std::int32_t IosBaseField = 0x70;
constexpr std::int32_t IosBoolAlpha = 0x8000;
constexpr std::size_t MaxIntDigits = 32;
constexpr int MaxSigDigV1 = 36;
constexpr int MaxSigDigV2 = 768;
constexpr int MaxExpDig = 8;
constexpr std::size_t FloatingBufferSize = MaxExpDig + MaxSigDigV2 + 16;
constexpr std::int32_t EnableV2Behavior = 1000000000;
constexpr std::ptrdiff_t DecExpAbsBound = 6000;
constexpr std::ptrdiff_t HexExpAbsBound = 20000;

struct InputStreambufVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

static_assert(offsetof(InputStreambufVtable, underflow) == 0x38);
static_assert(offsetof(InputStreambufVtable, uflow) == 0x40);
static_assert(offsetof(GuestLocale::Streambuf, reserved) + 6 * sizeof(void*) == 0x38);
static_assert(offsetof(GuestLocale::Streambuf, reservedCounts) + 2 * sizeof(void*) == 0x58);

template<typename TChar>
struct InputIteratorState {
    GuestLocale::Streambuf* buffer;
    bool got;
    TChar value;
};

static_assert(sizeof(InputIteratorState<char>) == sizeof(GuestLocale::InputIterator));
static_assert(sizeof(InputIteratorState<std::uint16_t>) == sizeof(GuestLocale::InputIterator));
static_assert(offsetof(InputIteratorState<std::uint16_t>, value) == 0xa);

template<typename TChar>
InputIteratorState<TChar> FromGuestIterator(GuestLocale::InputIterator iterator) {
    InputIteratorState<TChar> state{};
    std::memcpy(&state, &iterator, sizeof(state));
    return state;
}

template<typename TChar>
GuestLocale::InputIterator ToGuestIterator(InputIteratorState<TChar> state) {
    GuestLocale::InputIterator iterator{};
    std::memcpy(&iterator, &state, sizeof(iterator));
    return iterator;
}

const InputStreambufVtable& StreambufVirtuals(const GuestLocale::Streambuf* buffer) {
    if (buffer->vtable == nullptr) throw std::invalid_argument("num_get: null streambuf vtable");
    return *reinterpret_cast<const InputStreambufVtable*>(buffer->vtable);
}

char** StreambufGetNext(GuestLocale::Streambuf* buffer) {
    return static_cast<char**>(const_cast<void*>(buffer->reserved[6]));
}

char** StreambufGetEnd(GuestLocale::Streambuf* buffer) {
    return static_cast<char**>(const_cast<void*>(buffer->reservedCounts[2]));
}

bool HasBufferedInput(GuestLocale::Streambuf* buffer, std::size_t elementSize, char**& nextSlot) {
    nextSlot = StreambufGetNext(buffer);
    char** endSlot = StreambufGetEnd(buffer);
    if (nextSlot == nullptr || endSlot == nullptr) return false;
    char* next = *nextSlot;
    char* end = *endSlot;
    if (next == nullptr || end == nullptr) return false;
    const auto available = static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(end) - reinterpret_cast<std::uintptr_t>(next));
    return available >= static_cast<std::intptr_t>(elementSize);
}

template<typename TChar>
TChar PeekChar(InputIteratorState<TChar>& in) {
    if (in.got) return in.value;
    if (in.buffer != nullptr) {
        char** nextSlot = nullptr;
        if (HasBufferedInput(in.buffer, sizeof(TChar), nextSlot)) {
            TChar value{};
            std::memcpy(&value, *nextSlot, sizeof(TChar));
            in.value = value;
        } else {
            const int ch = StreambufVirtuals(in.buffer).underflow(in.buffer);
            if (ch == -1) in.buffer = nullptr;
            else in.value = static_cast<TChar>(ch);
        }
    }
    in.got = true;
    return in.value;
}

template<typename TChar>
void AdvanceChar(InputIteratorState<TChar>& in) {
    if (in.buffer != nullptr) {
        char** nextSlot = nullptr;
        if (HasBufferedInput(in.buffer, sizeof(TChar), nextSlot)) {
            *nextSlot += sizeof(TChar);
            in.got = false;
            return;
        }
        if (StreambufVirtuals(in.buffer).uflow(in.buffer) != -1) {
            in.got = false;
            return;
        }
        in.buffer = nullptr;
    }
    in.got = true;
}

template<typename TChar>
bool InputEnded(InputIteratorState<TChar>& first, InputIteratorState<TChar>& last) {
    PeekChar(first);
    PeekChar(last);
    return (first.buffer == nullptr) == (last.buffer == nullptr);
}

const GuestLocale::Facet* FindFacet(const GuestLocale::IosBase* stream, std::uint64_t& id, GuestLocale::GlobalLocaleHold& hold) {
    std::atomic_ref<std::uint64_t> slot(id);
    if (slot.load() == 0) {
        _Locksyslock_nid_postfix();
        if (slot.load() == 0) slot.store(static_cast<std::uint64_t>(++_ZNSt6locale2id7_Id_cntE_nid_postfix));
        _Unlocksyslock_nid_postfix();
    }
    const std::uint64_t index = slot.load();
    const GuestLocale::Implementation* locale = *stream->locale;
    const GuestLocale::Facet* facet = index < locale->facetCount ? locale->facets[index] : nullptr;
    if (facet != nullptr || !locale->transparent) return facet;
    return hold.Find(index);
}

template<typename TPunct>
std::string TakePunctString(GuestLocale::String* (APS5_VABI *method)(GuestLocale::String*, const TPunct*), const GuestLocale::Facet* facet) {
    GuestLocale::String value{};
    method(&value, reinterpret_cast<const TPunct*>(facet));
    struct Guard {
        GuestLocale::String& ref;
        bool onHeap;
        ~Guard() {
            if (onHeap) GuestStringStorage::Deallocate(ref.pointer, ref.capacity + 1);
        }
    } guard{value, value.capacity > GuestLocale::StringInlineCapacity};
    return std::string(guard.onHeap ? value.pointer : value.buffer, value.size);
}

template<typename TChar>
struct PunctInfo {
    std::string grouping;
    TChar decimalPoint;
    TChar thousandsSep;
};

template<typename TChar>
PunctInfo<TChar> ReadPunctInfo(const GuestLocale::IosBase* stream);

template<>
PunctInfo<char> ReadPunctInfo<char>(const GuestLocale::IosBase* stream) {
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* punct = FindFacet(stream, _ZNSt8numpunctIcE2idE_nid_postfix, hold);
    if (punct == nullptr) return {"", '.', '\0'};
    const auto& vtable = *reinterpret_cast<const GuestLocale::NumpunctVtable*>(punct->vtable);
    std::string grouping = TakePunctString(vtable.grouping, punct);
    const char thousandsSep = grouping.empty() ? '\0' : vtable.thousandsSep(reinterpret_cast<const GuestLocale::NumpunctFacet*>(punct));
    const char decimalPoint = vtable.decimalPoint(reinterpret_cast<const GuestLocale::NumpunctFacet*>(punct));
    return {std::move(grouping), decimalPoint, thousandsSep};
}

template<>
PunctInfo<std::uint16_t> ReadPunctInfo<std::uint16_t>(const GuestLocale::IosBase* stream) {
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* punct = FindFacet(stream, _ZNSt8numpunctIwE2idE_nid_postfix, hold);
    if (punct == nullptr) return {"", u'.', 0};
    const auto& vtable = *reinterpret_cast<const GuestLocale::WideNumpunctVtable*>(punct->vtable);
    std::string grouping = TakePunctString(vtable.grouping, punct);
    const std::uint16_t thousandsSep = grouping.empty() ? std::uint16_t{0} : vtable.thousandsSep(reinterpret_cast<const GuestLocale::WideNumpunctFacet*>(punct));
    const std::uint16_t decimalPoint = vtable.decimalPoint(reinterpret_cast<const GuestLocale::WideNumpunctFacet*>(punct));
    return {std::move(grouping), decimalPoint, thousandsSep};
}

template<typename TChar, std::size_t TSize>
void WidenAtoms(const GuestLocale::IosBase* stream, const char (&src)[TSize], TChar (&atoms)[TSize]);

template<std::size_t TSize>
void WidenAtoms(const GuestLocale::IosBase* stream, const char (&src)[TSize], char (&atoms)[TSize]) {
    GuestLocale::GlobalLocaleHold hold;
    if (const GuestLocale::Facet* ctype = FindFacet(stream, _ZNSt5ctypeIcE2idE_nid_postfix, hold)) {
        reinterpret_cast<const GuestLocale::CtypeVtable*>(ctype->vtable)->widenRange(ctype, src, src + TSize, atoms);
        return;
    }
    std::memcpy(atoms, src, TSize);
}

template<std::size_t TSize>
void WidenAtoms(const GuestLocale::IosBase* stream, const char (&src)[TSize], std::uint16_t (&atoms)[TSize]) {
    GuestLocale::GlobalLocaleHold hold;
    if (const GuestLocale::Facet* ctype = FindFacet(stream, _ZNSt5ctypeIwE2idE_nid_postfix, hold)) {
        using WideWidenRange = const char* (APS5_VABI*)(const GuestLocale::Facet*, const char*, const char*, std::uint16_t*);
        const auto& vtable = *reinterpret_cast<const GuestLocale::WideCtypeVtable*>(ctype->vtable);
        WideWidenRange widenRange{};
        std::memcpy(&widenRange, &vtable.widenMethods[1], sizeof(widenRange));
        widenRange(ctype, src, src + TSize, atoms);
        return;
    }
    for (std::size_t i = 0; i < TSize; ++i) {
        atoms[i] = static_cast<std::uint16_t>(_Btowc_nid_postfix(static_cast<unsigned char>(src[i])));
    }
}

template<typename TChar, std::size_t TSize>
std::size_t FindAtom(const TChar (&atoms)[TSize], TChar ch) {
    return static_cast<std::size_t>(std::find(atoms, atoms + (TSize - 1), ch) - atoms);
}

template<typename TChar>
bool SkipSeparators(InputIteratorState<TChar>& first, InputIteratorState<TChar>& last, TChar separator) {
    if (separator == TChar{}) return false;
    bool skipped = false;
    while (!InputEnded(first, last) && PeekChar(first) == separator) {
        AdvanceChar(first);
        skipped = true;
    }
    return skipped;
}

bool ValidateGroups(const std::string& grouping, const std::string& groups, std::size_t groupCount) {
    const char* iter = grouping.data();
    const char* const end = iter + grouping.size();
    char currentCount = '\0';
    while (groupCount > 0) {
        if (iter != end) {
            currentCount = *iter;
            ++iter;
        }
        --groupCount;
        if (currentCount > '\0' && currentCount != CHAR_MAX) {
            if ((groupCount > 0 && groups[groupCount] != currentCount) || (groupCount == 0 && groups[groupCount] > currentCount)) {
                return true;
            }
        }
    }
    return false;
}

struct ParseResult {
    std::int8_t base;
    bool badGrouping;
};

template<typename TChar>
ParseResult ParseIntField(char* ac, InputIteratorState<TChar>& first, InputIteratorState<TChar>& last, const GuestLocale::IosBase* stream) {
    const auto punct = ReadPunctInfo<TChar>(stream);
    constexpr int SignOffset = 22;
    constexpr int XOffset = 24;
    static constexpr char Src[] = "0123456789ABCDEFabcdef-+Xx";
    TChar atoms[sizeof(Src)]{};
    WidenAtoms(stream, Src, atoms);

    bool badGrouping = SkipSeparators(first, last, punct.thousandsSep);
    char* ptr = ac;
    if (!InputEnded(first, last)) {
        const TChar ch = PeekChar(first);
        if (ch == atoms[SignOffset + 1]) {
            *ptr++ = '+';
            AdvanceChar(first);
        } else if (ch == atoms[SignOffset]) {
            *ptr++ = '-';
            AdvanceChar(first);
        }
    }
    if (SkipSeparators(first, last, punct.thousandsSep)) badGrouping = true;

    const std::int32_t basefield = stream->flags & IosBaseField;
    std::int8_t base = 10;
    if (basefield == IosOct) base = 8;
    else if (basefield == IosHex) base = 16;
    else if (basefield == 0) base = 0;

    bool seenDigit = false;
    bool nonzero = false;
    if (!InputEnded(first, last) && PeekChar(first) == atoms[0]) {
        seenDigit = true;
        AdvanceChar(first);
        if (!InputEnded(first, last) && (PeekChar(first) == atoms[XOffset + 1] || PeekChar(first) == atoms[XOffset]) && (base == 0 || base == 16)) {
            base = 16;
            seenDigit = false;
            AdvanceChar(first);
        } else if (base == 0) {
            base = 8;
        }
    }

    const std::size_t dlen = static_cast<std::size_t>(base == 0 || base == 10 ? 10 : base == 8 ? 8 : 22);
    std::string groups(1, static_cast<char>(seenDigit));
    std::size_t groupsArrIdx = 0;

    for (char* const pe = ac + MaxIntDigits - 1; !InputEnded(first, last); AdvanceChar(first)) {
        const TChar ch = PeekChar(first);
        const std::size_t idx = FindAtom(atoms, ch);
        if (idx < dlen) {
            *ptr = Src[idx];
            if ((nonzero || *ptr != '0') && ptr < pe) {
                ++ptr;
                nonzero = true;
            }
            seenDigit = true;
            if (groups[groupsArrIdx] != CHAR_MAX) ++groups[groupsArrIdx];
        } else if (punct.thousandsSep == TChar{} || ch != punct.thousandsSep) {
            break;
        } else if (groups[groupsArrIdx] == '\0') {
            badGrouping = true;
        } else {
            groups.push_back('\0');
            ++groupsArrIdx;
        }
    }

    if (groupsArrIdx != 0) {
        if (groups[groupsArrIdx] > '\0') ++groupsArrIdx;
        else badGrouping = true;
    }

    if (SkipSeparators(first, last, punct.thousandsSep)) badGrouping = true;

    if (seenDigit && !badGrouping && groupsArrIdx > 0) {
        badGrouping = ValidateGroups(punct.grouping, groups, groupsArrIdx);
    }

    if (!seenDigit) return {static_cast<std::int8_t>(~base), false};
    if (!nonzero) *ptr++ = '0';
    *ptr = '\0';
    return {base, badGrouping};
}

void ValidateNumGetArgs(GuestLocale::IosBase* stream, std::int32_t* state, void* value) {
    if (stream == nullptr || state == nullptr || value == nullptr || stream->locale == nullptr || *stream->locale == nullptr) {
        throw std::invalid_argument("num_get::do_get: null argument");
    }
}

template<typename TChar, typename TUInt>
GuestLocale::InputIterator DoGetBoundedUnsigned(GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, TUInt* value) {
    ValidateNumGetArgs(stream, state, value);
    auto inFirst = FromGuestIterator<TChar>(first);
    auto inLast = FromGuestIterator<TChar>(last);
    char ac[MaxIntDigits];
    const auto parsed = ParseIntField(ac, inFirst, inLast, stream);
    if (parsed.base < 0) {
        *state = IosFailBit;
        *value = 0;
    } else {
        const bool minus = ac[0] == '-';
        const char* digits = minus ? ac + 1 : ac;
        char* ep = nullptr;
        int err = 0;
        int& guestErrno = *__error_nid_postfix();
        const int savedErrno = guestErrno;
        const std::uint64_t tmp = _Stoulx_nid_postfix(digits, &ep, parsed.base, &err);
        guestErrno = savedErrno;
        if (ep == digits || err != 0 || tmp > std::numeric_limits<TUInt>::max()) {
            *state = IosFailBit;
            *value = std::numeric_limits<TUInt>::max();
        } else {
            *value = static_cast<TUInt>(minus ? 0 - tmp : tmp);
        }
        if (parsed.badGrouping) *state = IosFailBit;
    }
    if (InputEnded(inFirst, inLast)) *state |= IosEofBit;
    return ToGuestIterator(inFirst);
}

template<typename TChar, typename TInt, typename TParseReturn>
GuestLocale::InputIterator DoGetFullInt(GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, TInt* value, TParseReturn (APS5_VABI *parse)(const char*, char**, int, int*)) {
    ValidateNumGetArgs(stream, state, value);
    auto inFirst = FromGuestIterator<TChar>(first);
    auto inLast = FromGuestIterator<TChar>(last);
    char ac[MaxIntDigits];
    const auto parsed = ParseIntField(ac, inFirst, inLast, stream);
    if (parsed.base < 0) {
        *state = IosFailBit;
        *value = 0;
    } else {
        char* ep = nullptr;
        int err = 0;
        int& guestErrno = *__error_nid_postfix();
        const int savedErrno = guestErrno;
        *value = static_cast<TInt>(parse(ac, &ep, parsed.base, &err));
        guestErrno = savedErrno;
        if (ep == ac || err != 0 || parsed.badGrouping) *state = IosFailBit;
    }
    if (InputEnded(inFirst, inLast)) *state |= IosEofBit;
    return ToGuestIterator(inFirst);
}

template<typename TChar>
ParseResult ParseFloatField(char* ac, int maxSigDig, InputIteratorState<TChar>& first, InputIteratorState<TChar>& last, const GuestLocale::IosBase* stream) {
    char* ptr = ac;
    constexpr std::size_t OffsetDecDigitEnd = 10;
    constexpr std::size_t OffsetHexDigitEnd = 22;
    constexpr std::size_t OffsetNegSign = 22;
    constexpr std::size_t OffsetPosSign = 23;
    constexpr std::size_t OffsetUpperX = 24;
    constexpr std::size_t OffsetLowerX = 25;
    constexpr std::size_t OffsetUpperP = 26;
    constexpr std::size_t OffsetLowerP = 27;
    constexpr std::size_t OffsetUpperE = 14;
    constexpr std::size_t OffsetLowerE = 20;
    static constexpr char Src[] = "0123456789ABCDEFabcdef-+XxPp";
    TChar atoms[sizeof(Src)]{};
    WidenAtoms(stream, Src, atoms);

    const TChar positiveSign = atoms[OffsetPosSign];
    const TChar negativeSign = atoms[OffsetNegSign];
    const TChar zeroWc = atoms[0];
    const auto punct = ReadPunctInfo<TChar>(stream);
    const TChar kSeparator = punct.grouping.empty() ? TChar{} : punct.thousandsSep;

    bool badGrouping = !punct.grouping.empty() && SkipSeparators(first, last, kSeparator);
    if (!InputEnded(first, last)) {
        const TChar ch = PeekChar(first);
        if (ch == positiveSign) {
            *ptr++ = '+';
            AdvanceChar(first);
        } else if (ch == negativeSign) {
            *ptr++ = '-';
            AdvanceChar(first);
        }
    }

    *ptr++ = '0';

    bool parseHex = false;
    bool seenDigit = false;
    char initialDecLeadingZero = '\0';
    if (!InputEnded(first, last) && PeekChar(first) == zeroWc) {
        AdvanceChar(first);
        if (InputEnded(first, last)) {
            *ptr = '\0';
            return {10, badGrouping};
        }
        const TChar next = PeekChar(first);
        if (next == atoms[OffsetLowerX] || next == atoms[OffsetUpperX]) {
            parseHex = true;
            AdvanceChar(first);
            *ptr++ = 'x';
        } else {
            seenDigit = true;
            ++initialDecLeadingZero;
        }
    }

    bool hasUnaccumulatedDigits = false;
    int significant = 0;
    std::ptrdiff_t powerOfRepBase = 0;
    const std::size_t offsetDigitEnd = parseHex ? OffsetHexDigitEnd : OffsetDecDigitEnd;

    if (punct.grouping.empty()) {
        while (!InputEnded(first, last)) {
            const std::size_t idx = FindAtom(atoms, PeekChar(first));
            if (idx >= offsetDigitEnd) break;
            if (significant >= maxSigDig) {
                ++powerOfRepBase;
                if (idx > 0) hasUnaccumulatedDigits = true;
            } else if (idx != 0 || significant != 0) {
                *ptr++ = Src[idx];
                ++significant;
            }
            seenDigit = true;
            AdvanceChar(first);
        }
    } else {
        if (!seenDigit && SkipSeparators(first, last, kSeparator)) badGrouping = true;
        std::string groups(1, initialDecLeadingZero);
        std::size_t groupsArrIdx = 0;
        for (; !InputEnded(first, last); AdvanceChar(first)) {
            const TChar ch = PeekChar(first);
            const std::size_t idx = FindAtom(atoms, ch);
            if (idx < offsetDigitEnd) {
                seenDigit = true;
                if (significant >= maxSigDig) {
                    ++powerOfRepBase;
                    if (idx > 0) hasUnaccumulatedDigits = true;
                } else if (idx != 0 || significant != 0) {
                    *ptr++ = Src[idx];
                    ++significant;
                }
                if (groups[groupsArrIdx] != CHAR_MAX) ++groups[groupsArrIdx];
            } else if (ch != kSeparator) {
                break;
            } else if (groups[groupsArrIdx] == '\0') {
                badGrouping = true;
            } else {
                groups.push_back('\0');
                ++groupsArrIdx;
            }
        }
        if (groupsArrIdx != 0) {
            if (groups[groupsArrIdx] > '\0') ++groupsArrIdx;
            else badGrouping = true;
        }
        if (SkipSeparators(first, last, kSeparator)) badGrouping = true;
        if (seenDigit && !badGrouping && groupsArrIdx > 0) {
            badGrouping = ValidateGroups(punct.grouping, groups, groupsArrIdx);
        }
    }

    if (parseHex && seenDigit && significant == 0) *ptr++ = '0';

    const char cDecimalPoint = localeconv_nid_postfix()->decimal_point[0];
    if (!InputEnded(first, last) && PeekChar(first) == punct.decimalPoint) {
        *ptr++ = cDecimalPoint;
        AdvanceChar(first);
    }

    if (significant == 0) {
        while (!InputEnded(first, last) && PeekChar(first) == zeroWc) {
            --powerOfRepBase;
            seenDigit = true;
            AdvanceChar(first);
        }
    }

    while (!InputEnded(first, last)) {
        const std::size_t idx = FindAtom(atoms, PeekChar(first));
        if (idx >= offsetDigitEnd) break;
        if (significant < maxSigDig) {
            *ptr++ = Src[idx];
            ++significant;
        } else if (idx > 0) {
            hasUnaccumulatedDigits = true;
        }
        seenDigit = true;
        AdvanceChar(first);
    }

    if (parseHex && seenDigit && significant == 0 && ptr[-1] == cDecimalPoint) *ptr++ = '0';

    if (hasUnaccumulatedDigits) {
        char& lastGotDigit = ptr[-1] == cDecimalPoint ? ptr[-2] : ptr[-1];
        if (lastGotDigit == '0' || lastGotDigit == (parseHex ? '8' : '5')) ++lastGotDigit;
    }

    const TChar lowerExpWc = atoms[parseHex ? OffsetLowerP : OffsetLowerE];
    const TChar upperExpWc = atoms[parseHex ? OffsetUpperP : OffsetUpperE];
    bool exponentPartNegative = false;
    std::ptrdiff_t exponentPart = 0;
    if (seenDigit && !InputEnded(first, last) && (PeekChar(first) == lowerExpWc || PeekChar(first) == upperExpWc)) {
        AdvanceChar(first);
        seenDigit = false;
        if (!InputEnded(first, last)) {
            const TChar ch = PeekChar(first);
            if (ch == positiveSign) {
                AdvanceChar(first);
            } else if (ch == negativeSign) {
                exponentPartNegative = true;
                AdvanceChar(first);
            }
        }
        while (!InputEnded(first, last) && PeekChar(first) == zeroWc) {
            seenDigit = true;
            AdvanceChar(first);
        }
        while (!InputEnded(first, last)) {
            const std::size_t idx = FindAtom(atoms, PeekChar(first));
            if (idx >= OffsetDecDigitEnd) break;
            if (exponentPart < PTRDIFF_MAX / 10 || (exponentPart == PTRDIFF_MAX / 10 && static_cast<std::ptrdiff_t>(idx) <= PTRDIFF_MAX % 10)) {
                exponentPart = exponentPart * 10 + static_cast<std::ptrdiff_t>(idx);
            } else {
                exponentPart = PTRDIFF_MAX;
            }
            seenDigit = true;
            AdvanceChar(first);
        }
        if (exponentPartNegative) exponentPart = -exponentPart;
    }

    if (!seenDigit) return {0, false};

    const std::ptrdiff_t expAbsBound = parseHex ? HexExpAbsBound : DecExpAbsBound;
    const std::ptrdiff_t expRepAbsBound = parseHex ? HexExpAbsBound / 4 : DecExpAbsBound;
    for (std::ptrdiff_t powerAdjusted = powerOfRepBase;;) {
        if (exponentPart >= 0 && powerAdjusted >= 0 && (exponentPart >= expAbsBound || powerAdjusted >= expRepAbsBound)) {
            exponentPart = expAbsBound;
            break;
        }
        if (exponentPart <= 0 && powerAdjusted <= 0 && (exponentPart <= -expAbsBound || powerAdjusted <= -expRepAbsBound)) {
            exponentPart = -expAbsBound;
            break;
        }
        if (std::abs(exponentPart) <= expAbsBound && std::abs(powerAdjusted) <= expRepAbsBound) {
            exponentPart += parseHex ? powerAdjusted * 4 : powerAdjusted;
            exponentPart = std::clamp(exponentPart, -expAbsBound, expAbsBound);
            break;
        }
        const std::ptrdiff_t preadjustmentRoundUp = parseHex ? (std::abs(exponentPart) - 1) / 4 + 1 : std::abs(exponentPart);
        const std::ptrdiff_t adjustment = std::min(preadjustmentRoundUp, std::abs(powerAdjusted));
        if (exponentPart >= 0) {
            exponentPart -= parseHex ? adjustment * 4 : adjustment;
            powerAdjusted += adjustment;
        } else {
            exponentPart += parseHex ? adjustment * 4 : adjustment;
            powerAdjusted -= adjustment;
        }
    }

    if (exponentPart != 0) {
        *ptr++ = parseHex ? 'p' : 'e';
        if (exponentPart < 0) *ptr++ = '-';
        char* const revBegin = ptr;
        for (std::ptrdiff_t mag = std::abs(exponentPart); mag != 0; mag /= 10) *ptr++ = static_cast<char>('0' + mag % 10);
        std::reverse(revBegin, ptr);
    }

    *ptr = '\0';
    return {static_cast<std::int8_t>(parseHex ? 16 : 10), badGrouping};
}

bool HasNonzeroHexMantissa(const char* digits) {
    const char* cursor = digits;
    if (*cursor == '+' || *cursor == '-') ++cursor;
    if (cursor[0] != '0' || (cursor[1] != 'x' && cursor[1] != 'X')) return false;
    for (cursor += 2; *cursor != '\0' && *cursor != 'p' && *cursor != 'P'; ++cursor) {
        if ((*cursor >= '1' && *cursor <= '9') || (*cursor >= 'a' && *cursor <= 'f') || (*cursor >= 'A' && *cursor <= 'F')) return true;
    }
    return false;
}

template<typename TChar, typename TFloat>
GuestLocale::InputIterator DoGetFloat(GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, TFloat* value, TFloat (APS5_VABI *parse)(const char*, char**, std::int64_t, int*)) {
    ValidateNumGetArgs(stream, state, value);
    auto inFirst = FromGuestIterator<TChar>(first);
    auto inLast = FromGuestIterator<TChar>(last);
    char ac[FloatingBufferSize];
    const auto parsed = ParseFloatField(ac, MaxSigDigV2, inFirst, inLast, stream);
    if (parsed.base == 0) {
        *state = IosFailBit;
        *value = TFloat{0};
    } else {
        int err = 0;
        char* ep = nullptr;
        int& guestErrno = *__error_nid_postfix();
        const int savedErrno = guestErrno;
        const TFloat result = parse(ac, &ep, 0, &err);
        guestErrno = savedErrno;
        if (err == 0 && parsed.base == 16) {
            if (std::isinf(result)) err = 1;
            else if (result == TFloat{0} && HasNonzeroHexMantissa(ac)) err = 1;
        }
        *value = result;
        if (ep == ac || err != 0 || parsed.badGrouping) *state = IosFailBit;
    }
    if (InputEnded(inFirst, inLast)) *state |= IosEofBit;
    return ToGuestIterator(inFirst);
}

template<typename TChar>
int GetFloatingFieldExport(char* digits, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, GuestLocale::IosBase* stream, std::int32_t* hexExp) {
    if (digits == nullptr || first == nullptr || last == nullptr || stream == nullptr || hexExp == nullptr || stream->locale == nullptr || *stream->locale == nullptr) {
        throw std::invalid_argument("num_get::_Getffld: null argument");
    }
    const int maxSigDig = *hexExp == EnableV2Behavior ? MaxSigDigV2 : MaxSigDigV1;
    auto inFirst = FromGuestIterator<TChar>(*first);
    auto inLast = FromGuestIterator<TChar>(*last);
    const auto parsed = ParseFloatField(digits, maxSigDig, inFirst, inLast, stream);
    *first = ToGuestIterator(inFirst);
    if (parsed.base == 0 || parsed.badGrouping) *digits = '\0';
    if (parsed.base == 16) *hexExp = 0;
    return 0;
}

const GuestLocale::NumGetVtable& NumGet(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::NumGetVtable*>(self->vtable);
}

void APS5_VABI DestroyNumGetFacet(GuestLocale::Facet*) {}

void APS5_VABI DeleteNumGetFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("num_get delete: null facet");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI RetainNumGetFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("num_get retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseNumGetFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("num_get release: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

GuestLocale::InputIterator APS5_VABI CharNumGetBool(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    ValidateNumGetArgs(stream, state, value);
    auto inFirst = FromGuestIterator<char>(first);
    auto inLast = FromGuestIterator<char>(last);

    if ((stream->flags & IosBoolAlpha) == 0) {
        char digits[MaxIntDigits];
        const auto parsed = ParseIntField(digits, inFirst, inLast, stream);
        if (parsed.base < 0) {
            *value = false;
            *state = IosFailBit;
        } else {
            char* end = nullptr;
            int error = 0;
            int& guestErrno = *__error_nid_postfix();
            const int savedErrno = guestErrno;
            const std::int64_t parsedValue = _Stolx_nid_postfix(digits, &end, parsed.base, &error);
            guestErrno = savedErrno;

            if (parsed.badGrouping || end == digits || error != 0) {
                *value = true;
                *state = IosFailBit;
            } else {
                *value = parsedValue != 0;
                if (parsedValue < 0 || parsedValue > 1) *state = IosFailBit;
            }
        }
    } else {
        GuestLocale::GlobalLocaleHold hold;
        const GuestLocale::Facet* punct = FindFacet(stream, _ZNSt8numpunctIcE2idE_nid_postfix, hold);
        if (punct == nullptr) throw std::invalid_argument("num_get::do_get(bool): missing numpunct facet");

        const auto& vtable = *reinterpret_cast<const GuestLocale::NumpunctVtable*>(punct->vtable);
        const std::string falseName = TakePunctString(vtable.falsename, punct);
        const std::string trueName = TakePunctString(vtable.truename, punct);

        std::size_t falseOffset = 0;
        std::size_t trueOffset = 0;
        bool falsePossible = true;
        bool truePossible = true;

        while (!InputEnded(inFirst, inLast)) {
            const char character = PeekChar(inFirst);
            const bool falseMatches =
                falsePossible &&
                falseOffset < falseName.size() &&
                falseName[falseOffset] == character;
            const bool trueMatches =
                truePossible &&
                trueOffset < trueName.size() &&
                trueName[trueOffset] == character;

            if (!falseMatches && !trueMatches) break;

            falsePossible = falseMatches;
            truePossible = trueMatches;
            if (falseMatches) ++falseOffset;
            if (trueMatches) ++trueOffset;
            AdvanceChar(inFirst);

            const bool falseComplete = falsePossible && falseOffset == falseName.size();
            const bool trueComplete = truePossible && trueOffset == trueName.size();
            const bool falseContinues = falsePossible && falseOffset < falseName.size();
            const bool trueContinues = truePossible && trueOffset < trueName.size();

            if ((falseComplete || trueComplete) && !falseContinues && !trueContinues) break;
        }

        const bool falseMatch = falsePossible && falseOffset == falseName.size();
        const bool trueMatch = truePossible && trueOffset == trueName.size();

        *value = trueMatch && !falseMatch;
        if (falseMatch == trueMatch) *state = IosFailBit;
    }

    if (InputEnded(inFirst, inLast)) *state |= IosEofBit;
    return ToGuestIterator(inFirst);
}

template<typename TChar>
GuestLocale::InputIterator DoGetPointer(GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    ValidateNumGetArgs(stream, state, value);
    auto inFirst = FromGuestIterator<TChar>(first);
    auto inLast = FromGuestIterator<TChar>(last);
    GuestLocale::IosBase pointerStream = *stream;
    pointerStream.flags = (stream->flags & ~IosBaseField) | IosHex;
    char ac[MaxIntDigits];
    const auto parsed = ParseIntField(ac, inFirst, inLast, &pointerStream);
    if (parsed.base < 0 || ac[0] == '\0') {
        *state = IosFailBit;
        *value = nullptr;
    } else {
        char* ep = nullptr;
        int err = 0;
        int& guestErrno = *__error_nid_postfix();
        const int savedErrno = guestErrno;
        const std::uint64_t parsedValue = _Stoullx_nid_postfix(ac, &ep, parsed.base, &err);
        guestErrno = savedErrno;
        if (ep == ac || err != 0) {
            *state = IosFailBit;
            *value = nullptr;
        } else {
            *value = reinterpret_cast<void*>(static_cast<std::uintptr_t>(parsedValue));
            if (parsed.badGrouping) *state |= IosFailBit;
        }
    }
    if (InputEnded(inFirst, inLast)) *state |= IosEofBit;
    return ToGuestIterator(inFirst);
}

GuestLocale::InputIterator APS5_VABI CharNumGetPointer(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return DoGetPointer<char>(first, last, stream, state, value);
}

const GuestLocale::NumGetVtable g_charNumGetVtable{
    {DestroyNumGetFacet, DeleteNumGetFacet, RetainNumGetFacet, ReleaseNumGetFacet},
    CharNumGetBool,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
    CharNumGetPointer};

}

extern "C" {

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self).getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("OWO5cpNw3NA", sceLibcInternalNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self).getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("gozsp4urvq8", sceLibcInternalNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self).getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("N1VqUWz2OEI", sceLibcInternalNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("I2UzwkwwEPs", sceLibcInternalNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("2bfL3yIBi5k", sceLibcInternalNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("4hiQK82QuLc", sceLibcInternalNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("eZfFLyWCkvg", sceLibcInternalNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self).getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("6RGkooTERsE", sceLibcInternalNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self).getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("mAwXCpkWaYc", sceLibcInternalNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self).getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("wUCRGap1j0U", sceLibcInternalNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self).getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("my9ujasm6-0", sceLibcInternalNumGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self).getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("vW-nnV62ea4", sceLibcInternalWideNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self).getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("bZ+lKHGvOr8", sceLibcInternalWideNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self).getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("Y3hBU5FYmhM", sceLibcInternalWideNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("-m2YPwVCwJQ", sceLibcInternalWideNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("94ZLp2+AOq0", sceLibcInternalWideNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("cG5hQhjFGog", sceLibcInternalWideNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("banNSumaAZ0", sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self).getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("2mb8FYgER+E", sceLibcInternalWideNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self).getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("+hjXHfvy1Mg", sceLibcInternalWideNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self).getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("xLZr4GJRMLo", sceLibcInternalWideNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self).getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("zomvAQ5RFdA", sceLibcInternalWideNumGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetDoGetPointer_nid_no_patch(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return DoGetPointer<char>(first, last, stream, state, value);
}
APS5_EXPORT("VKdXFE7ualw", sceLibcInternalNumGetDoGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetDoGetPointer_nid_no_patch(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return DoGetPointer<std::uint16_t>(first, last, stream, state, value);
}
APS5_EXPORT("BNZq-mRvDS8", sceLibcInternalWideNumGetDoGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return DoGetFloat<char>(first, last, stream, state, value, _Stodx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return DoGetFloat<char>(first, last, stream, state, value, _Stoldx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return DoGetFloat<char>(first, last, stream, state, value, _Stofx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return DoGetBoundedUnsigned<char>(first, last, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return DoGetBoundedUnsigned<char>(first, last, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return DoGetFullInt<char>(first, last, stream, state, value, _Stolx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return DoGetFullInt<char>(first, last, stream, state, value, _Stoulx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return DoGetFullInt<char>(first, last, stream, state, value, _Stollx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return DoGetFullInt<char>(first, last, stream, state, value, _Stoullx_nid_postfix);
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(const GuestLocale::Facet*, char* digits, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, GuestLocale::IosBase* stream, std::int32_t* hexExp) {
    return GetFloatingFieldExport<char>(digits, first, last, stream, hexExp);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return DoGetFloat<std::uint16_t>(first, last, stream, state, value, _Stodx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return DoGetFloat<std::uint16_t>(first, last, stream, state, value, _Stoldx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return DoGetFloat<std::uint16_t>(first, last, stream, state, value, _Stofx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return DoGetBoundedUnsigned<std::uint16_t>(first, last, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return DoGetFullInt<std::uint16_t>(first, last, stream, state, value, _Stolx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return DoGetFullInt<std::uint16_t>(first, last, stream, state, value, _Stoulx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return DoGetBoundedUnsigned<std::uint16_t>(first, last, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return DoGetFullInt<std::uint16_t>(first, last, stream, state, value, _Stollx_nid_postfix);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return DoGetFullInt<std::uint16_t>(first, last, stream, state, value, _Stoullx_nid_postfix);
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(const GuestLocale::Facet*, char* digits, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, GuestLocale::IosBase* stream, std::int32_t* hexExp) {
    return GetFloatingFieldExport<std::uint16_t>(digits, first, last, stream, hexExp);
}

std::size_t APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        *object = {&g_charNumGetVtable.facet, 0, 0};
        *facet = object;
    }
    return 4;
}

}
