// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using namespace Icu::Abi;
constexpr std::int32_t StackCapacity = 100;

template<typename TValue>
TValue& Require(TValue* pointer) {
    if (!pointer) throw std::invalid_argument("ICU edits: null pointer");
    return *pointer;
}

std::int32_t Checked(std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU edits: integer overflow");
    return static_cast<std::int32_t>(value);
}

std::uint16_t& Unit(Edits& edits, std::int32_t index) {
    if (!edits.array || index < 0 || index >= edits.capacity) throw std::invalid_argument("ICU edits: invalid array access");
    return edits.array[index];
}

std::uint16_t Read(const EditsIterator& iterator, std::int32_t index) {
    if (!iterator.array || index < 0 || index >= iterator.length) throw std::invalid_argument("ICU edits: invalid iterator access");
    return iterator.array[index];
}

void Release(Edits& edits) {
    if (edits.array != edits.stack) Storage::Release(edits.array);
}

void Reset(Edits& edits) {
    edits.length = edits.delta = edits.changes = edits.error = 0;
}

void CopyUnits(std::uint16_t* destination, const std::uint16_t* source, std::int32_t length) {
    if (length <= 0) return;
    if (!destination || !source) throw std::invalid_argument("ICU edits: null copy buffer");
    if (destination == source) return;
    const auto bytes = static_cast<std::size_t>(length) * sizeof(std::uint16_t);
    const auto left = reinterpret_cast<std::uintptr_t>(destination);
    const auto right = reinterpret_cast<std::uintptr_t>(source);
    if ((left < right ? right - left : left - right) < bytes) throw std::invalid_argument("ICU edits: overlapping copy");
    std::memcpy(destination, source, bytes);
}

bool Grow(Edits& edits) {
    std::int32_t capacity;
    if (edits.array == edits.stack) capacity = 2000;
    else if (edits.capacity == std::numeric_limits<std::int32_t>::max()) {
        edits.error = 8;
        return false;
    } else if (edits.capacity >= std::numeric_limits<std::int32_t>::max() / 2) capacity = std::numeric_limits<std::int32_t>::max();
    else capacity = Checked(static_cast<std::int64_t>(edits.capacity) * 2);
    if (Checked(static_cast<std::int64_t>(capacity) - edits.capacity) < 5) {
        edits.error = 8;
        return false;
    }
    auto* array = static_cast<std::uint16_t*>(Storage::Resize(nullptr, static_cast<std::size_t>(capacity) * 2));
    if (!array) {
        edits.error = 7;
        return false;
    }
    CopyUnits(array, edits.array, edits.length);
    Release(edits);
    edits.array = array;
    edits.capacity = capacity;
    return true;
}

void Append(Edits& edits, std::int32_t value) {
    if (edits.length < edits.capacity || Grow(edits)) {
        Unit(edits, edits.length) = static_cast<std::uint16_t>(value);
        edits.length = Checked(static_cast<std::int64_t>(edits.length) + 1);
    }
}

void AddUnchanged(Edits& edits, std::int32_t length) {
    if (edits.error > 0 || length == 0) return;
    if (length < 0) {
        edits.error = 1;
        return;
    }
    const auto last = edits.length > 0 ? Unit(edits, edits.length - 1) : 0xffff;
    if (last < 0xfff) {
        const auto room = 0xfff - last;
        if (length <= room) {
            Unit(edits, edits.length - 1) = static_cast<std::uint16_t>(last + length);
            return;
        }
        Unit(edits, edits.length - 1) = 0xfff;
        length -= room;
    }
    for (; length >= 0x1000; length -= 0x1000) Append(edits, 0xfff);
    if (length > 0) Append(edits, length - 1);
}

