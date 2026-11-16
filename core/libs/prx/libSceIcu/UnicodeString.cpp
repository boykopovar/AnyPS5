// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using String = Icu::Abi::UnicodeString;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("UnicodeString: null pointer");
    return *value;
}

std::int16_t Flags(const String* text) {
    return Require(text).storage.fields.lengthAndFlags;
}

std::int32_t Length(const String* text) {
    const auto flags = Flags(text);
    return flags >= 0 ? flags >> String::LengthShift : text->storage.fields.length;
}

char16_t* Array(const String* text) {
    if (Flags(text) & String::UsingStackBuffer) return const_cast<char16_t*>(text->storage.stack.buffer);
    return text->storage.fields.array;
}

char16_t Read(const String* text, std::int32_t index) {
    const auto* array = Array(text);
    if (!array) throw std::invalid_argument("UnicodeString: null character buffer");
    return array[index];
}

bool Lead(char16_t unit) {
    return unit >= 0xd800 && unit <= 0xdbff;
}

bool Trail(char16_t unit) {
    return unit >= 0xdc00 && unit <= 0xdfff;
}

void Pin(const String* text, std::int32_t& start, std::int32_t& count) {
    start = std::clamp(start, 0, Length(text));
    count = std::clamp(count, 0, Length(text) - start);
}

std::int32_t Count(const String* text, std::int32_t start, std::int32_t count) {
    std::int32_t result = 0;
    const auto limit = start + count;
    while (start < limit) {
        const auto unit = Read(text, start++);
        if (Lead(unit) && start < limit && Trail(Read(text, start))) ++start;
        ++result;
    }
    return result;
}

std::atomic_ref<std::int32_t> References(const String* text) {
    auto* array = Require(text).storage.fields.array;
    if (!array) throw std::invalid_argument("UnicodeString: null reference-counted buffer");
    auto* references = reinterpret_cast<std::int32_t*>(array) - 1;
    if (reinterpret_cast<std::uintptr_t>(references) % alignof(std::int32_t))
        throw std::invalid_argument("UnicodeString: unaligned reference count");
    return std::atomic_ref<std::int32_t>(*references);
}

char16_t APS5_VABI CharacterAt_nid_no_patch(std::int32_t offset, void* context) {
    return _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(static_cast<const String*>(context), offset);
}

}

