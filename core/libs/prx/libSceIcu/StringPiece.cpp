// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace {

using Piece = Icu::Abi::StringPiece;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("StringPiece: null pointer");
    return *value;
}

void Set(Piece* text, const char* source) {
    Require(text).pointer = source;
    text->length = source ? std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(std::strlen(source))) : 0;
}

void Slice(Piece* text, const Piece* source, std::int32_t position, std::int32_t count, bool hasCount) {
    const auto length = Require(source).length;
    if (position < 0) position = 0;
    else if (position > length) position = length;
    if (!source->pointer) throw std::invalid_argument("StringPiece: null slice buffer");
    if (hasCount) {
        if (count < 0) count = 0;
        else if (count > length - position) count = length - position;
    } else count = length - position;
    Require(text).pointer = source->pointer + position;
    text->length = count;
}

}

extern "C" {

void APS5_VABI _ZN6icu_6811StringPieceC1EPKc_nid_postfix(Piece* text, const char* source) {
    Set(text, source);
}

void APS5_VABI _ZN6icu_6811StringPieceC2EPKc_nid_postfix(Piece* text, const char* source) {
    Set(text, source);
}

void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(Piece* text, const Piece* source, std::int32_t position) {
    Slice(text, source, position, 0, false);
}

void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix(Piece* text, const Piece* source, std::int32_t position) {
    Slice(text, source, position, 0, false);
}

void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(Piece* text, const Piece* source, std::int32_t position, std::int32_t count) {
    Slice(text, source, position, count, true);
}

void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(Piece* text, const Piece* source, std::int32_t position, std::int32_t count) {
    Slice(text, source, position, count, true);
}

void APS5_VABI _ZN6icu_6811StringPiece3setEPKc_nid_postfix(Piece* text, const char* source) {
    Set(text, source);
}

std::int32_t APS5_VABI _ZN6icu_6811StringPiece7compareES0__nid_postfix(const Piece* text, Piece other) {
    const auto length = Require(text).length;
    std::int32_t index = 0;
    for (; index < length; ++index) {
        if (index == other.length) return 1;
        if (!text->pointer || !other.pointer) throw std::invalid_argument("StringPiece: null comparison buffer");
        const auto first = static_cast<std::int8_t>(text->pointer[index]);
        const auto second = static_cast<std::int8_t>(other.pointer[index]);
        if (first != second) return first < second ? -1 : 1;
    }
    return index < other.length ? -1 : 0;
}

}