void AddReplace(Edits& edits, std::int32_t oldLength, std::int32_t newLength) {
    if (edits.error > 0) return;
    if (oldLength < 0 || newLength < 0) {
        edits.error = 1;
        return;
    }
    if (oldLength == 0 && newLength == 0) return;
    edits.changes = Checked(static_cast<std::int64_t>(edits.changes) + 1);
    const auto delta = static_cast<std::int64_t>(edits.delta) + newLength - oldLength;
    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        edits.error = 8;
        return;
    }
    edits.delta = static_cast<std::int32_t>(delta);
    if (oldLength > 0 && oldLength <= 6 && newLength <= 7) {
        const auto head = (oldLength << 12) | (newLength << 9);
        const auto last = edits.length > 0 ? Unit(edits, edits.length - 1) : 0xffff;
        if (last > 0xfff && last < 0x6fff && (last & ~0x1ff) == head && (last & 0x1ff) < 0x1ff)
            Unit(edits, edits.length - 1) = static_cast<std::uint16_t>(last + 1);
        else Append(edits, head);
        return;
    }
    if (oldLength < 61 && newLength < 61) {
        Append(edits, 0x7000 | (oldLength << 6) | newLength);
        return;
    }
    if (Checked(static_cast<std::int64_t>(edits.capacity) - edits.length) < 5 && !Grow(edits)) return;
    auto limit = Checked(static_cast<std::int64_t>(edits.length) + 1);
    const auto encode = [&](std::int32_t length) {
        if (length < 61) return length;
        if (length <= 0x7fff) {
            Unit(edits, limit++) = static_cast<std::uint16_t>(0x8000 | length);
            return 61;
        }
        Unit(edits, limit++) = static_cast<std::uint16_t>(0x8000 | (length >> 15));
        Unit(edits, limit++) = static_cast<std::uint16_t>(0x8000 | length);
        return 62 + (length >> 30);
    };
    const auto oldHead = encode(oldLength);
    const auto newHead = encode(newLength);
    Unit(edits, edits.length) = static_cast<std::uint16_t>(0x7000 | (oldHead << 6) | newHead);
    edits.length = limit;
}

Edits* Copy(Edits& edits, const Edits& source) {
    if (edits.error > 0) {
        edits.length = edits.delta = edits.changes = 0;
        return &edits;
    }
    if (edits.length > edits.capacity) {
        auto* array = static_cast<std::uint16_t*>(Storage::Resize(nullptr, static_cast<std::size_t>(edits.length) * 2));
        if (!array) {
            edits.length = edits.delta = edits.changes = 0;
            edits.error = 7;
            return &edits;
        }
        Release(edits);
        edits.array = array;
        edits.capacity = edits.length;
    }
    CopyUnits(edits.array, source.array, edits.length);
    return &edits;
}

Edits* Move(Edits& edits, Edits& source) {
    if (edits.error > 0) {
        edits.length = edits.delta = edits.changes = 0;
        return &edits;
    }
    if (&edits == &source) throw std::invalid_argument("ICU edits: self move");
    Release(edits);
    if (edits.length > StackCapacity) {
        edits.array = source.array;
        edits.capacity = source.capacity;
        source.array = source.stack;
        source.capacity = StackCapacity;
        Reset(source);
    } else {
        edits.array = edits.stack;
        edits.capacity = StackCapacity;
        CopyUnits(edits.array, source.array, edits.length);
    }
    return &edits;
}

std::int32_t ReadLength(EditsIterator& iterator, std::int32_t head) {
    if (head < 61) return head;
    if (head == 61) {
        const auto value = Read(iterator, iterator.index) & 0x7fff;
        ++iterator.index;
        return value;
    }
    const auto high = Read(iterator, iterator.index);
    const auto low = Read(iterator, Checked(static_cast<std::int64_t>(iterator.index) + 1));
    iterator.index = Checked(static_cast<std::int64_t>(iterator.index) + 2);
    return ((head & 1) << 30) | ((high & 0x7fff) << 15) | (low & 0x7fff);
}

void Update(EditsIterator& iterator, std::int32_t direction) {
    iterator.sourceIndex = Checked(static_cast<std::int64_t>(iterator.sourceIndex) + direction * static_cast<std::int64_t>(iterator.oldLength));
    if (iterator.changed) iterator.replacementIndex = Checked(static_cast<std::int64_t>(iterator.replacementIndex) + direction * static_cast<std::int64_t>(iterator.newLength));
    iterator.destinationIndex = Checked(static_cast<std::int64_t>(iterator.destinationIndex) + direction * static_cast<std::int64_t>(iterator.newLength));
}

std::int8_t NoNext(EditsIterator& iterator) {
    iterator.direction = iterator.changed = 0;
    iterator.oldLength = iterator.newLength = 0;
    return 0;
}