namespace Icu::Abi {

std::int32_t CheckedInt_nid_no_patch(std::int64_t value) {
    if (value < INT32_MIN || value > INT32_MAX) throw std::invalid_argument("ICU: integer overflow");
    return static_cast<std::int32_t>(value);
}
std::int32_t Utf16Length_nid_no_patch(const char16_t* text) {
    if (!text) throw std::invalid_argument("ICU: null UTF-16 string");
    std::int32_t length = 0;
    while (text[length]) length = CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) + 1);
    return length;
}
std::int16_t UnicodeString::Flags() const { return storage.fields.lengthAndFlags; }
std::int32_t UnicodeString::Length() const { return Flags() >= 0 ? Flags() >> 5 : storage.fields.length; }
std::int32_t UnicodeString::Capacity() const { return Flags() & UsingStackBuffer ? 27 : storage.fields.capacity; }
char16_t* UnicodeString::Array() const { return Flags() & UsingStackBuffer ? const_cast<char16_t*>(storage.stack.buffer) : storage.fields.array; }
const char16_t* UnicodeString::Buffer() const { return Writable() ? Array() : nullptr; }
bool UnicodeString::Writable() const { return !(Flags() & (IsBogus | OpenGetBuffer)); }
bool UnicodeString::BufferWritable() const {
    return Writable() && !(Flags() & BufferIsReadonly) && (!(Flags() & RefCounted) || _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(this) == 1);
}
void UnicodeString::Empty() { storage.fields.lengthAndFlags = UsingStackBuffer; }
void UnicodeString::Init() { Initialize_nid_no_patch(this, Class::UnicodeString); Empty(); }
void UnicodeString::SetLength(std::int32_t length) {
    storage.fields.lengthAndFlags = static_cast<std::int16_t>((Flags() & 31) | (length <= 1023 ? static_cast<std::uint32_t>(length) << 5 : 0xffe0));
    if (length > 1023) storage.fields.length = length;
}
void UnicodeString::Pin(std::int32_t& start, std::int32_t& count) const {
    start = std::clamp(start, 0, Length());
    count = std::clamp(count, 0, Length() - start);
}
void UnicodeString::Release() {
    if ((Flags() & RefCounted) && _ZN6icu_6813UnicodeString9removeRefEv_nid_postfix(this) == 0)
        Free_nid_no_patch(reinterpret_cast<std::int32_t*>(storage.fields.array) - 1);
}
void UnicodeString::Bogus() {
    Release();
    storage.fields.lengthAndFlags = IsBogus;
    storage.fields.array = nullptr;
    storage.fields.capacity = 0;
}
bool UnicodeString::Allocate(std::int32_t capacity) {
    if (capacity <= 27) { Empty(); return true; }
    if (capacity <= 0x7ffffff5) {
        const auto bytes = (4 + (static_cast<std::size_t>(capacity) + 1) * 2 + 15) & ~std::size_t{15};
        auto* block = static_cast<std::int32_t*>(Allocate_nid_no_patch(bytes));
        if (block) {
            *block = 1;
            storage.fields.array = reinterpret_cast<char16_t*>(block + 1);
            storage.fields.capacity = static_cast<std::int32_t>((bytes - 4) / 2);
            storage.fields.lengthAndFlags = RefCounted;
            return true;
        }
    }
    storage.fields.lengthAndFlags = IsBogus;
    storage.fields.array = nullptr;
    storage.fields.capacity = 0;
    return false;
}
bool UnicodeString::CloneBuffer(std::int32_t capacity, std::int32_t grow, bool copy, std::int32_t** deferred, bool force) {
    if (capacity == -1) capacity = Capacity();
    if (!Writable()) return false;
    if (!force && !(Flags() & BufferIsReadonly) && (!(Flags() & RefCounted) || _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(this) <= 1) && capacity <= Capacity()) return true;
    if (grow < 0) grow = capacity;
    else if (capacity <= 27 && grow > 27) grow = 27;
    char16_t saved[27];
    auto* oldArray = Array();
    const auto oldLength = Length();
    const auto oldFlags = Flags();
    if (oldFlags & UsingStackBuffer) {
        if (copy && grow > 27) { std::memcpy(saved, oldArray, oldLength * sizeof(char16_t)); oldArray = saved; }
        else oldArray = nullptr;
    }
    if (Allocate(grow) || (capacity < grow && Allocate(capacity))) {
        if (copy) {
            const auto length = std::min(oldLength, Capacity());
            if (oldArray && length > 0) std::memmove(Array(), oldArray, length * sizeof(char16_t));
            SetLength(length);
        } else SetLength(0);
        if (oldFlags & RefCounted) {
            if (!oldArray) throw std::invalid_argument("ICU: null owned buffer");
            auto* references = reinterpret_cast<std::int32_t*>(oldArray) - 1;
            if (std::atomic_ref<std::int32_t>(*references).fetch_sub(1) == 1) {
                if (deferred) *deferred = references;
                else Free_nid_no_patch(references);
            }
        }
        return true;
    }
    if (!(oldFlags & UsingStackBuffer)) storage.fields.array = oldArray;
    storage.fields.lengthAndFlags = oldFlags;
    Bogus();
    return false;
}
UnicodeString* UnicodeString::CopyFrom(const UnicodeString* source, bool fast) {
    Require(source);
    if (this == source) return this;
    if (source->Flags() & IsBogus) { Bogus(); return this; }
    Release();
    if ((source->Flags() >> 5) == 0) { Empty(); return this; }
    storage.fields.lengthAndFlags = source->Flags();
    const auto kind = source->Flags() & 31;
    if (kind == UsingStackBuffer) std::memcpy(storage.stack.buffer, source->Array(), Length() * sizeof(char16_t));
    else if (kind == RefCounted || (kind == BufferIsReadonly && fast)) {
        if (kind == RefCounted) _ZN6icu_6813UnicodeString6addRefEv_nid_postfix(const_cast<UnicodeString*>(source));
        storage.fields.array = source->storage.fields.array;
        storage.fields.capacity = source->storage.fields.capacity;
        if (Flags() < 0) storage.fields.length = source->storage.fields.length;
    } else if (kind == BufferIsReadonly || kind == WritableAlias) {
        const auto length = source->Length();
        if (Allocate(length)) {
            if (length > 0) std::memcpy(Array(), source->Array(), length * sizeof(char16_t));
            SetLength(length);
        }
    } else {
        storage.fields.lengthAndFlags = IsBogus;
        storage.fields.array = nullptr;
        storage.fields.capacity = 0;
    }
    return this;
}
void UnicodeString::CopyFields(UnicodeString* source, bool reset) {
    Require(source);
    storage.fields.lengthAndFlags = source->Flags();
    if (Flags() & UsingStackBuffer) {
        if (this != source) std::memcpy(Array(), source->Array(), Length() * sizeof(char16_t));
    } else {
        storage.fields.array = source->storage.fields.array;
        storage.fields.capacity = source->storage.fields.capacity;
        if (Flags() < 0) storage.fields.length = source->storage.fields.length;
        if (reset) {
            source->storage.fields.lengthAndFlags = IsBogus;
            source->storage.fields.array = nullptr;
            source->storage.fields.capacity = 0;
        }
    }
}
namespace {
std::int32_t Grow(std::int32_t length) {
    return static_cast<std::int32_t>(std::min<std::int64_t>(static_cast<std::int64_t>(length) + (length >> 2) + 128, 0x7ffffff5));
}
struct Temporary {
    UnicodeString text;
    Temporary(const char16_t* source, std::int32_t length) { text.Init(); text.Append(source, 0, length); }
    ~Temporary() { text.Release(); }
};
bool Overlap(const char16_t* first, std::int32_t firstLength, const char16_t* second, std::int32_t secondLength) {
    if (!second || !first) return false;
    const auto a = reinterpret_cast<std::uintptr_t>(first);
    const auto b = reinterpret_cast<std::uintptr_t>(second);
    return a < b + static_cast<std::uint64_t>(secondLength) * 2 && b < a + static_cast<std::uint64_t>(firstLength) * 2;
}
void CopyUnits(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count <= 0) return;
    if (!source || !destination) throw std::invalid_argument("ICU: null string buffer");
    std::memmove(destination, source, static_cast<std::size_t>(count) * 2);
}
}
UnicodeString* UnicodeString::Append(const char16_t* source, std::int32_t start, std::int32_t count) {
    if (!Writable() || count == 0 || !source) return this;
    source += start;
    if (count < 0) count = Utf16Length_nid_no_patch(source);
    if (count == 0) return this;
    const auto oldLength = Length();
    if (count > INT32_MAX - oldLength) { Bogus(); return this; }
    const auto length = oldLength + count;
    if (BufferWritable() && Overlap(Array(), oldLength, source, count)) {
        Temporary saved(source, count);
        if (saved.text.Flags() & IsBogus) { Bogus(); return this; }
        return Append(saved.text.Array(), 0, count);
    }
    if ((length <= Capacity() && BufferWritable()) || CloneBuffer(length, Grow(length))) {
        if (source != Array() + oldLength) CopyUnits(Array() + oldLength, source, count);
        SetLength(length);
    }
    return this;
}
UnicodeString* UnicodeString::Replace(std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength) {
    if (!Writable()) return this;
    const auto oldLength = Length();
    if ((Flags() & BufferIsReadonly) && sourceLength == 0) {
        if (start == 0) {
            count = std::clamp(count, 0, oldLength);
            storage.fields.array += count;
            storage.fields.capacity -= count;
            SetLength(oldLength - count);
            return this;
        }
        start = std::clamp(start, 0, oldLength);
        if (count >= oldLength - start) { SetLength(start); storage.fields.capacity = start; return this; }
    }
    if (start == oldLength) return Append(source, sourceStart, sourceLength);
    if (!source) sourceLength = 0;
    else { source += sourceStart; if (sourceLength < 0) sourceLength = Utf16Length_nid_no_patch(source); }
    Pin(start, count);
    if (sourceLength > INT32_MAX - (oldLength - count)) { Bogus(); return this; }
    const auto length = oldLength - count + sourceLength;
    if (BufferWritable() && Overlap(Array(), oldLength, source, sourceLength)) {
        Temporary saved(source, sourceLength);
        if (saved.text.Flags() & IsBogus) { Bogus(); return this; }
        return Replace(start, count, saved.text.Array(), 0, sourceLength);
    }
    char16_t saved[27];
    auto* oldArray = Array();
    if ((Flags() & UsingStackBuffer) && length > 27) { CopyUnits(saved, oldArray, oldLength); oldArray = saved; }
    std::int32_t* deferred = nullptr;
    if (!CloneBuffer(length, Grow(length), false, &deferred)) return this;
    auto* array = Array();
    if (array != oldArray) {
        CopyUnits(array, oldArray, start);
        CopyUnits(array + start + sourceLength, oldArray + start + count, oldLength - start - count);
    } else if (count != sourceLength) CopyUnits(array + start + sourceLength, oldArray + start + count, oldLength - start - count);
    CopyUnits(array + start, source, sourceLength);
    SetLength(length);
    Free_nid_no_patch(deferred);
    return this;
}

}

