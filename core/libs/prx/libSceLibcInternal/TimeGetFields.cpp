/* Derived from Microsoft STL stl/inc/xloctime and stl/inc/xlocale (770da5aa457af11f0a796600e1aa6f7f6fce14a6) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
extern std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
extern std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix;
}

namespace {

void RequireArgument(bool condition, const char* caller) {
    if (!condition) NotImplemented_nid_no_patch(caller);
}

static_assert(sizeof(char16_t) == sizeof(std::uint16_t));
static_assert(sizeof(GuestLocale::InputIterator) == 0x10);
static_assert(offsetof(GuestLocale::Streambuf, reserved) + 6 * sizeof(void*) == 0x38);
static_assert(offsetof(GuestLocale::Streambuf, reservedCounts) + 2 * sizeof(void*) == 0x58);

constexpr std::size_t StreambufUnderflowSlot = 7;
constexpr std::size_t StreambufUflowSlot = 8;
static_assert(StreambufUnderflowSlot * sizeof(std::uintptr_t) == 0x38);
static_assert(StreambufUflowSlot * sizeof(std::uintptr_t) == 0x40);

constexpr std::int32_t IosEofBit = 0x1;
constexpr std::int32_t IosFailBit = 0x2;
constexpr short CtypeDigitMask = 0x20;
constexpr short CtypeSpaceMask = 0x44;

GuestLocale::Streambuf* IteratorBuffer(const GuestLocale::InputIterator& it) {
    return reinterpret_cast<GuestLocale::Streambuf*>(it.words[0]);
}

bool IteratorGot(const GuestLocale::InputIterator& it) {
    return (it.words[1] & 0xffu) != 0;
}

template<typename TChar>
TChar IteratorValue(const GuestLocale::InputIterator& it) {
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<TChar>(static_cast<unsigned char>((it.words[1] >> 8) & 0xffu));
    } else {
        return static_cast<TChar>((it.words[1] >> 16) & 0xffffu);
    }
}

void SetIteratorEof(GuestLocale::InputIterator& it) {
    it.words[0] = 0;
    it.words[1] = 0;
}

template<typename TChar>
void SetIteratorCached(GuestLocale::InputIterator& it, TChar value) {
    if constexpr (sizeof(TChar) == 1) {
        it.words[1] = 1ull | (static_cast<std::uint64_t>(static_cast<unsigned char>(value)) << 8);
    } else {
        it.words[1] = 1ull | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(value)) << 16);
    }
}

char** StreambufGetNext(GuestLocale::Streambuf* buffer) {
    return reinterpret_cast<char**>(const_cast<void*>(buffer->reserved[6]));
}

char** StreambufGetEnd(GuestLocale::Streambuf* buffer) {
    return reinterpret_cast<char**>(const_cast<void*>(buffer->reservedCounts[2]));
}

int CallStreambufVirtual(GuestLocale::Streambuf* buffer, std::size_t slot) {
    if (buffer->vtable == nullptr) return -1;
    const auto* slots = reinterpret_cast<const std::uintptr_t*>(buffer->vtable);
    using VirtualGet = int (APS5_VABI*)(GuestLocale::Streambuf*);
    const auto fn = reinterpret_cast<VirtualGet>(slots[slot]);
    return fn != nullptr ? fn(buffer) : -1;
}

template<typename TChar>
TChar Peek(GuestLocale::InputIterator& it) {
    if (IteratorGot(it)) return IteratorValue<TChar>(it);
    GuestLocale::Streambuf* buffer = IteratorBuffer(it);
    if (buffer == nullptr) {
        SetIteratorEof(it);
        return TChar{};
    }
    char** getNext = StreambufGetNext(buffer);
    char** getEnd = StreambufGetEnd(buffer);
    if (getNext != nullptr && *getNext != nullptr && getEnd != nullptr && *getEnd != nullptr &&
        (*getEnd - *getNext) >= static_cast<std::ptrdiff_t>(sizeof(TChar))) {
        TChar value{};
        std::memcpy(&value, *getNext, sizeof(TChar));
        SetIteratorCached<TChar>(it, value);
        return value;
    }
    const int ch = CallStreambufVirtual(buffer, StreambufUnderflowSlot);
    if (ch == -1) {
        SetIteratorEof(it);
        return TChar{};
    }
    const TChar value = static_cast<TChar>(ch);
    SetIteratorCached<TChar>(it, value);
    return value;
}

template<typename TChar>
void Inc(GuestLocale::InputIterator& it) {
    GuestLocale::Streambuf* buffer = IteratorBuffer(it);
    if (buffer == nullptr) return;
    char** getNext = StreambufGetNext(buffer);
    char** getEnd = StreambufGetEnd(buffer);
    if (getNext != nullptr && *getNext != nullptr && getEnd != nullptr && *getEnd != nullptr &&
        (*getEnd - *getNext) >= static_cast<std::ptrdiff_t>(sizeof(TChar))) {
        *getNext += sizeof(TChar);
        it.words[1] = 0;
        return;
    }
    if (CallStreambufVirtual(buffer, StreambufUflowSlot) == -1) {
        SetIteratorEof(it);
    } else {
        it.words[1] = 0;
    }
}

template<typename TChar>
bool Equal(GuestLocale::InputIterator& first, GuestLocale::InputIterator& last) {
    if (!IteratorGot(first)) Peek<TChar>(first);
    if (!IteratorGot(last)) Peek<TChar>(last);
    return (IteratorBuffer(first) == nullptr) == (IteratorBuffer(last) == nullptr);
}

std::uint64_t EnsureFacetId(std::uint64_t& id) {
    std::atomic_ref<std::uint64_t> slot(id);
    if (slot.load() == 0) {
        _Locksyslock_nid_postfix();
        if (slot.load() == 0) slot.store(static_cast<std::uint64_t>(++_ZNSt6locale2id7_Id_cntE_nid_postfix));
        _Unlocksyslock_nid_postfix();
    }
    return slot.load();
}

const GuestLocale::Facet* FindFacet(const GuestLocale::IosBase* stream, std::uint64_t& id, GuestLocale::GlobalLocaleHold& hold) {
    const std::uint64_t index = EnsureFacetId(id);
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) return nullptr;
    const GuestLocale::Implementation* locale = *stream->locale;
    const GuestLocale::Facet* facet = index < locale->facetCount ? locale->facets[index] : nullptr;
    if (facet != nullptr || !locale->transparent) return facet;
    return hold.Find(index);
}

const GuestLocale::Facet* FindGlobalFacet(std::uint64_t& id, GuestLocale::GlobalLocaleHold& hold) {
    const std::uint64_t index = EnsureFacetId(id);
    return hold.Find(index);
}

template<typename TChar>
struct CtypeTraits;

template<>
struct CtypeTraits<char> {
    static const GuestLocale::Facet* FromStream(const GuestLocale::IosBase* stream, GuestLocale::GlobalLocaleHold& hold) {
        return FindFacet(stream, _ZNSt5ctypeIcE2idE_nid_postfix, hold);
    }

    static const GuestLocale::Facet* FromGlobal(GuestLocale::GlobalLocaleHold& hold) {
        return FindGlobalFacet(_ZNSt5ctypeIcE2idE_nid_postfix, hold);
    }

    static bool Is(const GuestLocale::Facet* ctype, short mask, char ch) {
        if (ctype != nullptr) {
            const auto* ct = reinterpret_cast<const GuestLocale::CtypeFacet*>(ctype);
            const short* table = ct->classification != nullptr ? ct->classification : _Getpctype_nid_postfix();
            return (table[static_cast<unsigned char>(ch)] & mask) != 0;
        }
        return (_Getpctype_nid_postfix()[static_cast<unsigned char>(ch)] & mask) != 0;
    }

    static char Narrow(const GuestLocale::Facet* ctype, char ch, char fallback) {
        if (ctype != nullptr && ctype->vtable != nullptr) {
            const auto* vt = reinterpret_cast<const GuestLocale::CtypeVtable*>(ctype->vtable);
            if (vt->narrow != nullptr) return vt->narrow(ctype, ch, fallback);
        }
        return ch;
    }

    static char ToLower(const GuestLocale::Facet* ctype, char ch) {
        if (ctype != nullptr) {
            if (ctype->vtable != nullptr) {
                const auto* vt = reinterpret_cast<const GuestLocale::CtypeVtable*>(ctype->vtable);
                if (vt->caseMethods[0] != nullptr) {
                    using DoToLower = char (APS5_VABI*)(const GuestLocale::Facet*, char);
                    return reinterpret_cast<DoToLower>(const_cast<void*>(vt->caseMethods[0]))(ctype, ch);
                }
            }
            const auto* ct = reinterpret_cast<const GuestLocale::CtypeFacet*>(ctype);
            if (ct->lower != nullptr) return static_cast<char>(ct->lower[static_cast<unsigned char>(ch)]);
        }
        return static_cast<char>(_Getptolower_nid_postfix()[static_cast<unsigned char>(ch)]);
    }
};

template<>
struct CtypeTraits<std::uint16_t> {
    static const GuestLocale::Facet* FromStream(const GuestLocale::IosBase* stream, GuestLocale::GlobalLocaleHold& hold) {
        return FindFacet(stream, _ZNSt5ctypeIwE2idE_nid_postfix, hold);
    }

    static const GuestLocale::Facet* FromGlobal(GuestLocale::GlobalLocaleHold& hold) {
        return FindGlobalFacet(_ZNSt5ctypeIwE2idE_nid_postfix, hold);
    }

    static bool Is(const GuestLocale::Facet* ctype, short mask, std::uint16_t ch) {
        if (ctype != nullptr && ctype->vtable != nullptr) {
            const auto* vt = reinterpret_cast<const GuestLocale::WideCtypeVtable*>(ctype->vtable);
            if (vt->classificationMethods[0] != nullptr) {
                using DoIs = bool (APS5_VABI*)(const GuestLocale::Facet*, short, std::uint16_t);
                return reinterpret_cast<DoIs>(const_cast<void*>(vt->classificationMethods[0]))(ctype, mask, ch);
            }
        }
        const int narrowed = _Wctob_nid_postfix(ch);
        return narrowed != -1 && (_Getpctype_nid_postfix()[static_cast<unsigned char>(narrowed)] & mask) != 0;
    }

    static char Narrow(const GuestLocale::Facet* ctype, std::uint16_t ch, char fallback) {
        if (ctype != nullptr && ctype->vtable != nullptr) {
            const auto* vt = reinterpret_cast<const GuestLocale::WideCtypeVtable*>(ctype->vtable);
            if (vt->narrow != nullptr) return vt->narrow(ctype, ch, fallback);
        }
        const int narrowed = _Wctob_nid_postfix(ch);
        return narrowed == -1 ? fallback : static_cast<char>(narrowed);
    }

    static std::uint16_t ToLower(const GuestLocale::Facet* ctype, std::uint16_t ch) {
        if (ctype != nullptr && ctype->vtable != nullptr) {
            const auto* vt = reinterpret_cast<const GuestLocale::WideCtypeVtable*>(ctype->vtable);
            if (vt->caseMethods[0] != nullptr) {
                using DoToLower = std::uint16_t (APS5_VABI*)(const GuestLocale::Facet*, std::uint16_t);
                return reinterpret_cast<DoToLower>(const_cast<void*>(vt->caseMethods[0]))(ctype, ch);
            }
        }
        return static_cast<std::uint16_t>(_Towlower_nid_postfix(ch, nullptr));
    }
};

template<typename TChar>
std::int32_t GetIntV2(GuestLocale::InputIterator& first, GuestLocale::InputIterator& last, int lo, int hi, int& val, int& digitsRead, const GuestLocale::Facet* ctype) {
    int hiDigits = 1;
    if (hi >= 10) {
        for (int limit = hi; limit >= 10; limit /= 10) ++hiDigits;
    }
    char ac[32];
    char* ptr = ac;
    digitsRead = 0;

    while (!Equal<TChar>(first, last) && digitsRead < hiDigits && CtypeTraits<TChar>::Is(ctype, CtypeSpaceMask, Peek<TChar>(first))) {
        Inc<TChar>(first);
        ++digitsRead;
    }

    if (!Equal<TChar>(first, last) && digitsRead < hiDigits) {
        const char sign = CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0');
        if (sign == '+' || sign == '-') {
            *ptr++ = sign;
            Inc<TChar>(first);
        }
    }

    while (!Equal<TChar>(first, last) && digitsRead < hiDigits &&
           CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0') == '0') {
        Inc<TChar>(first);
        ++digitsRead;
    }

    if (digitsRead > 0) *ptr++ = '0';

    char* const endLimit = &ac[sizeof(ac) - 1];
    while (!Equal<TChar>(first, last) && digitsRead < hiDigits) {
        const char ch = CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0');
        if (ch < '0' || ch > '9') break;
        if (ptr < endLimit) *ptr++ = ch;
        ++digitsRead;
        Inc<TChar>(first);
    }

    if (digitsRead == 0) ptr = ac;

    *ptr = '\0';
    char* endPtr = nullptr;
    int errnoVal = 0;
    const std::int64_t ans = _Stolx_nid_postfix(ac, &endPtr, 10, &errnoVal);
    std::int32_t state = 0;
    if (Equal<TChar>(first, last)) state |= IosEofBit;
    if (endPtr == ac || errnoVal != 0 || ans < lo || ans > hi) {
        state |= IosFailBit;
    } else {
        val = static_cast<int>(ans);
    }
    return state;
}

inline constexpr char kCharDays[] = ":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday";
inline constexpr char kCharMonths[] = ":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December";
inline constexpr char kCharAmPm[] = ":AM:am:PM:pm";

template<typename TChar>
inline TChar FoldAscii(TChar ch) {
    if constexpr (sizeof(TChar) == 1) {
        const auto u = static_cast<unsigned char>(ch);
        if (u >= 'A' && u <= 'Z') {
            return static_cast<TChar>(u + ('a' - 'A'));
        }
        return ch;
    } else {
        if (ch >= u'A' && ch <= u'Z') {
            return static_cast<TChar>(ch + (u'a' - u'A'));
        }
        return ch;
    }
}

template<typename TChar>
bool IsAtEnd(GuestLocale::InputIterator& first, const GuestLocale::InputIterator* last) {
    if (IteratorBuffer(first) == nullptr) {
        return true;
    }
    if (last == nullptr || IteratorBuffer(*last) == nullptr) {
        if (!IteratorGot(first)) {
            Peek<TChar>(first);
        }
        return IteratorBuffer(first) == nullptr;
    }
    return Equal<TChar>(first, const_cast<GuestLocale::InputIterator&>(*last));
}

template<typename TChar>
int GetLocaleText(GuestLocale::InputIterator& first, const GuestLocale::InputIterator* last, std::size_t numFields, const TChar* ptr) {
    if (IteratorBuffer(first) == nullptr || ptr == nullptr) {
        return -2;
    }
    const TChar sep = ptr[0];
    const bool nulMarked = sep == TChar{};
    std::vector<std::size_t> fieldStarts;
    for (std::size_t off = 0; ptr[off] != TChar{}; ++off) {
        if (ptr[off] == sep) {
            if (numFields == SIZE_MAX) {
                throw std::length_error("string too long");
            }
            ++numFields;
            fieldStarts.push_back(off);
        }
    }
    if (numFields == 0) {
        return -2;
    }
    std::size_t nulOff = 0;
    while (ptr[nulOff] != TChar{}) {
        ++nulOff;
    }
    while (fieldStarts.size() < numFields) {
        fieldStarts.push_back(nulOff);
        if (nulMarked) {
            do {
                ++nulOff;
            } while (ptr[nulOff] != TChar{});
        }
    }

    std::string skip(numFields, '\0');
    int ans = -2;

    for (std::size_t col = 1; col < 128; ++col) {
        bool prefix = false;
        for (std::size_t field = 0; field < numFields; ++field) {
            if (skip[field] != '\0') {
                continue;
            }
            const std::size_t charOff = fieldStarts[field] + col;
            const TChar tableChar = (charOff >= nulOff) ? TChar{} : ptr[charOff];
            if (tableChar == sep || tableChar == TChar{}) {
                skip[field] = static_cast<char>(col);
                ans = static_cast<int>(field);
            } else if (IsAtEnd<TChar>(first, last) ||
                       FoldAscii(tableChar) != FoldAscii(Peek<TChar>(first))) {
                skip[field] = static_cast<char>(col);
            } else {
                prefix = true;
            }
        }
        if (!prefix || IsAtEnd<TChar>(first, last)) {
            break;
        }
        Inc<TChar>(first);
        ans = -1;
    }
    return ans;
}

template<typename TChar>
int GetLocaleText(GuestLocale::InputIterator& first, GuestLocale::InputIterator& last, std::size_t numFields, const TChar* ptr) {
    return GetLocaleText<TChar>(first, &last, numFields, ptr);
}

template<typename TChar>
GuestLocale::InputIterator DoGetTimeImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr, caller);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<TChar>::FromStream(stream, hold);
    int dummyDigits = 0;
    *state |= GetIntV2<TChar>(first, last, 0, 23, time->tm_hour, dummyDigits, ctype);

    if (*state != 0 || CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0') != ':') {
        *state |= IosFailBit;
    } else {
        Inc<TChar>(first);
        *state |= GetIntV2<TChar>(first, last, 0, 59, time->tm_min, dummyDigits, ctype);
    }

    if (*state != 0 || CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0') != ':') {
        *state |= IosFailBit;
    } else {
        Inc<TChar>(first);
        *state |= GetIntV2<TChar>(first, last, 0, 60, time->tm_sec, dummyDigits, ctype);
    }

    return first;
}

template<typename TChar>
GuestLocale::InputIterator DoGetYearImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr, caller);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<TChar>::FromStream(stream, hold);
    int ans = 0;
    int digitsRead = 0;
    const std::int32_t res = GetIntV2<TChar>(first, last, 0, 9999, ans, digitsRead, ctype);
    *state |= res;
    if ((res & IosFailBit) == 0) {
        if (digitsRead <= 2) {
            time->tm_year = ans < 69 ? ans + 100 : ans;
        } else {
            time->tm_year = ans - 1900;
        }
    }
    return first;
}

template<typename TChar>
GuestLocale::InputIterator DoGetNameFieldImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, std::int32_t* state, GuestTm* time, std::int32_t GuestTm::*field, const TChar* table, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr && table != nullptr, caller);
    const int num = GetLocaleText<TChar>(first, last, 0, table);
    if (num < 0) {
        *state |= IosFailBit;
    } else {
        time->*field = num >> 1;
    }
    return first;
}

inline constexpr std::int32_t DateOrderNoOrder = 0;
inline constexpr std::int32_t DateOrderDmy = 1;
inline constexpr std::int32_t DateOrderMdy = 2;
inline constexpr std::int32_t DateOrderYmd = 3;
inline constexpr std::int32_t DateOrderYdm = 4;

struct GuestTimeGetWideFacet {
    GuestLocale::Facet base;
    const char16_t* days;
    const char16_t* months;
    const char16_t* amPm;
    std::int32_t dateOrder;
    std::uint32_t reserved;
};

static_assert(offsetof(GuestTimeGetWideFacet, days) == 0x10);
static_assert(offsetof(GuestTimeGetWideFacet, months) == 0x18);
static_assert(offsetof(GuestTimeGetWideFacet, amPm) == 0x20);
static_assert(offsetof(GuestTimeGetWideFacet, dateOrder) == 0x28);
static_assert(sizeof(GuestTimeGetWideFacet) == 0x30);

struct GuestTimePutCharFacet {
    GuestLocale::Facet base;
    GuestLocale::TimeInfo names;
};

static_assert(offsetof(GuestTimePutCharFacet, names) == 0x10);
static_assert(sizeof(GuestTimePutCharFacet) == 0xb8);

inline constexpr char16_t kWideDays[] = u":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday";
inline constexpr char16_t kWideMonths[] = u":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December";
inline constexpr char16_t kWideAmPm[] = u":AM:am:PM:pm";

template<typename TChar>
std::int32_t GetIntImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, int lo, int hi, int* val, const GuestLocale::Facet* ctype, const char* caller) {
    RequireArgument(self != nullptr && first != nullptr && last != nullptr && val != nullptr && ctype != nullptr, caller);
    int digitsRead = 0;
    return GetIntV2<TChar>(*first, *last, lo, hi, *val, digitsRead, ctype);
}

template<typename TChar>
void SkipDateSeparator(GuestLocale::InputIterator& first, GuestLocale::InputIterator& last, const GuestLocale::Facet* ctype) {
    while (!Equal<TChar>(first, last) && CtypeTraits<TChar>::Is(ctype, CtypeSpaceMask, Peek<TChar>(first))) {
        Inc<TChar>(first);
    }
    if (!Equal<TChar>(first, last)) {
        const char ch = CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0');
        if (ch == ':' || ch == ',' || ch == '/') Inc<TChar>(first);
    }
    while (!Equal<TChar>(first, last) && CtypeTraits<TChar>::Is(ctype, CtypeSpaceMask, Peek<TChar>(first))) {
        Inc<TChar>(first);
    }
}

template<typename TChar>
void ParseDateMonth(GuestLocale::InputIterator& first, GuestLocale::InputIterator& last, std::int32_t* state, GuestTm* time, const GuestLocale::Facet* ctype) {
    int dummyDigits = 0;
    *state |= GetIntV2<TChar>(first, last, 1, 12, time->tm_mon, dummyDigits, ctype);
    --time->tm_mon;
}

template<typename TChar>
GuestLocale::InputIterator DoGetDateImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr, caller);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<TChar>::FromStream(stream, hold);
    const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(self->vtable);
    RequireArgument(vtable != nullptr && vtable->dateOrder != nullptr && vtable->getMonthname != nullptr && vtable->getYear != nullptr, caller);
    int dummyDigits = 0;
    std::int32_t dorder = vtable->dateOrder(self);
    if (dorder == DateOrderNoOrder) dorder = DateOrderMdy;

    if (!Equal<TChar>(first, last)) {
        if (!CtypeTraits<TChar>::Is(ctype, CtypeDigitMask, Peek<TChar>(first))) {
            first = vtable->getMonthname(self, first, last, stream, state, time);
            dorder = DateOrderMdy;
        } else if (dorder == DateOrderMdy) {
            ParseDateMonth<TChar>(first, last, state, time, ctype);
        } else if (dorder == DateOrderDmy) {
            *state |= GetIntV2<TChar>(first, last, 1, 31, time->tm_mday, dummyDigits, ctype);
        } else {
            first = vtable->getYear(self, first, last, stream, state, time);
        }
    }

    SkipDateSeparator<TChar>(first, last, ctype);

    if (!Equal<TChar>(first, last)) {
        if (!CtypeTraits<TChar>::Is(ctype, CtypeDigitMask, Peek<TChar>(first))) {
            if (dorder == DateOrderMdy) {
                *state |= IosFailBit;
            } else {
                first = vtable->getMonthname(self, first, last, stream, state, time);
                if (dorder == DateOrderYdm) dorder = DateOrderYmd;
            }
        } else if (dorder == DateOrderDmy || dorder == DateOrderYmd) {
            ParseDateMonth<TChar>(first, last, state, time, ctype);
        } else {
            *state |= GetIntV2<TChar>(first, last, 1, 31, time->tm_mday, dummyDigits, ctype);
        }
    }

    SkipDateSeparator<TChar>(first, last, ctype);

    if (Equal<TChar>(first, last)) {
        *state |= IosFailBit;
    } else if (!CtypeTraits<TChar>::Is(ctype, CtypeDigitMask, Peek<TChar>(first))) {
        if (dorder != DateOrderYdm) {
            *state |= IosFailBit;
        } else {
            first = vtable->getMonthname(self, first, last, stream, state, time);
        }
    } else if (dorder == DateOrderYdm) {
        ParseDateMonth<TChar>(first, last, state, time, ctype);
    } else if (dorder == DateOrderYmd) {
        *state |= GetIntV2<TChar>(first, last, 1, 31, time->tm_mday, dummyDigits, ctype);
    } else {
        first = vtable->getYear(self, first, last, stream, state, time);
    }

    if (Equal<TChar>(first, last)) *state |= IosEofBit;
    return first;
}

template<typename TChar>
GuestLocale::InputIterator DoGetFmtImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* fmtfirst, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr && fmtfirst != nullptr, caller);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<TChar>::FromStream(stream, hold);
    const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(self->vtable);

    for (; *fmtfirst != '\0'; ++fmtfirst) {
        if (Equal<TChar>(first, last)) {
            *state |= IosFailBit;
            break;
        }
        if (*fmtfirst == '%') {
            if (*++fmtfirst == '\0') {
                *state |= IosFailBit;
                break;
            }
            RequireArgument(vtable != nullptr && vtable->get != nullptr, caller);
            first = vtable->get(self, first, last, stream, state, time, *fmtfirst, '\0');
        } else if (*fmtfirst == ' ') {
            while (!Equal<TChar>(first, last) && CtypeTraits<TChar>::Is(ctype, CtypeSpaceMask, Peek<TChar>(first))) {
                Inc<TChar>(first);
            }
        } else if (CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0') != *fmtfirst) {
            *state |= IosFailBit;
            break;
        } else {
            Inc<TChar>(first);
        }
    }
    if (Equal<TChar>(first, last)) *state |= IosEofBit;
    return first;
}

template<typename TChar>
GuestLocale::InputIterator DoGetImpl(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, const TChar* amPmTable, const char* caller) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr && amPmTable != nullptr, caller);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<TChar>::FromStream(stream, hold);
    const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(self->vtable);
    int ans = 0;
    int dummyDigits = 0;
    *state = 0;

    switch (specifier) {
    case 'a':
    case 'A':
        RequireArgument(vtable != nullptr && vtable->getWeekday != nullptr, caller);
        first = vtable->getWeekday(self, first, last, stream, state, time);
        break;
    case 'b':
    case 'B':
    case 'h':
        RequireArgument(vtable != nullptr && vtable->getMonthname != nullptr, caller);
        first = vtable->getMonthname(self, first, last, stream, state, time);
        break;
    case 'c':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, "%a %b %e %T %Y", caller);
        break;
    case 'C':
        *state |= GetIntV2<TChar>(first, last, 0, 99, ans, dummyDigits, ctype);
        if ((*state & IosFailBit) == 0) time->tm_year = ans * 100 - 1900;
        break;
    case 'd':
    case 'e':
        *state |= GetIntV2<TChar>(first, last, 1, 31, time->tm_mday, dummyDigits, ctype);
        break;
    case 'D':
    case 'x':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, "%m / %d / %y", caller);
        break;
    case 'H':
        *state |= GetIntV2<TChar>(first, last, 0, 23, time->tm_hour, dummyDigits, ctype);
        break;
    case 'I':
        *state |= GetIntV2<TChar>(first, last, 1, 12, ans, dummyDigits, ctype);
        if ((*state & IosFailBit) == 0) time->tm_hour = ans == 12 ? 0 : ans;
        break;
    case 'j':
        *state |= GetIntV2<TChar>(first, last, 1, 366, time->tm_yday, dummyDigits, ctype);
        break;
    case 'm':
        *state |= GetIntV2<TChar>(first, last, 1, 12, ans, dummyDigits, ctype);
        if ((*state & IosFailBit) == 0) time->tm_mon = ans - 1;
        break;
    case 'M':
        *state |= GetIntV2<TChar>(first, last, 0, 59, time->tm_min, dummyDigits, ctype);
        break;
    case 'n':
    case 't':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, " ", caller);
        break;
    case 'p':
        ans = GetLocaleText<TChar>(first, last, 0, amPmTable);
        if (ans < 0) {
            *state |= IosFailBit;
        } else if (ans > 1) {
            time->tm_hour += 12;
        }
        break;
    case 'r':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, "%I : %M : %S %p", caller);
        break;
    case 'R':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, "%H : %M", caller);
        break;
    case 'S':
        *state |= GetIntV2<TChar>(first, last, 0, 60, time->tm_sec, dummyDigits, ctype);
        break;
    case 'T':
    case 'X':
        first = DoGetFmtImpl<TChar>(self, first, last, stream, state, time, "%H : %M : %S", caller);
        break;
    case 'U':
    case 'W':
        *state |= GetIntV2<TChar>(first, last, 0, 53, time->tm_yday, dummyDigits, ctype);
        break;
    case 'w':
        *state |= GetIntV2<TChar>(first, last, 0, 6, time->tm_wday, dummyDigits, ctype);
        break;
    case 'y':
        *state |= GetIntV2<TChar>(first, last, 0, 99, ans, dummyDigits, ctype);
        if ((*state & IosFailBit) == 0) time->tm_year = ans < 69 ? ans + 100 : ans;
        break;
    case 'Y':
        *state |= GetIntV2<TChar>(first, last, 0, 9999, ans, dummyDigits, ctype);
        if ((*state & IosFailBit) == 0) time->tm_year = ans - 1900;
        break;
    case '%':
        if (Equal<TChar>(first, last) || CtypeTraits<TChar>::Narrow(ctype, Peek<TChar>(first), '\0') != '%') {
            *state |= IosFailBit;
        } else {
            Inc<TChar>(first);
        }
        break;
    default:
        *state |= IosFailBit;
        break;
    }

    if (Equal<TChar>(first, last)) *state |= IosEofBit;
    return first;
}

void APS5_VABI TimeFieldsDestroyFacet_nid_no_patch(GuestLocale::Facet*) {}

void APS5_VABI TimeFieldsDeleteFacet_nid_no_patch(GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI TimeFieldsDestroyWideTimeGet_nid_no_patch(GuestLocale::Facet* self) {
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(self);
}

void APS5_VABI TimeFieldsDeleteWideTimeGet_nid_no_patch(GuestLocale::Facet* self) {
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI TimeFieldsRetainFacet_nid_no_patch(GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI TimeFieldsReleaseFacet_nid_no_patch(GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

std::int32_t APS5_VABI TimeFieldsWideDateOrder_nid_no_patch(const GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
    return reinterpret_cast<const GuestTimeGetWideFacet*>(self)->dateOrder;
}

GuestLocale::OutputIterator APS5_VABI TimeFieldsCharDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    return LibcTimePutDoPut_nid_no_patch(self, out, stream, fill, time, format, modifier);
}

GuestLocale::OutputIterator APS5_VABI TimeFieldsWideDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    return LibcWideTimePutDoPut_nid_no_patch(self, out, stream, fill, time, format, modifier);
}

const GuestLocale::TimeGetVtable g_wideTimeGetVtable{
    {TimeFieldsDestroyWideTimeGet_nid_no_patch, TimeFieldsDeleteWideTimeGet_nid_no_patch, TimeFieldsRetainFacet_nid_no_patch, TimeFieldsReleaseFacet_nid_no_patch},
    TimeFieldsWideDateOrder_nid_no_patch,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix,
};

const std::uint16_t* WideFieldTable(const GuestLocale::Facet* self, const char16_t* GuestTimeGetWideFacet::*field, const char16_t* fallback) {
    if (self != nullptr && self->vtable == &g_wideTimeGetVtable.facet) {
        return reinterpret_cast<const std::uint16_t*>(reinterpret_cast<const GuestTimeGetWideFacet*>(self)->*field);
    }
    return reinterpret_cast<const std::uint16_t*>(fallback);
}

const GuestLocale::TimePutVtable g_timePutCharVtable{
    {TimeFieldsDestroyFacet_nid_no_patch, TimeFieldsDeleteFacet_nid_no_patch, TimeFieldsRetainFacet_nid_no_patch, TimeFieldsReleaseFacet_nid_no_patch},
    TimeFieldsCharDoPut_nid_no_patch,
};

const GuestLocale::WideTimePutVtable g_timePutWideVtable{
    {TimeFieldsDestroyFacet_nid_no_patch, TimeFieldsDeleteFacet_nid_no_patch, TimeFieldsRetainFacet_nid_no_patch, TimeFieldsReleaseFacet_nid_no_patch},
    TimeFieldsWideDoPut_nid_no_patch,
};

struct ScopedClassicLocinfo {
    GuestLocale::LocinfoStorage storage{};
    bool active = false;

    explicit ScopedClassicLocinfo(const char* localeName) {
        _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, localeName);
        active = true;
    }

    ~ScopedClassicLocinfo() {
        if (active) _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
    }
};

GuestLocale::Facet MakeFacetHeader(const GuestLocale::FacetVtable* vtable, std::size_t refs) {
    return {vtable, static_cast<std::uint32_t>(refs), static_cast<std::uint32_t>(static_cast<std::uint64_t>(refs) >> 32)};
}

void InitWideTimeGetFacet(GuestLocale::Facet* self, std::size_t refs) {
    auto* facet = reinterpret_cast<GuestTimeGetWideFacet*>(self);
    facet->base = MakeFacetHeader(&g_wideTimeGetVtable.facet, refs);
    facet->days = kWideDays;
    facet->months = kWideMonths;
    facet->amPm = kWideAmPm;
    facet->dateOrder = DateOrderMdy;
    facet->reserved = 0;
}

void ConstructWideTimeGetFacet(GuestLocale::Facet* self, const char* localeName, std::size_t refs, const char* caller) {
    RequireArgument(self != nullptr, caller);
    ScopedClassicLocinfo locinfo(localeName);
    InitWideTimeGetFacet(self, refs);
}

void ConstructWideTimeGetFromLocinfo(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs, const char* caller) {
    RequireArgument(self != nullptr && info != nullptr, caller);
    InitWideTimeGetFacet(self, refs);
}

void InitTimePutCharFacet(GuestLocale::Facet* self, std::size_t refs) {
    auto* facet = reinterpret_cast<GuestTimePutCharFacet*>(self);
    facet->base = MakeFacetHeader(&g_timePutCharVtable.facet, refs);
    facet->names = *_Getptimes_nid_postfix();
}

} // namespace

extern "C" {

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t refs) {
    ConstructWideTimeGetFacet(self, localeName, refs, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t refs) {
    ConstructWideTimeGetFacet(self, localeName, refs, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    ConstructWideTimeGetFacet(self, "C", refs, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    ConstructWideTimeGetFacet(self, "C", refs, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    ConstructWideTimeGetFromLocinfo(self, info, refs, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    ConstructWideTimeGetFromLocinfo(self, info, refs, __func__);
}

std::size_t APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    if (facet != nullptr && *facet == nullptr) {
        RequireArgument(locale != nullptr && *locale != nullptr, __func__);
        struct FacetGuard {
            void* ptr;
            ~FacetGuard() {
                if (ptr != nullptr) _ZdlPv_nid_postfix(ptr);
            }
        } guard{_Znwm_nid_postfix(sizeof(GuestTimeGetWideFacet))};
        ConstructWideTimeGetFacet(static_cast<GuestLocale::Facet*>(guard.ptr), (*locale)->name, 0, __func__);
        *facet = static_cast<GuestLocale::Facet*>(guard.ptr);
        guard.ptr = nullptr;
    }
    return 5;
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix(GuestLocale::Facet* self) {
    TimeFieldsDeleteWideTimeGet_nid_no_patch(self);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(GuestLocale::Facet* self) {
    RequireArgument(self != nullptr, __func__);
}

void APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    RequireArgument(self != nullptr, __func__);
    ScopedClassicLocinfo locinfo("C");
    InitTimePutCharFacet(self, refs);
}

void APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    RequireArgument(self != nullptr && info != nullptr, __func__);
    InitTimePutCharFacet(self, refs);
}

void APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    RequireArgument(self != nullptr, __func__);
    ScopedClassicLocinfo locinfo("C");
    InitTimePutCharFacet(self, refs);
}

void APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info) {
    RequireArgument(self != nullptr && info != nullptr, __func__);
    reinterpret_cast<GuestTimePutCharFacet*>(self)->names = *_Getptimes_nid_postfix();
}

void APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev_nid_postfix(GuestLocale::Facet* self) {
    TimeFieldsDeleteFacet_nid_no_patch(self);
}

void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    RequireArgument(self != nullptr, __func__);
    *self = {&g_timePutWideVtable.facet, static_cast<std::uint32_t>(refs), 0};
}

void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    RequireArgument(self != nullptr, __func__);
    *self = {&g_timePutWideVtable.facet, static_cast<std::uint32_t>(refs), 0};
}

void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::Facet*, const GuestLocale::LocinfoStorage*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, int lo, int hi, int* val, const GuestLocale::Facet* ctype) {
    return GetIntImpl<char>(self, first, last, lo, hi, val, ctype, __func__);
}

std::int32_t APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, int lo, int hi, int* val, const GuestLocale::Facet* ctype) {
    return GetIntImpl<std::uint16_t>(self, first, last, lo, hi, val, ctype, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetTimeImpl<char>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetDateImpl<char>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase*, std::int32_t* state, GuestTm* time) {
    return DoGetNameFieldImpl<char>(self, first, last, state, time, &GuestTm::tm_wday, kCharDays, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetYearImpl<char>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase*, std::int32_t* state, GuestTm* time) {
    return DoGetNameFieldImpl<char>(self, first, last, state, time, &GuestTm::tm_mon, kCharMonths, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char) {
    return DoGetImpl<char>(self, first, last, stream, state, time, specifier, kCharAmPm, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* fmtfirst) {
    return DoGetFmtImpl<char>(self, first, last, stream, state, time, fmtfirst, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetTimeImpl<std::uint16_t>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetDateImpl<std::uint16_t>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase*, std::int32_t* state, GuestTm* time) {
    return DoGetNameFieldImpl<std::uint16_t>(self, first, last, state, time, &GuestTm::tm_wday, WideFieldTable(self, &GuestTimeGetWideFacet::days, kWideDays), __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return DoGetYearImpl<std::uint16_t>(self, first, last, stream, state, time, __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase*, std::int32_t* state, GuestTm* time) {
    return DoGetNameFieldImpl<std::uint16_t>(self, first, last, state, time, &GuestTm::tm_mon, WideFieldTable(self, &GuestTimeGetWideFacet::months, kWideMonths), __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const std::uint16_t* fmtfirst, const std::uint16_t* fmtlast) {
    RequireArgument(self != nullptr && state != nullptr && time != nullptr && (fmtfirst == nullptr) == (fmtlast == nullptr) && fmtlast >= fmtfirst, __func__);
    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* ctype = CtypeTraits<std::uint16_t>::FromStream(stream, hold);
    const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(self->vtable);
    *state = 0;

    for (; fmtfirst != fmtlast; ++fmtfirst) {
        if (*state != 0) {
            break;
        }
        if (Equal<std::uint16_t>(first, last)) {
            *state = IosEofBit | IosFailBit;
            break;
        }
        if (CtypeTraits<std::uint16_t>::Narrow(ctype, *fmtfirst, '\0') != '%') {
            if (CtypeTraits<std::uint16_t>::Is(ctype, CtypeSpaceMask, *fmtfirst)) {
                while (!Equal<std::uint16_t>(first, last) &&
                       CtypeTraits<std::uint16_t>::Is(ctype, CtypeSpaceMask, Peek<std::uint16_t>(first))) {
                    Inc<std::uint16_t>(first);
                }
            } else if (CtypeTraits<std::uint16_t>::ToLower(ctype, Peek<std::uint16_t>(first)) !=
                       CtypeTraits<std::uint16_t>::ToLower(ctype, *fmtfirst)) {
                *state |= IosFailBit;
                break;
            } else {
                Inc<std::uint16_t>(first);
            }
        } else if (++fmtfirst == fmtlast) {
            *state = IosFailBit;
            break;
        } else {
            char specifier = CtypeTraits<std::uint16_t>::Narrow(ctype, *fmtfirst, '\0');
            char modifier = '\0';
            if (specifier == 'E' || specifier == 'O' || specifier == 'Q' || specifier == '#') {
                if (++fmtfirst == fmtlast) {
                    *state = IosFailBit;
                    break;
                }
                modifier = specifier;
                specifier = CtypeTraits<std::uint16_t>::Narrow(ctype, *fmtfirst, '\0');
            }
            RequireArgument(vtable != nullptr && vtable->get != nullptr, __func__);
            first = vtable->get(self, first, last, stream, state, time, specifier, modifier);
        }
    }
    return first;
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char) {
    return DoGetImpl<std::uint16_t>(self, first, last, stream, state, time, specifier, WideFieldTable(self, &GuestTimeGetWideFacet::amPm, kWideAmPm), __func__);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, const char* fmtfirst) {
    return DoGetFmtImpl<std::uint16_t>(self, first, last, stream, state, time, fmtfirst, __func__);
}

int APS5_VABI _ZSt10_GetloctxtIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEiRT0_S5_mPKT__nid_postfix(GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, std::size_t numFields, const char* ptr) {
    if (first == nullptr || last == nullptr || ptr == nullptr) {
        return -2;
    }
    return GetLocaleText<char>(*first, last, numFields, ptr);
}

}