bool Next(EditsIterator& iterator, bool onlyChanges, std::int32_t status) {
    if (status > 0) return false;
    if (iterator.direction > 0) Update(iterator, 1);
    else {
        if (iterator.direction < 0 && iterator.remaining > 0) {
            iterator.index = Checked(static_cast<std::int64_t>(iterator.index) + 1);
            iterator.direction = 1;
            return true;
        }
        iterator.direction = 1;
    }
    if (iterator.remaining > 1) {
        --iterator.remaining;
        return true;
    }
    if (iterator.remaining == 1) iterator.remaining = 0;
    if (iterator.index >= iterator.length) return NoNext(iterator);
    auto unit = Read(iterator, iterator.index++);
    if (unit <= 0xfff) {
        iterator.changed = 0;
        iterator.oldLength = unit + 1;
        while (iterator.index < iterator.length && (unit = Read(iterator, iterator.index)) <= 0xfff) {
            ++iterator.index;
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + unit + 1);
        }
        iterator.newLength = iterator.oldLength;
        if (!onlyChanges) return true;
        Update(iterator, 1);
        if (iterator.index >= iterator.length) return NoNext(iterator);
        ++iterator.index;
    }
    iterator.changed = 1;
    if (unit <= 0x6fff) {
        const auto count = (unit & 0x1ff) + 1;
        iterator.oldLength = unit >> 12;
        iterator.newLength = (unit >> 9) & 7;
        if (!iterator.coarse) {
            if (count > 1) iterator.remaining = count;
            return true;
        }
        iterator.oldLength *= count;
        iterator.newLength *= count;
    } else {
        iterator.oldLength = ReadLength(iterator, (unit >> 6) & 63);
        iterator.newLength = ReadLength(iterator, unit & 63);
        if (!iterator.coarse) return true;
    }
    while (iterator.index < iterator.length && (unit = Read(iterator, iterator.index)) > 0xfff) {
        ++iterator.index;
        if (unit <= 0x6fff) {
            const auto count = (unit & 0x1ff) + 1;
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + (unit >> 12) * count);
            iterator.newLength = Checked(static_cast<std::int64_t>(iterator.newLength) + ((unit >> 9) & 7) * count);
        } else {
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + ReadLength(iterator, (unit >> 6) & 63));
            iterator.newLength = Checked(static_cast<std::int64_t>(iterator.newLength) + ReadLength(iterator, unit & 63));
        }
    }
    return true;
}

bool Previous(EditsIterator& iterator, std::int32_t status) {
    if (status > 0) return false;
    if (iterator.direction >= 0) {
        if (iterator.direction > 0) {
            if (iterator.remaining > 0) {
                iterator.index = Checked(static_cast<std::int64_t>(iterator.index) - 1);
                iterator.direction = -1;
                return true;
            }
            Update(iterator, 1);
        }
        iterator.direction = -1;
    }
    if (iterator.remaining > 0) {
        const auto unit = Read(iterator, iterator.index);
        if (iterator.remaining <= (unit & 0x1ff)) {
            ++iterator.remaining;
            Update(iterator, -1);
            return true;
        }
        iterator.remaining = 0;
    }
    if (iterator.index <= 0) return NoNext(iterator);
    auto unit = Read(iterator, --iterator.index);
    if (unit <= 0xfff) {
        iterator.changed = 0;
        iterator.oldLength = unit + 1;
        while (iterator.index > 0 && (unit = Read(iterator, iterator.index - 1)) <= 0xfff) {
            --iterator.index;
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + unit + 1);
        }
        iterator.newLength = iterator.oldLength;
        Update(iterator, -1);
        return true;
    }
    iterator.changed = 1;
    if (unit <= 0x6fff) {
        const auto count = (unit & 0x1ff) + 1;
        iterator.oldLength = unit >> 12;
        iterator.newLength = (unit >> 9) & 7;
        if (!iterator.coarse) {
            if (count > 1) iterator.remaining = 1;
            Update(iterator, -1);
            return true;
        }
        iterator.oldLength *= count;
        iterator.newLength *= count;
    } else {
        if (unit <= 0x7fff) {
            iterator.oldLength = ReadLength(iterator, (unit >> 6) & 63);
            iterator.newLength = ReadLength(iterator, unit & 63);
        } else {
            do { unit = Read(iterator, --iterator.index); } while (unit > 0x7fff);
            const auto head = iterator.index++;
            iterator.oldLength = ReadLength(iterator, (unit >> 6) & 63);
            iterator.newLength = ReadLength(iterator, unit & 63);
            iterator.index = head;
        }
        if (!iterator.coarse) {
            Update(iterator, -1);
            return true;
        }
    }
    while (iterator.index > 0 && (unit = Read(iterator, iterator.index - 1)) > 0xfff) {
        --iterator.index;
        if (unit <= 0x6fff) {
            const auto count = (unit & 0x1ff) + 1;
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + (unit >> 12) * count);
            iterator.newLength = Checked(static_cast<std::int64_t>(iterator.newLength) + ((unit >> 9) & 7) * count);
        } else if (unit <= 0x7fff) {
            const auto head = iterator.index++;
            iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) + ReadLength(iterator, (unit >> 6) & 63));
            iterator.newLength = Checked(static_cast<std::int64_t>(iterator.newLength) + ReadLength(iterator, unit & 63));
            iterator.index = head;
        }
    }
    Update(iterator, -1);
    return true;
}

