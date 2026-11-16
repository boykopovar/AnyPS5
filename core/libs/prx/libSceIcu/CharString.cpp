// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <array>
#include <limits>

namespace {

using String = Icu::Abi::CharString;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("CharString: null pointer");
    return *value;
}

}

namespace {
std::int32_t Checked(std::int64_t value) { return Icu::Abi::CheckedInt_nid_no_patch(value); }
void Initialize(String* text) {
    Require(text);
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&text->buffer);
    text->length = 0;
    text->buffer.pointer[0] = 0;
}
bool Invariant(char16_t unit) {
    constexpr std::array<std::uint32_t, 4> bits{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};
    return unit <= 0x7f && (bits[unit >> 5] & (1u << (unit & 31)));
}
}
extern "C" {

String* APS5_VABI _ZN6icu_6810CharString8truncateEi_nid_postfix(String* text, std::int32_t newLength) {
    newLength = std::max(newLength, 0);
    if (newLength < Require(text).length) {
        if (!text->buffer.pointer) throw std::invalid_argument("CharString: null buffer");
        text->length = newLength;
        text->buffer.pointer[newLength] = 0;
    }
    return text;
}

std::int32_t APS5_VABI _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(const String* text, char character) {
    auto index = Require(text).length;
    if (index > 0 && !text->buffer.pointer) throw std::invalid_argument("CharString: null buffer");
    while (index > 0) if (text->buffer.pointer[--index] == character) return index;
    return -1;
}

char APS5_VABI _ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(const String*) {
    return '/';
}

std::int32_t APS5_VABI _ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(const String* text, char* destination, std::int32_t capacity, std::int32_t* status) {
    const auto length = Require(text).length;
    if (Require(status) > 0) return length;
    if (capacity < 0 || (capacity > 0 && !destination)) {
        *status = 1;
        return length;
    }
    if (length > 0 && length <= capacity && text->buffer.pointer != destination) {
        if (!text->buffer.pointer) throw std::invalid_argument("CharString: null buffer");
        std::memcpy(destination, text->buffer.pointer, static_cast<std::size_t>(length));
    }
    if (length >= 0) {
        if (length < capacity) {
            destination[length] = 0;
            if (*status == -124) *status = 0;
        } else *status = length == capacity ? -124 : 15;
    }
    return length;
}

std::int8_t APS5_VABI _ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t capacity, std::int32_t desired, std::int32_t* status) {
    if (Require(status) > 0) return 0;
    Require(text);
    if (capacity > text->buffer.capacity) {
        if (!desired) desired = Checked(static_cast<std::int64_t>(capacity) + text->buffer.capacity);
        const auto length = Checked(static_cast<std::int64_t>(text->length) + 1);
        if ((desired <= capacity || !_ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&text->buffer, desired, length)) && !_ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&text->buffer, capacity, length)) { *status = 7; return 0; }
    }
    return 1;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, char unit, std::int32_t* status) {
    Require(text);
    if (_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(text, Checked(static_cast<std::int64_t>(text->length) + 2), 0, status)) {
        if (!text->buffer.pointer) throw std::invalid_argument("CharString: null array");
        text->buffer.pointer[text->length++] = unit;
        text->buffer.pointer[text->length] = 0;
    }
    return text;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const char* source, std::int32_t count, std::int32_t* status) {
    if (Require(status) > 0) return text;
    Require(text);
    if (count < -1 || (!source && count != 0)) { *status = 1; return text; }
    if (count < 0) count = Checked(std::strlen(source));
    if (count <= 0) return text;
    auto* buffer = text->buffer.pointer;
    if (!buffer) throw std::invalid_argument("CharString: null array");
    if (source == buffer + text->length) {
        if (count >= text->buffer.capacity - text->length) *status = 5;
        else { text->length += count; buffer[text->length] = 0; }
        return text;
    }
    const auto input = reinterpret_cast<std::uintptr_t>(source), own = reinterpret_cast<std::uintptr_t>(buffer);
    if (own <= input && input < own + text->length && count >= text->buffer.capacity - text->length) {
        String copy;
        Initialize(&copy);
        try {
            _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&copy, source, count, status);
            _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(text, copy.buffer.pointer, copy.length, status);
        } catch (...) { _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&copy.buffer); throw; }
        _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&copy.buffer);
        return text;
    }
    if (_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(text, Checked(static_cast<std::int64_t>(text->length) + count + 1), 0, status)) {
        auto* destination = text->buffer.pointer + text->length;
        const auto target = reinterpret_cast<std::uintptr_t>(destination);
        if (input < target + count && target < input + count) throw std::invalid_argument("CharString: overlapping append");
        std::memcpy(destination, source, count);
        text->length += count;
        text->buffer.pointer[text->length] = 0;
    }
    return text;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString8copyFromERKS0_R10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const Icu::Abi::CharString* source, std::int32_t* status) {
    if (Require(status) <= 0 && text != source) {
        Require(text); Require(source);
        const auto capacity = Checked(static_cast<std::int64_t>(source->length) + 1);
        if (_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(text, capacity, 0, status)) {
            text->length = source->length;
            if (!text->buffer.pointer || !source->buffer.pointer) throw std::invalid_argument("CharString: null array");
            std::memcpy(text->buffer.pointer, source->buffer.pointer, capacity);
        }
    }
    return text;
}