namespace {
std::int32_t Search(const String* text, const char16_t* needle, std::int32_t size, std::int32_t start, std::int32_t count, bool reverse) {
    text->Pin(start, count);
    if (size < -1) return start;
    if (size == -1) size = Icu::Abi::Utf16Length_nid_no_patch(needle);
    if (size == 0) return start;
    if (size > count) return -1;
    const auto* array = text->Array();
    if (!array || !needle) throw std::invalid_argument("UnicodeString: null search buffer");
    const auto last = start + count - size;
    for (auto index = reverse ? last : start; reverse ? index >= start : index <= last; reverse ? --index : ++index) {
        if (std::memcmp(array + index, needle, static_cast<std::size_t>(size) * 2)) continue;
        if (Trail(array[index]) && index > start && Lead(array[index - 1])) continue;
        if (Lead(array[index + size - 1]) && index + size < start + count && Trail(array[index + size])) continue;
        return index;
    }
    return -1;
}
std::int8_t Compare(const String* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength, bool codePoints) {
    if (text->Flags() & 1) return -1;
    text->Pin(start, count);
    if (!source) sourceLength = sourceStart = 0;
    else {
        source += sourceStart;
        if (sourceLength < 0) sourceLength = Icu::Abi::Utf16Length_nid_no_patch(source + (codePoints ? 0 : sourceStart));
    }
    const auto* array = text->Array();
    for (std::int32_t index = 0; index < std::min(count, sourceLength); ++index) {
        std::int32_t first = Read(text, start + index);
        std::int32_t second = source[index];
        if (first == second) continue;
        if (codePoints && first >= 0xd800 && second >= 0xd800) {
            if (!((Lead(first) && index + 1 < count && Trail(array[start + index + 1])) || (Trail(first) && index > 0 && Lead(array[start + index - 1])))) first -= 0x2800;
            if (!((Lead(second) && index + 1 < sourceLength && Trail(source[index + 1])) || (Trail(second) && index > 0 && Lead(source[index - 1])))) second -= 0x2800;
        }
        return first < second ? -1 : 1;
    }
    return count < sourceLength ? -1 : count > sourceLength ? 1 : 0;
}
template<typename TUnit>
void Terminate(TUnit* destination, std::int32_t capacity, std::int32_t length, std::int32_t& status) {
    if (status > 0 || length < 0) return;
    if (length < capacity) { if (!destination) throw std::invalid_argument("ICU: null destination"); destination[length] = 0; if (status == -124) status = 0; }
    else status = length == capacity ? -124 : 15;
}
std::int32_t NextPoint(const char16_t* source, std::int32_t length, std::int32_t& index) {
    if (!source) throw std::invalid_argument("ICU: null UTF-16 input");
    const auto unit = source[index++];
    if (Lead(unit) && index < length && Trail(source[index])) return 0x10000 + ((unit - 0xd800) << 10) + source[index++] - 0xdc00;
    return Lead(unit) || Trail(unit) ? 0xfffd : unit;
}
std::int32_t EncodeUtf8(const char16_t* source, std::int32_t length, char* destination, std::int32_t capacity, std::int32_t& status) {
    if (status > 0) return 0;
    if ((!source && length) || length < 0 || capacity < 0 || (!destination && capacity)) { status = 1; return 0; }
    std::int32_t result = 0;
    bool writing = true;
    for (std::int32_t index = 0; index < length;) {
        const auto point = NextPoint(source, length, index);
        char bytes[4];
        const auto width = point <= 0x7f ? 1 : point <= 0x7ff ? 2 : point <= 0xffff ? 3 : 4;
        if (width == 1) bytes[0] = static_cast<char>(point);
        else {
            for (int part = width - 1, remaining = point; part > 0; --part, remaining >>= 6) bytes[part] = static_cast<char>(0x80 | (remaining & 63));
            bytes[0] = static_cast<char>((width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (point >> ((width - 1) * 6)));
        }
        if (writing && width <= capacity - result) std::memcpy(destination + result, bytes, width);
        else writing = false;
        result = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(result) + width);
    }
    Terminate(destination, capacity, result, status);
    return result;
}
std::int32_t DecodeUtf8(const char* source, std::int32_t length, std::int32_t& index) {
    const auto lead = static_cast<std::uint8_t>(source[index++]);
    if (lead < 0x80) return lead;
    const auto count = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
    if (!count) return 0xfffd;
    std::int32_t point = lead & (0x7f >> count);
    for (int part = 0; part < count; ++part) {
        if (index == length) return 0xfffd;
        const auto trail = static_cast<std::uint8_t>(source[index]);
        if (trail < 0x80 || trail > 0xbf || (part == 0 && ((lead == 0xe0 && trail < 0xa0) || (lead == 0xed && trail >= 0xa0) || (lead == 0xf0 && trail < 0x90) || (lead == 0xf4 && trail > 0x8f)))) return 0xfffd;
        point = (point << 6) | (trail & 63);
        ++index;
    }
    return point;
}
}