std::int32_t Find(EditsIterator& iterator, std::int32_t index, bool source, std::int32_t status) {
    if (status > 0 || index < 0) return -1;
    auto start = source ? iterator.sourceIndex : iterator.destinationIndex;
    auto length = source ? iterator.oldLength : iterator.newLength;
    if (index < start) {
        if (index >= start / 2) {
            for (;;) {
                if (!Previous(iterator, status)) throw std::invalid_argument("ICU edits: invalid backward search state");
                start = source ? iterator.sourceIndex : iterator.destinationIndex;
                if (index >= start) return 0;
                if (iterator.remaining <= 0) continue;
                length = source ? iterator.oldLength : iterator.newLength;
                const auto count = (Read(iterator, iterator.index) & 0x1ff) + 1 - iterator.remaining;
                const auto span = Checked(static_cast<std::int64_t>(count) * length);
                std::int32_t skipped = count;
                const bool found = index >= Checked(static_cast<std::int64_t>(start) - span);
                if (found) {
                    if (!length) throw std::invalid_argument("ICU edits: zero search divisor");
                    skipped = Checked((static_cast<std::int64_t>(start) - index - 1) / length + 1);
                }
                iterator.sourceIndex = Checked(static_cast<std::int64_t>(iterator.sourceIndex) - static_cast<std::int64_t>(skipped) * iterator.oldLength);
                iterator.replacementIndex = Checked(static_cast<std::int64_t>(iterator.replacementIndex) - static_cast<std::int64_t>(skipped) * iterator.newLength);
                iterator.destinationIndex = Checked(static_cast<std::int64_t>(iterator.destinationIndex) - static_cast<std::int64_t>(skipped) * iterator.newLength);
                if (found) {
                    iterator.remaining = Checked(static_cast<std::int64_t>(iterator.remaining) + skipped);
                    return 0;
                }
                iterator.remaining = 0;
            }
        }
        iterator.direction = 0;
        iterator.index = iterator.remaining = iterator.oldLength = iterator.newLength = 0;
        iterator.sourceIndex = iterator.replacementIndex = iterator.destinationIndex = 0;
    } else if (index < Checked(static_cast<std::int64_t>(start) + length)) return 0;
    while (Next(iterator, false, status)) {
        start = source ? iterator.sourceIndex : iterator.destinationIndex;
        length = source ? iterator.oldLength : iterator.newLength;
        if (index < Checked(static_cast<std::int64_t>(start) + length)) return 0;
        if (iterator.remaining <= 1) continue;
        const auto span = Checked(static_cast<std::int64_t>(iterator.remaining) * length);
        if (index < Checked(static_cast<std::int64_t>(start) + span)) {
            if (!length) throw std::invalid_argument("ICU edits: zero search divisor");
            const auto skipped = Checked((static_cast<std::int64_t>(index) - start) / length);
            iterator.sourceIndex = Checked(static_cast<std::int64_t>(iterator.sourceIndex) + static_cast<std::int64_t>(skipped) * iterator.oldLength);
            iterator.replacementIndex = Checked(static_cast<std::int64_t>(iterator.replacementIndex) + static_cast<std::int64_t>(skipped) * iterator.newLength);
            iterator.destinationIndex = Checked(static_cast<std::int64_t>(iterator.destinationIndex) + static_cast<std::int64_t>(skipped) * iterator.newLength);
            iterator.remaining -= skipped;
            return 0;
        }
        iterator.oldLength = Checked(static_cast<std::int64_t>(iterator.oldLength) * iterator.remaining);
        iterator.newLength = Checked(static_cast<std::int64_t>(iterator.newLength) * iterator.remaining);
        iterator.remaining = 0;
    }
    return 1;
}

}

