// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2003, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2006, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace {

using namespace Icu::Abi;
using Set = UnicodeSet;
using String = UnicodeString;
constexpr std::int32_t High = 0x110000;

template<typename T> T& Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("UnicodeSet: null pointer");
    return *pointer;
}
inline void RequirePointer(const void* pointer) {
    if (!pointer) throw std::invalid_argument("UnicodeSet: null pointer");
}

bool Frozen(const Set& set) { return set.bmpSet || set.stringSpan; }
bool Locked(const Set& set) { return Frozen(set) || (set.fFlags & 1); }
std::int32_t Pin(std::int32_t point) { return std::clamp(point, 0, High - 1); }
std::int32_t StringCount(const Set& set) { return set.strings ? set.strings->count : 0; }
const String* StringAt(const Set& set, std::int32_t index) {
    return static_cast<const String*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(&Require(set.strings), index));
}
std::int32_t Find(const Set& set, std::int32_t point) {
    if (!set.list || set.len < 1) throw std::invalid_argument("UnicodeSet: invalid list");
    if (point < set.list[0]) return 0;
    if (set.len == 1 || point >= set.list[set.len - 2]) return set.len - 1;
    return static_cast<std::int32_t>(std::upper_bound(set.list, set.list + set.len, point) - set.list);
}
std::int32_t Point(const String& text, std::int32_t index) {
    return _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(&text, index);
}
std::int32_t Single(const String& text) {
    if (text.Length() > 2) return -1;
    if (text.Length() == 1) return _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&text, 0);
    const auto point = Point(text, 0);
    return point > 0xffff ? point : -1;
}
bool EqualString(const String& a, const String& b) {
    return a.Length() == b.Length() && (!a.Length() || std::memcmp(&Require(a.Buffer()), &Require(b.Buffer()), a.Length() * 2) == 0);
}
bool StringsContain(const Set& set, const String& text) {
    for (std::int32_t i = 0; i < StringCount(set); ++i) if (EqualString(Require(StringAt(set, i)), text)) return true;
    return false;
}
void ReleasePattern(Set& set) {
    if (set.pat) {
        Free_nid_no_patch(set.pat);
        set.pat = nullptr;
        set.patLen = 0;
    }
}
void ClearStrings(Set& set) {
    if (set.strings) _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(set.strings);
}
Set* Clear(Set& set) {
    if (Frozen(set)) return &set;
    Require(set.list) = High;
    set.len = 1;
    ReleasePattern(set);
    ClearStrings(set);
    set.fFlags = 0;
    return &set;
}
void Bogus(Set& set) { Clear(set); set.fFlags = 1; }
std::int32_t NextCapacity(std::int32_t minimum) {
    if (minimum < 25) return minimum + 25;
    if (minimum <= 2500) return minimum * 5;
    if (minimum > std::numeric_limits<std::int32_t>::max() / 2) throw std::invalid_argument("UnicodeSet: capacity overflow");
    return std::min(minimum * 2, High + 1);
}
bool Ensure(Set& set, std::int32_t length, bool scratch) {
    length = std::min(length, High + 1);
    auto& capacity = scratch ? set.bufferCapacity : set.capacity;
    auto*& pointer = scratch ? set.buffer : set.list;
    if (length <= capacity) return true;
    const auto next = NextCapacity(length);
    auto* replacement = static_cast<std::int32_t*>(Allocate_nid_no_patch(static_cast<std::size_t>(next) * 4));
    if (!replacement) { Bogus(set); return false; }
    if (!scratch) std::memcpy(replacement, &Require(pointer), set.len * 4);
    if (pointer != set.stackList) Free_nid_no_patch(pointer);
    pointer = replacement;
    capacity = next;
    return true;
}
void Swap(Set& set) { std::swap(set.list, set.buffer); std::swap(set.capacity, set.bufferCapacity); }
void Combine(Set& set, const std::int32_t* other, std::int32_t length, std::int8_t polarity, int operation) {
    if (Locked(set) || (!other && operation == 0)) return;
    Require(other);
    if (polarity < 0 || polarity > 3 || length < 0 || length > std::numeric_limits<std::int32_t>::max() - set.len)
        throw std::invalid_argument("UnicodeSet: invalid list operation");
    if (!Ensure(set, set.len + length, true)) return;
    std::int32_t i = 0, j = 0, used = 0;
    auto readFirst = [&] {
        if (i >= set.len) throw std::invalid_argument("UnicodeSet: list traversal outside active storage");
        return set.list[i++];
    };
    auto readSecond = [&] { return other[j++]; };
    if (operation == 2) {
        auto first = readFirst();
        auto second = readSecond();
        if (polarity == 1 || polarity == 2) {
            j = other[0] == 0 ? 1 : 0;
            second = j ? other[j] : 0;
        }
        while (first != High || second != High) {
            if (first < second) { set.buffer[used++] = first; first = readFirst(); }
            else if (second < first) { set.buffer[used++] = second; second = readSecond(); }
            else { first = readFirst(); second = readSecond(); }
        }
        set.buffer[used++] = High;
        set.len = used;
        Swap(set);
        ReleasePattern(set);
        return;
    }
    auto first = readFirst();
    auto second = readSecond();
    for (;;) {
        if (first == High && second == High) break;
        if (operation == 0 && polarity == 3) {
            const auto limit = std::max(first, second);
            if (limit == High) break;
            set.buffer[used++] = limit;
            first = readFirst();
            second = readSecond();
            polarity = 0;
        } else if (operation == 0 && polarity == 0) {
            const bool equal = first == second;
            if (first <= second) {
                if (used && first <= set.buffer[used - 1]) first = std::max(readFirst(), set.buffer[--used]);
                else { set.buffer[used++] = first; first = readFirst(); }
                polarity ^= 1;
                if (equal) { second = readSecond(); polarity ^= 2; }
            } else {
                if (used && second <= set.buffer[used - 1]) second = std::max(readSecond(), set.buffer[--used]);
                else { set.buffer[used++] = second; second = readSecond(); }
                polarity ^= 2;
            }
        } else if (first < second) {
            if (operation == 0 ? polarity == 1 : (polarity & 2) != 0) set.buffer[used++] = first;
            first = readFirst();
            polarity ^= 1;
        } else if (second < first) {
            if (operation == 0 ? polarity == 2 : (polarity & 1) != 0) set.buffer[used++] = second;
            second = readSecond();
            polarity ^= 2;
        } else {
            if (operation == 1 && (polarity == 0 || polarity == 3)) set.buffer[used++] = first;
            first = readFirst();
            second = readSecond();
            polarity ^= 3;
        }
    }
    set.buffer[used++] = High;
    set.len = used;
    Swap(set);
    ReleasePattern(set);
}
Set* AddPoint(Set& set, std::int32_t point) {
    point = Pin(point);
    const auto index = Find(set, point);
    if ((index & 1) || Locked(set)) return &set;
    if (point == set.list[index] - 1) {
        set.list[index] = point;
        if (point == High - 1) {
            if (!Ensure(set, set.len + 1, false)) return &set;
            set.list[set.len++] = High;
        }
        if (index > 0 && point == set.list[index - 1]) {
            std::memmove(set.list + index - 1, set.list + index + 1, (set.len - index - 1) * 4);
            set.len -= 2;
        }
    } else if (index > 0 && point == set.list[index - 1]) {
        ++set.list[index - 1];
    } else {
        if (!Ensure(set, set.len + 2, false)) return &set;
        std::memmove(set.list + index + 2, set.list + index, (set.len - index) * 4);
        set.list[index] = point;
        set.list[index + 1] = point + 1;
        set.len += 2;
    }
    ReleasePattern(set);
    return &set;
}
Set* AddRange(Set& set, std::int32_t start, std::int32_t end) {
    start = Pin(start);
    end = Pin(end);
    if (start == end) return AddPoint(set, start);
    if (start > end) return &set;
    const auto limit = end + 1;
    const auto last = set.len == 1 ? -2 : set.list[set.len - 2];
    if ((set.len & 1) && last <= start && !Locked(set)) {
        if (last == start) {
            set.list[set.len - 2] = limit;
            if (limit == High) --set.len;
        } else {
            set.list[set.len - 1] = start;
            if (Ensure(set, set.len + (limit < High ? 2 : 1), false)) {
                if (limit < High) set.list[set.len++] = limit;
                set.list[set.len++] = High;
            }
        }
        ReleasePattern(set);
    } else {
        const std::int32_t range[]{start, limit, High};
        Combine(set, range, 2, 0, 0);
    }
    return &set;
}
Set* RangeOperation(Set& set, std::int32_t start, std::int32_t end, int operation) {
    if (operation == 2 && Locked(set)) return &set;
    start = Pin(start);
    end = Pin(end);
    if (start <= end) {
        const std::int32_t range[]{start, end + 1, High};
        Combine(set, range, 2, operation == 3 ? 2 : 0, operation == 2 ? 2 : 1);
    } else if (operation == 1) Clear(set);
    if (operation == 2) ReleasePattern(set);
    return &set;
}
void APS5_VABI DeleteString_nid_no_patch(void* object) {
    if (!object) return;
    Virtual_nid_no_patch<void (APS5_VABI *)(void*)>(object, 1)(object);
}
void APS5_VABI DestroyVector_nid_no_patch(Vector* vector) {
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&Require(vector));
    Free_nid_no_patch(vector->elements);
    vector->vtable = Vtable_nid_no_patch(Class::Object);
}
void APS5_VABI DeleteVector_nid_no_patch(Vector* vector) { DestroyVector_nid_no_patch(vector); Free_nid_no_patch(vector); }
std::int8_t APS5_VABI EqualElement_nid_no_patch(Element a, Element b) {
    if (a.pointer == b.pointer) return 1;
    return a.pointer && b.pointer && EqualString(*static_cast<String*>(a.pointer), *static_cast<String*>(b.pointer));
}
std::int8_t APS5_VABI CompareElement_nid_no_patch(Element a, Element b) {
    const auto& first = Require(static_cast<String*>(a.pointer));
    const auto& second = Require(static_cast<String*>(b.pointer));
    return _ZNK6icu_6813UnicodeString9doCompareEiiPKDsii_nid_postfix(&first, 0, first.Length(), second.Buffer(), 0, second.Length());
}
template<std::size_t N> struct Table {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, N> slots;
};
bool AllocateStrings(Set& set, std::int32_t& status) {
    if (status > 0) return false;
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_687UVectorE"}, Rtti_nid_no_patch(Class::Object)};
    static const Table<3> table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(DestroyVector_nid_no_patch),
        reinterpret_cast<GuestFunction>(DeleteVector_nid_no_patch),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix)}}};
    set.strings = static_cast<Vector*>(Allocate_nid_no_patch(sizeof(Vector)));
    if (!set.strings) { status = 7; return false; }
    *set.strings = {table.slots.data(), 0, 0, nullptr, DeleteString_nid_no_patch, EqualElement_nid_no_patch};
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(set.strings, 1, &status);
    if (status > 0) { DeleteVector_nid_no_patch(set.strings); set.strings = nullptr; return false; }
    return true;
}
void AddString(Set& set, const String& text) {
    if (Locked(set)) return;
    std::int32_t status = 0;
    if (!set.strings && !AllocateStrings(set, status)) { Bogus(set); return; }
    auto* copy = static_cast<String*>(Allocate_nid_no_patch(sizeof(String)));
    if (!copy) { Bogus(set); return; }
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(copy, &text);
    _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(set.strings, copy, CompareElement_nid_no_patch, &status);
    if (status > 0) { Bogus(set); DeleteString_nid_no_patch(copy); }
}
Set* StringOperation(Set& set, const String& text, int operation) {
    if (!text.Length() || Locked(set)) return &set;
    const auto point = Single(text);
    if (point >= 0) return operation == 0 ? AddPoint(set, point) : RangeOperation(set, point, point, operation == 1 ? 3 : 2);
    const bool contains = StringsContain(set, text);
    if (operation == 0) {
        if (!contains) { AddString(set, text); ReleasePattern(set); }
    } else if (operation == 1) {
        if (set.strings && _ZN6icu_687UVector13removeElementEPv_nid_postfix(set.strings, const_cast<String*>(&text))) ReleasePattern(set);
    } else {
        if (contains) _ZN6icu_687UVector13removeElementEPv_nid_postfix(set.strings, const_cast<String*>(&text));
        else AddString(set, text);
        ReleasePattern(set);
    }
    return &set;
}
struct SpanStorage {
    std::uint64_t padding;
    Set spanSet;
    Set* notSet;
    const Vector* strings;
    std::int32_t* lengths;
    std::uint8_t* spanLengths;
    std::uint8_t* utf8;
    std::int32_t utf8Length;
    std::int32_t maxLength16;
    std::int32_t maxLength8;
    std::int8_t all;
    std::int32_t staticLengths[32];
};
static_assert(sizeof(SpanStorage) == 392 && alignof(SpanStorage) == 8);
static_assert(offsetof(SpanStorage, spanSet) == 8 && offsetof(SpanStorage, notSet) == 208);
static_assert(offsetof(SpanStorage, lengths) == 224 && offsetof(SpanStorage, staticLengths) == 264);
void Destroy(Set& set) {
    if (set.list != set.stackList) Free_nid_no_patch(set.list);
    if (set.bmpSet) Virtual_nid_no_patch<void (APS5_VABI *)(void*)>(set.bmpSet, 1)(set.bmpSet);
    if (set.buffer != set.stackList) Free_nid_no_patch(set.buffer);
    if (set.strings) Virtual_nid_no_patch<void (APS5_VABI *)(void*)>(set.strings, 1)(set.strings);
    if (set.stringSpan) {
        auto& span = *static_cast<SpanStorage*>(set.stringSpan);
        if (span.notSet && span.notSet != &span.spanSet) { Destroy(*span.notSet); Free_nid_no_patch(span.notSet); }
        if (span.lengths && span.lengths != span.staticLengths) Free_nid_no_patch(span.lengths);
        Destroy(span.spanSet);
        Free_nid_no_patch(set.stringSpan);
    }
    ReleasePattern(set);
    set.vtable = Vtable_nid_no_patch(Class::Object);
}
const GuestFunction* SetTable();
const GuestFunction* MatcherTable();
void Initialize(Set& set) {
    set.vtable = SetTable();
    set.matcherVtable = MatcherTable();
    set.list = set.stackList;
    set.capacity = 25;
    set.len = 1;
    set.fFlags = 0;
    set.bmpSet = nullptr;
    set.buffer = nullptr;
    set.bufferCapacity = 0;
    set.pat = nullptr;
    set.patLen = 0;
    set.strings = nullptr;
    set.stringSpan = nullptr;
    set.stackList[0] = High;
}
void* SetId() { static char id; return &id; }

}