void APS5_VABI _ZN6icu_6810CharStringC1EOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source) {
    Require(text); Require(source);
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(&text->buffer, &source->buffer);
    text->length = source->length;
    source->length = 0;
}

void APS5_VABI _ZN6icu_6810CharStringC2EOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source) {
    Require(text); Require(source);
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(&text->buffer, &source->buffer);
    text->length = source->length;
    source->length = 0;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharStringaSEOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source) {
    Require(text); Require(source);
    _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(&text->buffer, &source->buffer);
    text->length = source->length;
    source->length = 0;
    return text;
}

bool APS5_VABI _ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(const Icu::Abi::CharString* text, Icu::Abi::StringPiece piece) {
    if (piece.length == 0) return false;
    Require(text);
    if (piece.length < 0) throw std::invalid_argument("CharString: negative search length");
    const auto last = Checked(static_cast<std::int64_t>(text->length) - piece.length);
    for (std::int32_t index = 0; index <= last; ++index) {
        if (!text->buffer.pointer || !piece.pointer) throw std::invalid_argument("CharString: null search array");
        if (!std::memcmp(text->buffer.pointer + index, piece.pointer, piece.length)) return true;
    }
    return false;
}

char* APS5_VABI _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t minimum, std::int32_t desired, std::int32_t* result, std::int32_t* status) {
    Require(result);
    if (Require(status) > 0) { *result = 0; return nullptr; }
    Require(text);
    const auto available = Checked(static_cast<std::int64_t>(text->buffer.capacity) - text->length - 1);
    if (available >= minimum) { *result = available; if (!text->buffer.pointer) throw std::invalid_argument("CharString: null array"); return text->buffer.pointer + text->length; }
    if (_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(text, Checked(static_cast<std::int64_t>(text->length) + minimum + 1), Checked(static_cast<std::int64_t>(text->length) + desired + 1), status)) {
        *result = text->buffer.capacity - text->length - 1;
        return text->buffer.pointer + text->length;
    }
    *result = 0;
    return nullptr;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const char16_t* source, std::int32_t length, std::int32_t* status) {
    if (Require(status) > 0) return text;
    if (!source && length != 0) throw std::invalid_argument("CharString: null invariant input");
    const auto scanned = length < 0 ? Icu::Abi::Utf16Length_nid_no_patch(source) : length;
    for (std::int32_t index = 0; index < scanned; ++index) if (!Invariant(source[index])) { *status = 26; return text; }
    Require(text);
    if (_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(text, Checked(static_cast<std::int64_t>(text->length) + length + 1), 0, status)) {
        const auto finalLength = Checked(static_cast<std::int64_t>(text->length) + length);
        if (finalLength < 0 || !text->buffer.pointer) throw std::invalid_argument("CharString: invalid invariant append length");
        u_UCharsToChars_nid_postfix(source, text->buffer.pointer + text->length, length);
        text->length = finalLength;
        text->buffer.pointer[text->length] = 0;
    }
    return text;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsERKNS_13UnicodeStringER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const Icu::Abi::UnicodeString* source, std::int32_t* status) {
    Require(source);
    return _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(text, source->Buffer(), source->Length(), status);
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t* status) {
    if (Require(status) <= 0 && Require(text).length > 0) {
        if (!text->buffer.pointer) throw std::invalid_argument("CharString: null array");
        const char c = text->buffer.pointer[text->length - 1];
        if (c != '/') {
            const char sep = _ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(text);
            _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(text, sep, status);
        }
    }
    return text;
}

Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, Icu::Abi::StringPiece piece, std::int32_t* status) {
    if (Require(status) > 0 || piece.length == 0) return text;
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(text, status);
    return _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(text, piece.pointer, piece.length, status);
}

}