extern "C" {

void APS5_VABI _ZN6icu_685Edits12releaseArrayEv_nid_postfix(Edits* self) { Release(Require(self)); }
void APS5_VABI _ZN6icu_685EditsD1Ev_nid_postfix(Edits* self) { Release(Require(self)); }
void APS5_VABI _ZN6icu_685EditsD2Ev_nid_postfix(Edits* self) { Release(Require(self)); }
void APS5_VABI _ZN6icu_685Edits5resetEv_nid_postfix(Edits* self) { Reset(Require(self)); }
void APS5_VABI _ZN6icu_685Edits6appendEi_nid_postfix(Edits* self, std::int32_t unit) { Append(Require(self), unit); }
std::int8_t APS5_VABI _ZN6icu_685Edits9growArrayEv_nid_postfix(Edits* self) { return Grow(Require(self)); }
void APS5_VABI _ZN6icu_685Edits12addUnchangedEi_nid_postfix(Edits* self, std::int32_t length) { AddUnchanged(Require(self), length); }
void APS5_VABI _ZN6icu_685Edits10addReplaceEii_nid_postfix(Edits* self, std::int32_t oldLength, std::int32_t newLength) { AddReplace(Require(self), oldLength, newLength); }
Edits* APS5_VABI _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(Edits* self, const Edits* source) { return Copy(Require(self), Require(source)); }
Edits* APS5_VABI _ZN6icu_685Edits9moveArrayERS0__nid_postfix(Edits* self, Edits* source) { return Move(Require(self), Require(source)); }

Edits* APS5_VABI _ZN6icu_685EditsaSERKS0__nid_postfix(Edits* self, const Edits* source) {
    auto& edits = Require(self);
    const auto& other = Require(source);
    edits.length = other.length;
    edits.delta = other.delta;
    edits.changes = other.changes;
    edits.error = other.error;
    return Copy(edits, other);
}

Edits* APS5_VABI _ZN6icu_685EditsaSEOS0__nid_postfix(Edits* self, Edits* source) {
    auto& edits = Require(self);
    auto& other = Require(source);
    if (self == source) throw std::invalid_argument("ICU edits: self move");
    edits.length = other.length;
    edits.delta = other.delta;
    edits.changes = other.changes;
    edits.error = other.error;
    return Move(edits, other);
}

std::int8_t APS5_VABI _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(const Edits* self, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0) return 1;
    const auto& edits = Require(self);
    if (edits.error <= 0) return 0;
    error = edits.error;
    return 1;
}

void APS5_VABI _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(EditsIterator* self, const std::uint16_t* array, std::int32_t length, std::int8_t onlyChanges, std::int8_t coarse) {
    auto& iterator = Require(self);
    iterator = EditsIterator{};
    iterator.array = array;
    iterator.length = length;
    iterator.onlyChanges = onlyChanges;
    iterator.coarse = coarse;
}