extern "C" {

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(const String* text) {
    return Length(text);
}

char16_t APS5_VABI _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(const String* text, std::int32_t offset) {
    return static_cast<std::uint32_t>(offset) < static_cast<std::uint32_t>(Length(text)) ? Read(text, offset) : 0xffff;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(const String* text, std::int32_t offset) {
    const auto length = Length(text);
    if (static_cast<std::uint32_t>(offset) >= static_cast<std::uint32_t>(length)) return 0xffff;
    const auto unit = Read(text, offset);
    if (Lead(unit) && offset + 1 < length && Trail(Read(text, offset + 1)))
        return 0x10000 + ((unit - 0xd800) << 10) + Read(text, offset + 1) - 0xdc00;
    if (Trail(unit) && offset > 0 && Lead(Read(text, offset - 1)))
        return 0x10000 + ((Read(text, offset - 1) - 0xd800) << 10) + unit - 0xdc00;
    return unit;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11getChar32AtEi_nid_postfix(const String* text, std::int32_t offset) {
    return _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(text, offset);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(const String* text, std::int32_t offset) {
    if (static_cast<std::uint32_t>(offset) >= static_cast<std::uint32_t>(Length(text))) return 0;
    if (Trail(Read(text, offset)) && offset > 0 && Lead(Read(text, offset - 1))) --offset;
    return offset;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString14getChar32LimitEi_nid_postfix(const String* text, std::int32_t offset) {
    const auto length = Length(text);
    if (static_cast<std::uint32_t>(offset) >= static_cast<std::uint32_t>(length)) return length;
    if (offset > 0 && Lead(Read(text, offset - 1)) && Trail(Read(text, offset))) ++offset;
    return offset;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(const String* text, std::int32_t start, std::int32_t count) {
    Pin(text, start, count);
    if (!Array(text)) return 0;
    return Count(text, start, count);
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString17hasMoreChar32ThanEiii_nid_postfix(const String* text, std::int32_t start, std::int32_t count, std::int32_t number) {
    Pin(text, start, count);
    if (number < 0) return 1;
    if (!Array(text)) return 0;
    if (count == std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("UnicodeString: code point count overflow");
    if ((count + 1) / 2 > number) return 1;
    if (count <= number) return 0;
    return Count(text, start, count) > number;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(const String* text, std::int32_t index, std::int32_t delta) {
    index = std::clamp(index, 0, Length(text));
    if (delta == std::numeric_limits<std::int32_t>::min())
        throw std::invalid_argument("UnicodeString: index delta overflow");
    while (delta > 0 && index < Length(text)) {
        const auto unit = Read(text, index++);
        if (Lead(unit) && index < Length(text) && Trail(Read(text, index))) ++index;
        --delta;
    }
    while (delta < 0 && index > 0) {
        const auto unit = Read(text, --index);
        if (Trail(unit) && index > 0 && Lead(Read(text, index - 1))) --index;
        ++delta;
    }
    return index;
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(const String* text, const String* other, std::int32_t count) {
    Require(text);
    Require(other);
    if (count < 0) throw std::invalid_argument("UnicodeString: negative comparison length");
    if (!Array(text) || !Array(other)) throw std::invalid_argument("UnicodeString: null comparison buffer");
    return std::memcmp(Array(text), Array(other), static_cast<std::size_t>(count) * sizeof(char16_t)) == 0;
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(const String* text, const String* other) {
    const auto firstFlags = Flags(text);
    const auto secondFlags = Flags(other);
    if (firstFlags & String::IsBogus) return (secondFlags & String::IsBogus) != 0;
    return !(secondFlags & String::IsBogus) && Length(text) == Length(other) &&
        _ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(text, other, Length(text));
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(const String* text) {
    const auto length = Length(text);
    std::uint32_t hash = 0;
    if (Array(text)) {
        const auto step = (length - 32) / 32 + 1;
        for (std::int64_t index = 0; index < length; index += step) hash = hash * 37 + Read(text, static_cast<std::int32_t>(index));
    }
    return hash ? std::bit_cast<std::int32_t>(hash) : 1;
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString11hasMetaDataEv_nid_postfix(const String*) {
    return 0;
}

void APS5_VABI _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(String* text) {
    if (Flags(text) & String::IsBogus) text->storage.fields.lengthAndFlags = String::UsingStackBuffer;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(const String* text, std::int32_t* offset) {
    return u_unescapeAt_nid_postfix(CharacterAt_nid_no_patch, offset, Length(text), const_cast<String*>(text));
}

void APS5_VABI _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(const String* text, std::int32_t start, std::int32_t count, char16_t* destination, std::int32_t destinationStart) {
    Pin(text, start, count);
    if (!Array(text) || !destination) throw std::invalid_argument("UnicodeString: null extraction buffer");
    if (Array(text) + start != destination + destinationStart && count > 0)
        std::memmove(destination + destinationStart, Array(text) + start, static_cast<std::size_t>(count) * sizeof(char16_t));
}

void APS5_VABI _ZN6icu_6813UnicodeString6addRefEv_nid_postfix(String* text) {
    if (References(text).fetch_add(1, std::memory_order_seq_cst) == std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("UnicodeString: reference count overflow");
}

std::int32_t APS5_VABI _ZN6icu_6813UnicodeString9removeRefEv_nid_postfix(String* text) {
    const auto previous = References(text).fetch_sub(1, std::memory_order_seq_cst);
    if (previous == std::numeric_limits<std::int32_t>::min())
        throw std::invalid_argument("UnicodeString: reference count overflow");
    return previous - 1;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(const String* text) {
    return References(text).load(std::memory_order_acquire);
}

void APS5_VABI _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(String* text, std::int32_t newLength) {
    if (!(Flags(text) & String::OpenGetBuffer) || newLength < -1) return;
    const auto capacity = Flags(text) & String::UsingStackBuffer ? 27 : text->storage.fields.capacity;
    if (newLength == -1) {
        if (!Array(text)) throw std::invalid_argument("UnicodeString: null released buffer");
        newLength = 0;
        while (newLength < capacity && Read(text, newLength) != 0) ++newLength;
    } else if (newLength > capacity) newLength = capacity;
    auto& flags = text->storage.fields.lengthAndFlags;
    flags &= 0x1f;
    if (newLength <= 0x3ff) flags = static_cast<std::int16_t>(flags | (static_cast<std::uint32_t>(newLength) << String::LengthShift));
    else {
        flags = static_cast<std::int16_t>(flags | 0xffe0);
        text->storage.fields.length = newLength;
    }
    flags &= ~String::OpenGetBuffer;
}

std::int8_t APS5_VABI _ZN6icu_6813UnicodeString8allocateEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity) {
    return Require(text).Allocate(capacity);
}

void APS5_VABI _ZN6icu_6813UnicodeString12releaseArrayEv_nid_postfix(Icu::Abi::UnicodeString* text) {
    Require(text).Release();
}

void APS5_VABI _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(Icu::Abi::UnicodeString* text) {
    Require(text).Bogus();
}

std::int8_t APS5_VABI _ZN6icu_6813UnicodeString18cloneArrayIfNeededEiiaPPia_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t grow, std::int8_t copy, std::int32_t** deferred, std::int8_t force) {
    return Require(text).CloneBuffer(capacity, grow, copy != 0, deferred, force != 0);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8copyFromERKS0_a_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int8_t fast) {
    return Require(text).CopyFrom(source, fast != 0);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source) {
    return Require(text).CopyFrom(source, false);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString12fastCopyFromERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source) {
    return Require(text).CopyFrom(source, true);
}

void APS5_VABI _ZN6icu_6813UnicodeString14copyFieldsFromERS0_a_nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source, std::int8_t reset) {
    Require(text).CopyFields(source, reset != 0);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeStringaSEOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source) {
    Require(text).Release();
    text->CopyFields(source, true);
    return text;
}

void APS5_VABI _ZN6icu_6813UnicodeString4swapERS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* other) {
    Icu::Abi::UnicodeString temporary;
    temporary.Init();
    temporary.CopyFields(&Require(text), false);
    text->CopyFields(&Require(other), false);
    other->CopyFields(&temporary, false);
}

void APS5_VABI _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(Icu::Abi::UnicodeString* text) {
    Require(text).Release();
    _ZN6icu_6811ReplaceableD2Ev_nid_postfix(text);
}

void APS5_VABI _ZN6icu_6813UnicodeStringD2Ev_nid_postfix(Icu::Abi::UnicodeString* text) {
    Require(text).Release();
    _ZN6icu_6811ReplaceableD2Ev_nid_postfix(text);
}

void APS5_VABI _ZN6icu_6813UnicodeStringD0Ev_nid_postfix(Icu::Abi::UnicodeString* text) {
    Require(text).Release();
    _ZN6icu_6811ReplaceableD2Ev_nid_postfix(text);
    Icu::Abi::Free_nid_no_patch(text);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8doAppendEPKDsii_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t start, std::int32_t count) {
    return Require(text).Append(source, start, count);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count) {
    Require(text);
    if (count == 0) return text;
    Require(source).Pin(start, count);
    return text->Append(source->Array(), start, count);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength) {
    return Require(text).Replace(start, count, source, sourceStart, sourceLength);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReplaceEiiRKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const Icu::Abi::UnicodeString* source, std::int32_t sourceStart, std::int32_t sourceLength) {
    Require(source).Pin(sourceStart, sourceLength);
    return Require(text).Replace(start, count, source->Array(), sourceStart, sourceLength);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t length) {
    Require(text).Init();
    text->Append(source, 0, length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source) {
    Require(text).Init();
    text->Append(source, 0, -1);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit) {
    Require(text).Init();
    text->storage.stack.buffer[0] = unit;
    text->SetLength(1);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1Ei_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point) {
    Require(text).Init();
    if (static_cast<std::uint32_t>(point) <= 0xffff) { text->Array()[0] = static_cast<char16_t>(point); text->SetLength(1); }
    else if (point >= 0x10000 && point <= 0x10ffff) { text->Array()[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); text->Array()[1] = static_cast<char16_t>((point & 1023) | 0xdc00); text->SetLength(2); }
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1Eiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t point, std::int32_t count) {
    Require(text).Init();
    if (count <= 0 || static_cast<std::uint32_t>(point) > 0x10ffff || (point > 0xffff && count > INT32_MAX / 2)) { text->Allocate(capacity); return; }
    const auto length = point <= 0xffff ? count : count * 2;
    if (!text->Allocate(std::max(capacity, length))) return;
    for (std::int32_t index = 0; index < length;) {
        if (point <= 0xffff) text->Array()[index++] = static_cast<char16_t>(point);
        else { text->Array()[index++] = static_cast<char16_t>((point >> 10) + 0xd7c0); text->Array()[index++] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    }
    text->SetLength(length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source) {
    Require(text).Init();
    text->CopyFrom(source, false);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source) {
    Require(text).Init();
    text->CopyFields(source, true);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0_i_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start) {
    Require(text).Init();
    Require(source);
    start = std::clamp(start, 0, source->Length());
    std::int32_t sourceLength = source->Length() - start;
    source->Pin(start, sourceLength);
    text->Replace(0, 0, source->Array(), start, sourceLength);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count) {
    Require(text).Init();
    Require(source);
    std::int32_t sourceLength = count;
    source->Pin(start, sourceLength);
    text->Replace(0, 0, source->Array(), start, sourceLength);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity) {
    Require(text).Init();
    _ZN6icu_6813UnicodeString5setToEPDsii_nid_postfix(text, buffer, length, capacity);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length) {
    Require(text).Init();
    _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(text, terminated, source, length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKciNS0_10EInvariantE_nid_postfix(Icu::Abi::UnicodeString* text, const char* source, std::int32_t length, std::int32_t) {
    Require(text).Init();
    if (!source) return;
    if (length < 0) length = Icu::Abi::CheckedInt_nid_no_patch(std::strlen(source));
    if (text->CloneBuffer(length, length, false)) {
        u_charsToUChars_nid_postfix(source, text->Array(), length);
        text->SetLength(length);
    } else text->Bogus();
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKDsi_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t length) {
    Require(text).Init();
    text->Append(source, 0, length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKDs_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source) {
    Require(text).Init();
    text->Append(source, 0, -1);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit) {
    Require(text).Init();
    text->storage.stack.buffer[0] = unit;
    text->SetLength(1);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2Ei_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point) {
    Require(text).Init();
    if (static_cast<std::uint32_t>(point) <= 0xffff) { text->Array()[0] = static_cast<char16_t>(point); text->SetLength(1); }
    else if (point >= 0x10000 && point <= 0x10ffff) { text->Array()[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); text->Array()[1] = static_cast<char16_t>((point & 1023) | 0xdc00); text->SetLength(2); }
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2Eiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t point, std::int32_t count) {
    Require(text).Init();
    if (count <= 0 || static_cast<std::uint32_t>(point) > 0x10ffff || (point > 0xffff && count > INT32_MAX / 2)) { text->Allocate(capacity); return; }
    const auto length = point <= 0xffff ? count : count * 2;
    if (!text->Allocate(std::max(capacity, length))) return;
    for (std::int32_t index = 0; index < length;) {
        if (point <= 0xffff) text->Array()[index++] = static_cast<char16_t>(point);
        else { text->Array()[index++] = static_cast<char16_t>((point >> 10) + 0xd7c0); text->Array()[index++] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    }
    text->SetLength(length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source) {
    Require(text).Init();
    text->CopyFrom(source, false);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source) {
    Require(text).Init();
    text->CopyFields(source, true);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0_i_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start) {
    Require(text).Init();
    Require(source);
    start = std::clamp(start, 0, source->Length());
    std::int32_t sourceLength = source->Length() - start;
    source->Pin(start, sourceLength);
    text->Replace(0, 0, source->Array(), start, sourceLength);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count) {
    Require(text).Init();
    Require(source);
    std::int32_t sourceLength = count;
    source->Pin(start, sourceLength);
    text->Replace(0, 0, source->Array(), start, sourceLength);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity) {
    Require(text).Init();
    _ZN6icu_6813UnicodeString5setToEPDsii_nid_postfix(text, buffer, length, capacity);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length) {
    Require(text).Init();
    _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(text, terminated, source, length);
}

void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKciNS0_10EInvariantE_nid_postfix(Icu::Abi::UnicodeString* text, const char* source, std::int32_t length, std::int32_t) {
    Require(text).Init();
    if (!source) return;
    if (length < 0) length = Icu::Abi::CheckedInt_nid_no_patch(std::strlen(source));
    if (text->CloneBuffer(length, length, false)) {
        u_charsToUChars_nid_postfix(source, text->Array(), length);
        text->SetLength(length);
    } else text->Bogus();
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity) {
    Require(text);
    if (text->Flags() & Icu::Abi::UnicodeString::OpenGetBuffer) return text;
    if (!buffer) { text->Release(); text->Empty(); return text; }
    if (length < -1 || capacity < 0 || length > capacity) { text->Bogus(); return text; }
    if (length == -1) { length = 0; while (length < capacity && buffer[length]) ++length; }
    text->Release();
    text->storage.fields.lengthAndFlags = Icu::Abi::UnicodeString::WritableAlias;
    text->SetLength(length);
    text->storage.fields.array = buffer;
    text->storage.fields.capacity = capacity;
    return text;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* input, std::int32_t length) {
    Require(text);
    if (text->Flags() & 16) return text;
    const auto* source = Require(input).pointer;
    if (!source) { text->Release(); text->Empty(); return text; }
    if (length < -1 || (length == -1 && !terminated) || (length >= 0 && terminated && source[length])) { text->Bogus(); return text; }
    text->Release();
    if (length == -1) length = Icu::Abi::Utf16Length_nid_no_patch(source);
    const auto capacity = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) + (terminated != 0));
    text->storage.fields.lengthAndFlags = 8;
    text->SetLength(length);
    text->storage.fields.array = const_cast<char16_t*>(source);
    text->storage.fields.capacity = capacity;
    return text;
}

char16_t* APS5_VABI _ZN6icu_6813UnicodeString9getBufferEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity) {
    if (Require(text).Writable() && capacity >= -1 && text->CloneBuffer(capacity)) { text->storage.fields.lengthAndFlags |= 16; text->SetLength(0); return text->Array(); }
    return nullptr;
}

const char16_t* APS5_VABI _ZN6icu_6813UnicodeString19getTerminatedBufferEv_nid_postfix(Icu::Abi::UnicodeString* text) {
    if (!Require(text).Writable()) return nullptr;
    auto* array = text->Array();
    const auto length = text->Length();
    if (length < text->Capacity()) {
        if (text->Flags() & 8) { if (!array) throw std::invalid_argument("UnicodeString: null array"); if (array[length] == 0) return array; }
        else if (!(text->Flags() & 4) || _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(text) == 1) { array[length] = 0; return array; }
    }
    if (length < INT32_MAX && text->CloneBuffer(length + 1)) { text->Array()[length] = 0; return text->Array(); }
    return nullptr;
}

void* APS5_VABI _ZN6icu_6813UnicodeString16getStaticClassIDEv_nid_postfix() {
    return Icu::Abi::ClassId_nid_no_patch(Icu::Abi::Class::UnicodeString);
}

void* APS5_VABI _ZNK6icu_6813UnicodeString17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UnicodeString*) {
    return _ZN6icu_6813UnicodeString16getStaticClassIDEv_nid_postfix();
}

Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString5cloneEv_nid_postfix(const Icu::Abi::UnicodeString* text) {
    auto* copy = static_cast<Icu::Abi::UnicodeString*>(Icu::Abi::Allocate_nid_no_patch(sizeof(Icu::Abi::UnicodeString)));
    if (copy) { copy->Init(); copy->CopyFrom(&Require(text), false); }
    return copy;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit) {
    _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(text);
    return text->Replace(0, text->Length(), &unit, 0, 1);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9setCharAtEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t index, char16_t unit) {
    const auto length = Require(text).Length();
    if (text->CloneBuffer() && length > 0) text->Array()[std::clamp(index, 0, length - 1)] = unit;
    return text;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString6appendEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point) {
    return _ZN6icu_6813UnicodeString7replaceEiii_nid_postfix(text, Require(text).Length(), 0, point);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString7replaceEiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, std::int32_t point) {
    char16_t units[2];
    std::int32_t length = 0;
    if (static_cast<std::uint32_t>(point) <= 0xffff) units[length++] = static_cast<char16_t>(point);
    else if (point >= 0x10000 && point <= 0x10ffff) { units[length++] = static_cast<char16_t>((point >> 10) + 0xd7c0); units[length++] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    return Require(text).Replace(start, count, units, 0, length);
}

std::int8_t APS5_VABI _ZN6icu_6813UnicodeString10padLeadingEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t target, char16_t unit) {
    const auto length = Require(text).Length();
    if (length >= target || !text->CloneBuffer(target)) return 0;
    std::memmove(text->Array() + target - length, text->Array(), length * sizeof(char16_t));
    std::fill_n(text->Array(), target - length, unit);
    text->SetLength(target);
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_6813UnicodeString11padTrailingEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t target, char16_t unit) {
    const auto length = Require(text).Length();
    if (length >= target || !text->CloneBuffer(target)) return 0;
    std::fill(text->Array() + length, text->Array() + target, unit);
    text->SetLength(target);
    return 1;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReverseEii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count) {
    Require(text);
    if (count <= 1 || !text->CloneBuffer()) return text;
    text->Pin(start, count);
    if (count <= 1) return text;
    auto* array = text->Array();
    std::reverse(array + start, array + start + count);
    for (auto index = start; index < start + count - 1; ++index) {
        if (Trail(array[index]) && Lead(array[index + 1])) { std::swap(array[index], array[index + 1]); ++index; }
    }
    return text;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(const Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength, std::int32_t start, std::int32_t count) {
    Require(text);
    if ((text->Flags() & 1) || !source || sourceStart < 0 || sourceLength == 0 || (sourceLength < 0 && source[sourceStart] == 0)) return -1;
    return Search(text, source + sourceStart, sourceLength, start, count, false);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11lastIndexOfEPKDsiiii_nid_postfix(const Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength, std::int32_t start, std::int32_t count) {
    Require(text);
    if ((text->Flags() & 1) || !source || sourceStart < 0 || sourceLength == 0 || (sourceLength < 0 && source[sourceStart] == 0)) return -1;
    return Search(text, source + sourceStart, sourceLength, start, count, true);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9doIndexOfEDsii_nid_postfix(const Icu::Abi::UnicodeString* text, char16_t point, std::int32_t start, std::int32_t count) {
    Require(text);
    if (static_cast<std::uint32_t>(point) > 0x10ffff) return -1;
    char16_t units[2];
    const auto size = point <= 0xffff ? 1 : 2;
    if (size == 1) units[0] = static_cast<char16_t>(point);
    else { units[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); units[1] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    return Search(text, units, size, start, count, false);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9doIndexOfEiii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t point, std::int32_t start, std::int32_t count) {
    Require(text);
    if (static_cast<std::uint32_t>(point) > 0x10ffff) return -1;
    char16_t units[2];
    const auto size = point <= 0xffff ? 1 : 2;
    if (size == 1) units[0] = static_cast<char16_t>(point);
    else { units[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); units[1] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    return Search(text, units, size, start, count, false);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString13doLastIndexOfEDsii_nid_postfix(const Icu::Abi::UnicodeString* text, char16_t point, std::int32_t start, std::int32_t count) {
    Require(text);
    if (static_cast<std::uint32_t>(point) > 0x10ffff) return -1;
    char16_t units[2];
    const auto size = point <= 0xffff ? 1 : 2;
    if (size == 1) units[0] = static_cast<char16_t>(point);
    else { units[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); units[1] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    return Search(text, units, size, start, count, true);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString13doLastIndexOfEiii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t point, std::int32_t start, std::int32_t count) {
    Require(text);
    if (static_cast<std::uint32_t>(point) > 0x10ffff) return -1;
    char16_t units[2];
    const auto size = point <= 0xffff ? 1 : 2;
    if (size == 1) units[0] = static_cast<char16_t>(point);
    else { units[0] = static_cast<char16_t>((point >> 10) + 0xd7c0); units[1] = static_cast<char16_t>((point & 1023) | 0xdc00); }
    return Search(text, units, size, start, count, true);
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString9doCompareEiiPKDsii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength) {
    return Compare(&Require(text), start, count, source, sourceStart, sourceLength, false);
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString23doCompareCodePointOrderEiiPKDsii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength) {
    return Compare(&Require(text), start, count, source, sourceStart, sourceLength, true);
}

std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString7compareENS_14ConstChar16PtrEi_nid_postfix(const Icu::Abi::UnicodeString* text, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length) {
    return Compare(&Require(text), 0, text->Length(), Require(source).pointer, 0, length, false);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7extractENS_9Char16PtrEiR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* text, const Icu::Abi::Char16Ptr* target, std::int32_t capacity, std::int32_t* status) {
    const auto length = Require(text).Length();
    if (Require(status) > 0) return length;
    auto* destination = Require(target).pointer;
    if ((text->Flags() & 1) || capacity < 0 || (capacity > 0 && !destination)) { *status = 1; return length; }
    if (length > 0 && length <= capacity && text->Array() != destination) std::memcpy(destination, text->Array(), static_cast<std::size_t>(length) * 2);
    Terminate(destination, capacity, length, *status);
    return length;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7extractEiiPciNS0_10EInvariantE_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, char* destination, std::int32_t capacity, std::int32_t) {
    if (capacity < 0 || (capacity > 0 && !destination)) return 0;
    Require(text).Pin(start, count);
    if (count <= capacity) u_UCharsToChars_nid_postfix(count ? text->Array() + start : nullptr, destination, count);
    std::int32_t status = 0;
    Terminate(destination, capacity, count, status);
    return count;
}

void APS5_VABI _ZNK6icu_6813UnicodeString14extractBetweenEiiRS0__nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, Icu::Abi::UnicodeString* target) {
    Require(text);
    start = std::clamp(start, 0, text->Length());
    limit = std::clamp(limit, 0, text->Length());
    auto count = limit - start;
    text->Pin(start, count);
    Require(target).Replace(0, target->Length(), text->Array(), start, count);
}

void APS5_VABI _ZN6icu_6813UnicodeString20handleReplaceBetweenEiiRKS0__nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, const Icu::Abi::UnicodeString* source) {
    const auto count = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(limit) - start);
    Require(text).Replace(start, count, Require(source).Array(), 0, source->Length());
}

void APS5_VABI _ZN6icu_6813UnicodeString4copyEiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, std::int32_t destination) {
    Require(text);
    if (limit <= start) return;
    const auto count = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(limit) - start);
    auto* temporary = static_cast<char16_t*>(Icu::Abi::Allocate_nid_no_patch(static_cast<std::size_t>(count) * 2));
    if (!temporary) return;
    auto pinnedStart = std::clamp(start, 0, text->Length());
    auto pinnedCount = std::clamp(limit, 0, text->Length()) - pinnedStart;
    text->Pin(pinnedStart, pinnedCount);
    try {
        if (pinnedCount > 0) std::memmove(temporary, text->Array() + pinnedStart, static_cast<std::size_t>(pinnedCount) * 2);
        if (pinnedCount != count) throw std::invalid_argument("UnicodeString: copying uninitialized extracted units");
        text->Replace(destination, 0, temporary, 0, count);
    } catch (...) { Icu::Abi::Free_nid_no_patch(temporary); throw; }
    Icu::Abi::Free_nid_no_patch(temporary);
}

Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString13tempSubStringEii_nid_postfix(Icu::Abi::UnicodeString* result, const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count) {
    Require(text).Pin(start, count);
    Require(result).Init();
    if (!text->Buffer()) { result->Bogus(); return result; }
    Icu::Abi::ConstChar16Ptr pointer{text->Buffer() + start};
    return _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(result, 0, &pointer, count);
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString14findAndReplaceEiiRKS0_iiS2_ii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const Icu::Abi::UnicodeString* oldText, std::int32_t oldStart, std::int32_t oldCount, const Icu::Abi::UnicodeString* replacement, std::int32_t newStart, std::int32_t newCount) {
    Require(text); Require(oldText); Require(replacement);
    if ((text->Flags() | oldText->Flags() | replacement->Flags()) & 1) return text;
    text->Pin(start, count); oldText->Pin(oldStart, oldCount); replacement->Pin(newStart, newCount);
    if (!oldCount) return text;
    while (count > 0 && count >= oldCount) {
        const auto position = _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(text, oldText->Array(), oldStart, oldCount, start, count);
        if (position < 0) break;
        auto sourceStart = newStart, sourceLength = newCount;
        replacement->Pin(sourceStart, sourceLength);
        text->Replace(position, oldCount, replacement->Array(), sourceStart, sourceLength);
        count -= position + oldCount - start;
        start = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(position) + newCount);
    }
    return text;
}

Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString8unescapeEv_nid_postfix(Icu::Abi::UnicodeString* result, const Icu::Abi::UnicodeString* text) {
    Require(result).Init();
    Require(text);
    if (!result->Allocate(text->Length())) return result;
    const auto* array = text->Buffer();
    std::int32_t previous = 0;
    for (std::int32_t position = 0;;) {
        if (position == text->Length()) { result->Append(array, previous, position - previous); break; }
        if (!array) throw std::invalid_argument("UnicodeString: null escape input");
        if (array[position++] == u'\\') {
            result->Append(array, previous, position - previous - 1);
            const auto point = _ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(text, &position);
            if (point < 0) { if (result->Writable()) result->SetLength(0); break; }
            _ZN6icu_6813UnicodeString6appendEi_nid_postfix(result, point);
            previous = position;
        }
    }
    return result;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9setToUTF8ENS_11StringPieceE_nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::StringPiece input) {
    _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(text);
    if (input.length < -1 || (!input.pointer && input.length != 0)) throw std::invalid_argument("UnicodeString: invalid UTF-8 input");
    auto length = input.length;
    if (length == -1) length = Icu::Abi::CheckedInt_nid_no_patch(std::strlen(input.pointer));
    const auto capacity = input.length <= 27 ? 27 : Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(input.length) + 1);
    auto* destination = _ZN6icu_6813UnicodeString9getBufferEi_nid_postfix(text, capacity);
    std::int32_t written = 0;
    for (std::int32_t index = 0; index < length;) {
        const auto point = DecodeUtf8(input.pointer, length, index);
        const auto width = point <= 0xffff ? 1 : 2;
        if (destination && written < text->Capacity()) destination[written] = static_cast<char16_t>(width == 1 ? point : (point >> 10) + 0xd7c0);
        if (width == 2 && destination && written + 1 < text->Capacity()) destination[written + 1] = static_cast<char16_t>((point & 1023) | 0xdc00);
        written = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(written) + width);
    }
    const bool overflow = written > text->Capacity();
    if (destination && written < text->Capacity()) destination[written] = 0;
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(text, written);
    if (overflow) text->Bogus();
    return text;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8fromUTF8ENS_11StringPieceE_nid_postfix(Icu::Abi::UnicodeString* result, Icu::Abi::StringPiece input) {
    Require(result).Init();
    return _ZN6icu_6813UnicodeString9setToUTF8ENS_11StringPieceE_nid_postfix(result, input);
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString6toUTF8EiiPci_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, char* destination, std::int32_t capacity) {
    Require(text).Pin(start, count);
    const auto* source = text->Buffer();
    std::int32_t status = 0;
    const auto result = EncodeUtf8(source ? source + start : nullptr, count, destination, capacity, status);
    if (status == 1) throw std::invalid_argument("UnicodeString: invalid UTF-8 extraction arguments");
    return result;
}

std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7toUTF32EPiiR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* destination, std::int32_t capacity, std::int32_t* status) {
    if (Require(status) > 0) return 0;
    const auto length = Require(text).Length();
    const auto* source = text->Buffer();
    if ((!source && length) || capacity < 0 || (!destination && capacity > 0)) { *status = 1; return 0; }
    std::int32_t written = 0;
    for (std::int32_t index = 0; index < length;) {
        const auto point = NextPoint(source, length, index);
        if (written < capacity) destination[written] = point;
        ++written;
    }
    Terminate(destination, capacity, written, *status);
    return written;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9fromUTF32EPKii_nid_postfix(Icu::Abi::UnicodeString* result, const std::int32_t* source, std::int32_t length) {
    Require(result).Init();
    if (length < -1 || (!source && length != 0)) throw std::invalid_argument("UnicodeString: invalid UTF-32 input");
    auto capacity = length <= 27 ? 27 : Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) + (length >> 4) + 4);
    auto count = length;
    if (count == -1) { count = 0; while (source[count]) count = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(count) + 1); }
    for (;;) {
        auto* destination = _ZN6icu_6813UnicodeString9getBufferEi_nid_postfix(result, capacity);
        std::int32_t written = 0;
        for (std::int32_t index = 0; index < count; ++index) {
            auto point = source[index];
            if (static_cast<std::uint32_t>(point) > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) point = 0xfffd;
            const auto width = point <= 0xffff ? 1 : 2;
            if (destination && written < result->Capacity()) destination[written] = static_cast<char16_t>(width == 1 ? point : (point >> 10) + 0xd7c0);
            if (width == 2 && destination && written + 1 < result->Capacity()) destination[written + 1] = static_cast<char16_t>((point & 1023) | 0xdc00);
            written = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(written) + width);
        }
        const bool overflow = written > result->Capacity();
        if (destination && written < result->Capacity()) destination[written] = 0;
        _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(result, written);
        if (overflow) { capacity = Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(written) + 1); if (result->Flags() & 1) break; continue; }
        break;
    }
    return result;
}

}