extern "C" {
Set* APS5_VABI _ZN6icu_6810UnicodeSet6addAllERKS0__nid_postfix(Set* self, const Set* other) {
    auto& set = Require(self); const auto& that = Require(other);

    if (that.len > 0 && that.list) Combine(set, that.list, that.len, 0, 0);
    for (std::int32_t i = 0; i < StringCount(that); ++i) { const auto& text = Require(StringAt(that, i)); if (!StringsContain(set, text)) AddString(set, text); }
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet9retainAllERKS0__nid_postfix(Set* self, const Set* other) {
    auto& set = Require(self); const auto& that = Require(other);
    if (Locked(set)) return self;
    Combine(set, that.list, that.len, 0, 1);
    if (StringCount(set)) { if (!StringCount(that)) ClearStrings(set); else _ZN6icu_687UVector9retainAllERKS0__nid_postfix(set.strings, that.strings); }
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet9removeAllERKS0__nid_postfix(Set* self, const Set* other) {
    auto& set = Require(self); const auto& that = Require(other);
    if (Locked(set)) return self;
    Combine(set, that.list, that.len, 2, 1);
    if (StringCount(set) && StringCount(that)) _ZN6icu_687UVector9removeAllERKS0__nid_postfix(set.strings, that.strings);
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet13complementAllERKS0__nid_postfix(Set* self, const Set* other) {
    auto& set = Require(self); const auto& that = Require(other);
    if (Locked(set)) return self;
    Combine(set, that.list, that.len, 0, 2);
    for (std::int32_t i = 0; i < StringCount(that); ++i) { const auto* text = StringAt(that, i); if (!set.strings || !_ZN6icu_687UVector13removeElementEPv_nid_postfix(set.strings, const_cast<String*>(text))) AddString(set, Require(text)); }
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet10createFromERKNS_13UnicodeStringE_nid_postfix(const String* text) {
    auto* set = static_cast<Set*>(Allocate_nid_no_patch(sizeof(Set)));
    if (set) { Initialize(*set); StringOperation(*set, Require(text), 0); }
    return set;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet13createFromAllERKNS_13UnicodeStringE_nid_postfix(const String* text) {
    auto* set = static_cast<Set*>(Allocate_nid_no_patch(sizeof(Set)));
    if (set) { Initialize(*set); _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(set, text); }
    return set;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet9retainAllERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    Set temporary{};
    Initialize(temporary);
    _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(&temporary, text);
    _ZN6icu_6810UnicodeSet9retainAllERKS0__nid_postfix(self, &temporary);
    Destroy(temporary);
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet9removeAllERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    Set temporary{};
    Initialize(temporary);
    _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(&temporary, text);
    _ZN6icu_6810UnicodeSet9removeAllERKS0__nid_postfix(self, &temporary);
    Destroy(temporary);
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet13complementAllERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    Set temporary{};
    Initialize(temporary);
    _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(&temporary, text);
    _ZN6icu_6810UnicodeSet13complementAllERKS0__nid_postfix(self, &temporary);
    Destroy(temporary);
    return self;
}

void APS5_VABI _ZNK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix(const Set* self, Set* other) {
    _ZN6icu_6810UnicodeSet6addAllERKS0__nid_postfix(other, self);
}

void APS5_VABI _ZThn8_NK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix(const void* self, Set* other) {
    _ZNK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix(reinterpret_cast<const Set*>(static_cast<const char*>(&Require(static_cast<const char*>(self))) - 8), other);
}

std::int8_t APS5_VABI _ZThn8_NK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix(const void* self, std::uint8_t value) {
    return _ZNK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix(reinterpret_cast<const Set*>(static_cast<const char*>(&Require(static_cast<const char*>(self))) - 8), value);
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet9serializeEPtiR10UErrorCode_nid_postfix(const Set* self, std::uint16_t* destination, std::int32_t capacity, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0) return 0;
    if (capacity < 0 || (capacity > 0 && !destination)) { error = 1; return 0; }
    const auto& set = Require(self);
    const auto boundaries = set.len - 1;
    if (!boundaries) { if (capacity > 0) *destination = 0; else error = 15; return 1; }
    std::int32_t bmp = 0;
    while (bmp < boundaries && set.list[bmp] <= 0xffff) ++bmp;
    const auto units = bmp + 2 * (boundaries - bmp);
    if (units > 0x7fff) { error = 8; return 0; }
    const auto total = units + (units > bmp ? 2 : 1);
    if (total > capacity) { error = 15; return total; }
    *destination++ = static_cast<std::uint16_t>(units | (units > bmp ? 0x8000 : 0));
    if (units > bmp) *destination++ = static_cast<std::uint16_t>(bmp);
    for (std::int32_t i = 0; i < boundaries; ++i) {
        if (i >= bmp) *destination++ = static_cast<std::uint16_t>(set.list[i] >> 16);
        *destination++ = static_cast<std::uint16_t>(set.list[i]);
    }
    return total;
}
void APS5_VABI _ZN6icu_6810UnicodeSetC1Ev_nid_postfix(Set* self) {
    Initialize(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSetC1Eii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    Initialize(Require(self)); AddRange(*self, start, end);
}

void APS5_VABI _ZN6icu_6810UnicodeSetC2Ev_nid_postfix(Set* self) {
    Initialize(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSetC2Eii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    Initialize(Require(self)); AddRange(*self, start, end);
}

void APS5_VABI _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(Set* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSetD2Ev_nid_postfix(Set* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSetD0Ev_nid_postfix(Set* self) {
    Destroy(Require(self)); Free_nid_no_patch(self);
}

void APS5_VABI _ZThn8_N6icu_6810UnicodeSetD1Ev_nid_postfix(void* self) {
    _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(reinterpret_cast<Set*>(static_cast<char*>(&Require(static_cast<char*>(self))) - 8));
}

void APS5_VABI _ZThn8_N6icu_6810UnicodeSetD0Ev_nid_postfix(void* self) {
    _ZN6icu_6810UnicodeSetD0Ev_nid_postfix(reinterpret_cast<Set*>(static_cast<char*>(&Require(static_cast<char*>(self))) - 8));
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet3addEi_nid_postfix(Set* self, std::int32_t point) {
    return AddPoint(Require(self), point);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet3addEii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    return AddRange(Require(self), start, end);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6retainEii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    return RangeOperation(Require(self), start, end, 1);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6retainEi_nid_postfix(Set* self, std::int32_t point) {
    return RangeOperation(Require(self), point, point, 1);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6removeEii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    return RangeOperation(Require(self), start, end, 3);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6removeEi_nid_postfix(Set* self, std::int32_t point) {
    return RangeOperation(Require(self), point, point, 3);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet10complementEii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    return RangeOperation(Require(self), start, end, 2);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet10complementEi_nid_postfix(Set* self, std::int32_t point) {
    return RangeOperation(Require(self), point, point, 2);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet3setEii_nid_postfix(Set* self, std::int32_t start, std::int32_t end) {
    Clear(Require(self)); return RangeOperation(*self, start, end, 2);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet10complementEv_nid_postfix(Set* self) {
    auto& set = Require(self);
    if (Locked(set)) return self;
    if (Require(set.list) == 0) {
        std::memmove(set.list, set.list + 1, (set.len - 1) * 4);
        --set.len;
    } else {
        if (!Ensure(set, set.len + 1, false)) return self;
        std::memmove(set.list + 1, set.list, set.len * 4);
        set.list[0] = 0;
        ++set.len;
    }
    ReleasePattern(set);
    return self;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet5clearEv_nid_postfix(Set* self) {
    return Clear(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSet10setToBogusEv_nid_postfix(Set* self) {
    Bogus(Require(self));
}

std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet12nextCapacityEi_nid_postfix(std::int32_t minimum) {
    return NextCapacity(minimum);
}

bool APS5_VABI _ZN6icu_6810UnicodeSet14ensureCapacityEi_nid_postfix(Set* self, std::int32_t length) {
    return Ensure(Require(self), length, false);
}

bool APS5_VABI _ZN6icu_6810UnicodeSet20ensureBufferCapacityEi_nid_postfix(Set* self, std::int32_t length) {
    return Ensure(Require(self), length, true);
}

void APS5_VABI _ZN6icu_6810UnicodeSet11swapBuffersEv_nid_postfix(Set* self) {
    Swap(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSet14releasePatternEv_nid_postfix(Set* self) {
    ReleasePattern(Require(self));
}

void APS5_VABI _ZN6icu_6810UnicodeSet10setPatternEPKDsi_nid_postfix(Set* self, const char16_t* pattern, std::int32_t length) {
    auto& set = Require(self);
    ReleasePattern(set);
    if (length < 0 || length == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("UnicodeSet: pattern length");
    set.pat = static_cast<char16_t*>(Allocate_nid_no_patch((static_cast<std::size_t>(length) + 1) * 2));
    if (set.pat) {
        if (length) std::memcpy(set.pat, &Require(pattern), length * 2);
        set.patLen = length;
        set.pat[length] = 0;
    }
}

void APS5_VABI _ZN6icu_6810UnicodeSet3addEPKiia_nid_postfix(Set* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity) {
    Combine(Require(self), list, length, polarity, 0);
}

void APS5_VABI _ZN6icu_6810UnicodeSet6retainEPKiia_nid_postfix(Set* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity) {
    Combine(Require(self), list, length, polarity, 1);
}

void APS5_VABI _ZN6icu_6810UnicodeSet11exclusiveOrEPKiia_nid_postfix(Set* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity) {
    Combine(Require(self), list, length, polarity, 2);
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13findCodePointEi_nid_postfix(const Set* self, std::int32_t point) {
    return Find(Require(self), point);
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix(const Set* self) {
    return Require(self).len / 2;
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix(const Set* self, std::int32_t index) {
    const auto& set = Require(self);
    if (index < 0 || index > (std::numeric_limits<std::int32_t>::max() - 0) / 2 || index * 2 + 0 >= set.len) throw std::invalid_argument("UnicodeSet: range index");
    return (&Require(set.list))[index * 2 + 0];
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix(const Set* self, std::int32_t index) {
    const auto& set = Require(self);
    if (index < 0 || index > (std::numeric_limits<std::int32_t>::max() - 1) / 2 || index * 2 + 1 >= set.len) throw std::invalid_argument("UnicodeSet: range index");
    return (&Require(set.list))[index * 2 + 1] - 1;
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsEi_nid_postfix(const Set* self, std::int32_t point) {
    return point < High && (Find(Require(self), point) & 1);
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsEii_nid_postfix(const Set* self, std::int32_t start, std::int32_t end) {
    const auto& set = Require(self); const auto index = Find(set, start); return (index & 1) == 1 && end < set.list[index];
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet12containsNoneEii_nid_postfix(const Set* self, std::int32_t start, std::int32_t end) {
    const auto& set = Require(self); const auto index = Find(set, start); return (index & 1) == 0 && end < set.list[index];
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet8hashCodeEv_nid_postfix(const Set* self) {
    const auto& set = Require(self);
    std::uint32_t result = set.len;
    for (std::int32_t i = 0; i < set.len; ++i) result = result * 1000003u + (&Require(set.list))[i];
    return static_cast<std::int32_t>(result);
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(const Set* self) {
    const auto& set = Require(self); std::int32_t count = StringCount(set);
    for (std::int32_t i = 0; i + 1 < set.len; i += 2) count += set.list[i + 1] - set.list[i];
    return count;
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet7isEmptyEv_nid_postfix(const Set* self) {
    return Require(self).len == 1 && !StringCount(*self);
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet10hasStringsEv_nid_postfix(const Set* self) {
    return StringCount(Require(self)) != 0;
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet11stringsSizeEv_nid_postfix(const Set* self) {
    return StringCount(Require(self));
}

const String* APS5_VABI _ZNK6icu_6810UnicodeSet9getStringEi_nid_postfix(const Set* self, std::int32_t index) {
    return StringAt(Require(self), index);
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet15stringsContainsERKNS_13UnicodeStringE_nid_postfix(const Set* self, const String* text) {
    return StringsContain(Require(self), Require(text));
}

std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet11getSingleCPERKNS_13UnicodeStringE_nid_postfix(const String* text) {
    return Single(Require(text));
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsERKNS_13UnicodeStringE_nid_postfix(const Set* self, const String* text) {
    if (!Require(text).Length()) return 0;
    const auto point = Single(*text);
    return point < 0 ? StringsContain(Require(self), *text) : _ZNK6icu_6810UnicodeSet8containsEi_nid_postfix(self, point);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet3addERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    return StringOperation(Require(self), Require(text), 0);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6removeERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    return StringOperation(Require(self), Require(text), 1);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet10complementERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    return StringOperation(Require(self), Require(text), 2);
}

void APS5_VABI _ZN6icu_6810UnicodeSet4_addERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    AddString(Require(self), Require(text));
}

std::int8_t APS5_VABI _ZN6icu_6810UnicodeSet15allocateStringsER10UErrorCode_nid_postfix(Set* self, std::int32_t* status) {
    return AllocateStrings(Require(self), Require(status));
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(Set* self, const String* text) {
    Require(self);
    for (std::int32_t i = 0; i < Require(text).Length();) { const auto point = Point(*text, i); AddPoint(*self, point); i += point > 0xffff ? 2 : 1; }
    return self;
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet11containsAllERKS0__nid_postfix(const Set* self, const Set* other) {
    const auto& set = Require(self); const auto& that = Require(other);
    for (std::int32_t i = 0; i + 1 < that.len; i += 2)
        if (!_ZNK6icu_6810UnicodeSet8containsEii_nid_postfix(&set, that.list[i], that.list[i + 1] - 1)) return 0;
    for (std::int32_t i = 0; i < StringCount(that); ++i) if (!StringsContain(set, Require(StringAt(that, i)))) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet12containsNoneERKS0__nid_postfix(const Set* self, const Set* other) {
    const auto& set = Require(self); const auto& that = Require(other);
    for (std::int32_t i = 0; i + 1 < that.len; i += 2)
        if (!_ZNK6icu_6810UnicodeSet12containsNoneEii_nid_postfix(&set, that.list[i], that.list[i + 1] - 1)) return 0;
    for (std::int32_t i = 0; i < StringCount(that); ++i) if (StringsContain(set, Require(StringAt(that, i)))) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSeteqERKS0__nid_postfix(const Set* self, const Set* other) {
    const auto& set = Require(self); const auto& that = Require(other);
    if (set.len != that.len || std::memcmp(&Require(set.list), &Require(that.list), set.len * 4)) return 0;
    if (StringCount(set) != StringCount(that)) return 0;
    for (std::int32_t i = 0; i < StringCount(set); ++i) if (!EqualString(Require(StringAt(set, i)), Require(StringAt(that, i)))) return 0;
    return 1;
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet6charAtEi_nid_postfix(const Set* self, std::int32_t index) {
    const auto& set = Require(self);
    if (index >= 0) for (std::int32_t i = 0; i + 1 < set.len; i += 2) {
        const auto count = set.list[i + 1] - set.list[i];
        if (index < count) return set.list[i] + index;
        index -= count;
    }
    return -1;
}

std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet7indexOfEi_nid_postfix(const Set* self, std::int32_t point) {
    const auto& set = Require(self);
    if (point < 0 || point >= High) return -1;
    std::int32_t count = 0;
    for (std::int32_t i = 0; i + 1 < set.len; i += 2) {
        if (point < set.list[i]) return -1;
        if (point < set.list[i + 1]) return count + point - set.list[i];
        count += set.list[i + 1] - set.list[i];
    }
    return -1;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet16removeAllStringsEv_nid_postfix(Set* self) {
    auto& set = Require(self);
    if (!Frozen(set) && StringCount(set)) { ClearStrings(set); ReleasePattern(set); }
    return self;
}

void* APS5_VABI _ZNK6icu_6810UnicodeSet17getDynamicClassIDEv_nid_postfix(const Set*) {
    return SetId();
}

void* APS5_VABI _ZN6icu_6810UnicodeSet16getStaticClassIDEv_nid_postfix() {
    return SetId();
}

std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix(const Set* self, std::uint8_t value) {
    const auto& set = Require(self);
    for (std::int32_t i = 0; i + 1 < set.len; i += 2) {
        const auto low = set.list[i], high = set.list[i + 1] - 1;
        if ((low & ~255) == (high & ~255) ? (low & 255) <= value && value <= (high & 255) : (low & 255) <= value || value <= (high & 255)) return 1;
    }
    for (std::int32_t i = 0; i < StringCount(set); ++i) if ((Point(Require(StringAt(set, i)), 0) & 255) == value) return 1;
    return 0;
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet7compactEv_nid_postfix(Set* self) {
    auto& set = Require(self);
    if (Locked(set)) return self;
    if (set.buffer != set.stackList) { Free_nid_no_patch(set.buffer); set.buffer = nullptr; set.bufferCapacity = 0; }
    if (set.list != set.stackList) {
        if (set.len <= 25) {
            std::memcpy(set.stackList, set.list, set.len * 4);
            Free_nid_no_patch(set.list); set.list = set.stackList; set.capacity = 25;
        } else if (set.len + 7 < set.capacity) {
            auto* replacement = static_cast<std::int32_t*>(Allocate_nid_no_patch(set.len * 4));
            if (replacement) { std::memcpy(replacement, set.list, set.len * 4); Free_nid_no_patch(set.list); set.list = replacement; set.capacity = set.len; }
        }
    }
    if (set.strings && !set.strings->count) { DeleteVector_nid_no_patch(set.strings); set.strings = nullptr; }
    return self;
}
}

namespace {
struct MultipleInfo {
    TypeInfo type;
    std::uint32_t flags;
    std::uint32_t count;
    struct Base { const TypeInfo* type; std::ptrdiff_t offsetFlags; } bases[2];
};
static_assert(sizeof(MultipleInfo) == 56 && offsetof(MultipleInfo, bases) == 24);
const TypeInfo* SetType() {
    static const DerivedTypeInfo functor{{SingleTypeTable_nid_no_patch(), "N6icu_6814UnicodeFunctorE"}, Rtti_nid_no_patch(Class::Object)};
    static const TypeInfo matcher{ClassTypeTable_nid_no_patch(), "N6icu_6814UnicodeMatcherE"};
    static const MultipleInfo filter{{MultipleTypeTable_nid_no_patch(), "N6icu_6813UnicodeFilterE"}, 0, 2, {{&functor.type, 2}, {&matcher, 2050}}};
    static const DerivedTypeInfo set{{SingleTypeTable_nid_no_patch(), "N6icu_6810UnicodeSetE"}, &filter.type};
    return &set.type;
}
const GuestFunction* SetTable() {
    static const Table<33> table{0, SetType(), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSetD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSetD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet5cloneEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet8containsEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSeteqERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet8hashCodeEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet7isEmptyEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet8containsEii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet11containsAllERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet3addEii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet6retainEii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet6removeEii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet10complementEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet10complementEii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet6addAllERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet9retainAllERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet9removeAllERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet13complementAllERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet5clearEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet16removeAllStringsEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810UnicodeSet7compactEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix),
    }}};
    return table.slots.data();
}

const GuestFunction* MatcherTable() {
    static const Table<6> table{-8, SetType(), {{
        reinterpret_cast<GuestFunction>(_ZThn8_N6icu_6810UnicodeSetD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZThn8_N6icu_6810UnicodeSetD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZThn8_N6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZThn8_NK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZThn8_NK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZThn8_NK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix),
    }}};
    return table.slots.data();
}

} // namespace

extern "C" {

void APS5_VABI _ZN6icu_6810UnicodeSetC2ERKS0__nid_postfix(Set* self, const Set* other) {
    _ZN6icu_6810UnicodeSetC2Ev_nid_postfix(self);
    _ZN6icu_6810UnicodeSet8copyFromERKS0_a_nid_postfix(self, other, 0);
}

void APS5_VABI _ZN6icu_6810UnicodeSetC1ERKS0__nid_postfix(Set* self, const Set* other) {
    _ZN6icu_6810UnicodeSetC2ERKS0__nid_postfix(self, other);
}

void APS5_VABI _ZN6icu_6810UnicodeSetC2ERKS0_a_nid_postfix(Set* self, const Set* other, std::int8_t asThawed) {
    _ZN6icu_6810UnicodeSetC2Ev_nid_postfix(self);
    _ZN6icu_6810UnicodeSet8copyFromERKS0_a_nid_postfix(self, other, asThawed);
}

void APS5_VABI _ZN6icu_6810UnicodeSetC1ERKS0_a_nid_postfix(Set* self, const Set* other, std::int8_t asThawed) {
    _ZN6icu_6810UnicodeSetC2ERKS0_a_nid_postfix(self, other, asThawed);
}

void APS5_VABI _ZN6icu_6810UnicodeSetC2EPKtiNS0_14ESerializationER10UErrorCode_nid_postfix(
    Set* self,
    const std::uint16_t* data,
    std::int32_t dataLen,
    std::int32_t serialization,
    std::int32_t* status
) {
    _ZN6icu_6810UnicodeSetC2Ev_nid_postfix(self);
    auto& stat = Require(status);
    if (stat > 0) {
        Bogus(*self);
        return;
    }
    if (serialization != 1 || !data || dataLen < 1) {
        stat = 1;
        Bogus(*self);
        return;
    }
    std::int32_t headerSize = (data[0] & 0x8000) ? 2 : 1;
    std::int32_t bmpLength = (headerSize == 1) ? data[0] : data[1];
    std::int32_t newLength = (((data[0] & 0x7FFF) - bmpLength) / 2) + bmpLength;
    if (!Ensure(*self, newLength + 1, false)) {
        Bogus(*self);
        return;
    }
    std::int32_t i = 0;
    for (i = 0; i < bmpLength; ++i) {
        self->list[i] = data[i + headerSize];
    }
    for (i = bmpLength; i < newLength; ++i) {
        self->list[i] = (static_cast<std::int32_t>(data[headerSize + bmpLength + (i - bmpLength) * 2 + 0]) << 16) +
                        static_cast<std::int32_t>(data[headerSize + bmpLength + (i - bmpLength) * 2 + 1]);
    }
    if (i == 0 || self->list[i - 1] != High) {
        self->list[i++] = High;
    }
    self->len = i;
}

void APS5_VABI _ZN6icu_6810UnicodeSetC1EPKtiNS0_14ESerializationER10UErrorCode_nid_postfix(
    Set* self,
    const std::uint16_t* data,
    std::int32_t dataLen,
    std::int32_t serialization,
    std::int32_t* status
) {
    _ZN6icu_6810UnicodeSetC2EPKtiNS0_14ESerializationER10UErrorCode_nid_postfix(self, data, dataLen, serialization, status);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSetaSERKS0__nid_postfix(Set* self, const Set* other) {
    return _ZN6icu_6810UnicodeSet8copyFromERKS0_a_nid_postfix(self, other, 0);
}

Set* APS5_VABI _ZN6icu_6810UnicodeSet8copyFromERKS0_a_nid_postfix(Set* self, const Set* other, std::int8_t asThawed) {
    auto& dst = Require(self);
    const auto& src = Require(other);
    if (self == other || Frozen(dst)) return self;
    if (src.fFlags & 1) {
        Bogus(dst);
        return self;
    }
    if (!Ensure(dst, src.len, false)) {
        Bogus(dst);
        return self;
    }
    dst.len = src.len;
    std::memcpy(dst.list, src.list, src.len * 4);
    if (StringCount(src) > 0) {
        std::int32_t status = 0;
        if ((!dst.strings && !AllocateStrings(dst, status)) || status > 0) {
            Bogus(dst);
            return self;
        }
        _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(dst.strings);
        for (std::int32_t i = 0; i < StringCount(src); ++i) {
            const auto* s = StringAt(src, i);
            auto* clone = static_cast<String*>(Allocate_nid_no_patch(sizeof(String)));
            if (!clone) { Bogus(dst); return self; }
            _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(clone, s);
            _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(dst.strings, clone, &status);
            if (status > 0) { Bogus(dst); return self; }
        }
    } else {
        ClearStrings(dst);
    }
    ReleasePattern(dst);
    if (src.pat) {
        _ZN6icu_6810UnicodeSet10setPatternEPKDsi_nid_postfix(&dst, src.pat, src.patLen);
    }
    return self;
}

Set* APS5_VABI _ZNK6icu_6810UnicodeSet5cloneEv_nid_postfix(const Set* self) {
    const auto& src = Require(self);
    auto* copy = static_cast<Set*>(Allocate_nid_no_patch(sizeof(Set)));
    if (!copy) return nullptr;
    _ZN6icu_6810UnicodeSetC1ERKS0__nid_postfix(copy, &src);
    return copy;
}

Set* APS5_VABI _ZNK6icu_6810UnicodeSet13cloneAsThawedEv_nid_postfix(const Set* self) {
    const auto& src = Require(self);
    auto* copy = static_cast<Set*>(Allocate_nid_no_patch(sizeof(Set)));
    if (!copy) return nullptr;
    _ZN6icu_6810UnicodeSetC1ERKS0_a_nid_postfix(copy, &src, 1);
    return copy;
}

std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet9matchRestERKNS_11ReplaceableEiiRKNS_13UnicodeStringE_nid_postfix(const void* text, std::int32_t start, std::int32_t limit, const String* s) {
    RequirePointer(text);
    const auto& str = Require(s);
    auto charAt = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t)>(text, 9);

    std::int32_t slen = str.Length();
    if (start < limit) {
        std::int32_t maxLen = limit - start;
        if (maxLen > slen) maxLen = slen;
        for (std::int32_t i = 1; i < maxLen; ++i) {
            if (charAt(text, start + i) != _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&str, i)) return 0;
        }
        return maxLen;
    } else {
        std::int32_t maxLen = start - limit;
        if (maxLen > slen) maxLen = slen;
        --slen;
        for (std::int32_t i = 1; i < maxLen; ++i) {
            if (charAt(text, start - i) != _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&str, slen - i)) return 0;
        }
        return maxLen;
    }
}

std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix(Set* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental) {
    auto& set = Require(self);
    RequirePointer(text);
    auto& off = Require(offset);

    if (off == limit) {
        if (_ZNK6icu_6810UnicodeSet8containsEi_nid_postfix(&set, 0xffff)) {
            return incremental ? 1 : 2;
        }
        return 0;
    }

    if (StringCount(set) > 0) {
        auto charAt = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t)>(text, 9);
        bool forward = off < limit;
        char16_t firstChar = charAt(text, off);
        std::int32_t highWaterLength = 0;

        for (std::int32_t i = 0; i < StringCount(set); ++i) {
            const auto* trial = StringAt(set, i);
            char16_t c = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(trial, forward ? 0 : trial->Length() - 1);
            if (forward && c > firstChar) break;
            if (c != firstChar) continue;

            std::int32_t matchLen = _ZN6icu_6810UnicodeSet9matchRestERKNS_11ReplaceableEiiRKNS_13UnicodeStringE_nid_postfix(text, off, limit, trial);
            if (incremental) {
                std::int32_t maxLen = forward ? limit - off : off - limit;
                if (matchLen == maxLen) return 1;
            }
            if (matchLen == trial->Length()) {
                if (matchLen > highWaterLength) highWaterLength = matchLen;
                if (forward && matchLen < highWaterLength) break;
                continue;
            }
        }
        if (highWaterLength != 0) {
            off += forward ? highWaterLength : -highWaterLength;
            return 2;
        }
    }

    return _ZN6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(
        reinterpret_cast<const UnicodeFilter*>(&set), text, &off, limit, incremental);
}

std::int32_t APS5_VABI _ZThn8_N6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix(void* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental) {
    RequirePointer(self);
    auto* set = reinterpret_cast<Set*>(static_cast<char*>(self) - 8);
    return _ZN6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix(set, text, offset, limit, incremental);
}

void APS5_VABI _ZN6icu_6810UnicodeSet11applyFilterEPFaiPvES1_PKS0_R10UErrorCode_nid_postfix(
    Set* self,
    std::int8_t (APS5_VABI *filter)(std::int32_t, void*),
    void* context,
    const Set* inclusions,
    std::int32_t* status
) {
    auto& set = Require(self);
    auto& stat = Require(status);
    if (stat > 0) return;
    if (!filter || !inclusions) throw std::invalid_argument("UnicodeSet: null filter/inclusions");

    Clear(set);
    std::int32_t startHasProperty = -1;
    std::int32_t limitRange = _ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix(inclusions);

    for (std::int32_t j = 0; j < limitRange; ++j) {
        std::int32_t start = _ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix(inclusions, j);
        std::int32_t end = _ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix(inclusions, j);
        for (std::int32_t ch = start; ch <= end; ++ch) {
            if (filter(ch, context)) {
                if (startHasProperty < 0) startHasProperty = ch;
            } else if (startHasProperty >= 0) {
                _ZN6icu_6810UnicodeSet3addEii_nid_postfix(&set, startHasProperty, ch - 1);
                startHasProperty = -1;
            }
        }
    }
    if (startHasProperty >= 0) {
        _ZN6icu_6810UnicodeSet3addEii_nid_postfix(&set, startHasProperty, 0x10ffff);
    }
    if ((set.fFlags & 1) && stat <= 0) {
        stat = 7;
    }
}

void APS5_VABI _ZN6icu_6810UStringSetD2Ev_nid_postfix(Vector* self) {
    auto& vec = Require(self);
    DestroyVector_nid_no_patch(&vec);
}

void APS5_VABI _ZN6icu_6810UStringSetD1Ev_nid_postfix(Vector* self) {
    _ZN6icu_6810UStringSetD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6810UStringSetD0Ev_nid_postfix(Vector* self) {
    auto& vec = Require(self);
    _ZN6icu_6810UStringSetD1Ev_nid_postfix(&vec);
    Free_nid_no_patch(&vec);
}

}
