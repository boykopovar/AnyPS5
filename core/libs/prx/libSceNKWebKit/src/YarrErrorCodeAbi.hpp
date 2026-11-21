#pragma once

#include <cstdint>
#include <type_traits>

namespace NKWebKit::YarrAbi {

enum class ErrorCode : std::uint8_t {
    NoError = 0,
    PatternTooLarge,
    QuantifierOutOfOrder,
    QuantifierWithoutAtom,
    QuantifierTooLarge,
    QuantifierIncomplete,
    CantQuantifyAtom,
    MissingParentheses,
    BracketUnmatched,
    ParenthesesUnmatched,
    ParenthesesTypeInvalid,
    InvalidGroupName,
    DuplicateGroupName,
    CharacterClassUnmatched,
    CharacterClassRangeOutOfOrder,
    CharacterClassRangeInvalid,
    ClassStringDisjunctionUnmatched,
    EscapeUnterminated,
    InvalidUnicodeEscape,
    InvalidUnicodeCodePointEscape,
    InvalidBackreference,
    InvalidNamedBackReference,
    InvalidIdentityEscape,
    InvalidOctalEscape,
    InvalidControlLetterEscape,
    InvalidUnicodePropertyExpression,
    TooManyDisjunctions,
    OffsetTooLarge,
    InvalidRegularExpressionFlags,
    InvalidClassSetOperation,
    NegatedClassSetMayContainStrings,
    InvalidClassSetCharacter,
};

static_assert(sizeof(ErrorCode) == 1);
static_assert(std::is_same_v<std::underlying_type_t<ErrorCode>, std::uint8_t>);
static_assert(static_cast<unsigned>(ErrorCode::TooManyDisjunctions) == 26);
static_assert(static_cast<unsigned>(ErrorCode::InvalidClassSetCharacter) == 31);

}  // namespace NKWebKit::YarrAbi