void APS5_VABI _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(EditsIterator* self, const std::uint16_t* array, std::int32_t length, std::int8_t onlyChanges, std::int8_t coarse) {
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(self, array, length, onlyChanges, coarse);
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(EditsIterator* self, std::int32_t head) { return ReadLength(Require(self), head); }
void APS5_VABI _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(EditsIterator* self) { Update(Require(self), 1); }
void APS5_VABI _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(EditsIterator* self) { Update(Require(self), -1); }
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(EditsIterator* self) { return NoNext(Require(self)); }
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(EditsIterator* self, std::int8_t onlyChanges, std::int32_t* status) { return Next(Require(self), onlyChanges != 0, Require(status)); }

std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(EditsIterator* self, std::int32_t* status) { return Previous(Require(self), Require(status)); }

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(EditsIterator* self, std::int32_t index, std::int8_t source, std::int32_t* status) {
    return Find(Require(self), index, source != 0, Require(status));
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(EditsIterator* self, std::int32_t index, std::int32_t* status) {
    auto& iterator = Require(self);
    const auto found = Find(iterator, index, true, Require(status));
    if (found < 0) return 0;
    if (found > 0 || index == iterator.sourceIndex) return iterator.destinationIndex;
    return Checked(static_cast<std::int64_t>(iterator.destinationIndex) + (iterator.changed ? iterator.newLength : index - iterator.sourceIndex));
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(EditsIterator* self, std::int32_t index, std::int32_t* status) {
    auto& iterator = Require(self);
    const auto found = Find(iterator, index, false, Require(status));
    if (found < 0) return 0;
    if (found > 0 || index == iterator.destinationIndex) return iterator.sourceIndex;
    return Checked(static_cast<std::int64_t>(iterator.sourceIndex) + (iterator.changed ? iterator.oldLength : index - iterator.destinationIndex));
}

Edits* APS5_VABI _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(Edits* self, const Edits* first, const Edits* second, std::int32_t* status) {
    if (_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(self, status)) return self;
    auto& edits = Require(self);
    const auto& ab = Require(first);
    const auto& bc = Require(second);
    EditsIterator left{};
    EditsIterator right{};
    left.array = ab.array;
    left.length = ab.length;
    right.array = bc.array;
    right.length = bc.length;
    bool hasLeft = true;
    bool hasRight = true;
    std::int32_t a = 0, leftB = 0, rightB = 0, c = 0, pendingA = 0, pendingC = 0;
    const auto emit = [&](std::int32_t oldLength, std::int32_t newLength) {
        AddReplace(edits, oldLength, newLength);
        pendingA = pendingC = 0;
    };
    for (;;) {
        if (rightB == 0 && hasRight && (hasRight = Next(right, false, *status))) {
            rightB = right.oldLength;
            c = right.newLength;
            if (rightB == 0) {
                if (leftB == 0 || !left.changed) emit(pendingA, Checked(static_cast<std::int64_t>(pendingC) + c));
                else pendingC = Checked(static_cast<std::int64_t>(pendingC) + c);
                continue;
            }
        }
        if (leftB == 0) {
            if (hasLeft && (hasLeft = Next(left, false, *status))) {
                a = left.oldLength;
                leftB = left.newLength;
                if (leftB == 0) {
                    if (rightB == right.oldLength || !right.changed) emit(Checked(static_cast<std::int64_t>(pendingA) + a), pendingC);
                    else pendingA = Checked(static_cast<std::int64_t>(pendingA) + a);
                    continue;
                }
            } else if (rightB == 0) break;
            else {
                if (!_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(self, status)) *status = 1;
                return self;
            }
        }
        if (rightB == 0) {
            if (!_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(self, status)) *status = 1;
            return self;
        }
        if (!left.changed && !right.changed) {
            if (pendingA != 0 || pendingC != 0) emit(pendingA, pendingC);
            const auto length = std::min(a, c);
            AddUnchanged(edits, length);
            leftB = a -= length;
            rightB = c -= length;
            continue;
        }
        if (!left.changed && right.changed) {
            if (leftB >= rightB) {
                emit(Checked(static_cast<std::int64_t>(pendingA) + rightB), Checked(static_cast<std::int64_t>(pendingC) + c));
                a = leftB -= rightB;
                rightB = 0;
                continue;
            }
        } else if (left.changed && !right.changed) {
            if (leftB <= rightB) {
                emit(Checked(static_cast<std::int64_t>(pendingA) + a), Checked(static_cast<std::int64_t>(pendingC) + leftB));
                c = rightB -= leftB;
                leftB = 0;
                continue;
            }
        } else if (leftB == rightB) {
            emit(Checked(static_cast<std::int64_t>(pendingA) + a), Checked(static_cast<std::int64_t>(pendingC) + c));
            leftB = rightB = 0;
            continue;
        }
        pendingA = Checked(static_cast<std::int64_t>(pendingA) + a);
        pendingC = Checked(static_cast<std::int64_t>(pendingC) + c);
        if (leftB < rightB) {
            rightB -= leftB;
            c = leftB = 0;
        } else {
            leftB -= rightB;
            a = rightB = 0;
        }
    }
    if (pendingA != 0 || pendingC != 0) emit(pendingA, pendingC);
    _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(self, status);
    return self;
}

}
