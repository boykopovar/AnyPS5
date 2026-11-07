// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2015, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU error code: null pointer");
    return *value;
}
struct ErrorTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, 3> slots;
};
static_assert(sizeof(ErrorTable) == 40 && alignof(ErrorTable) == 8);
static_assert(offsetof(ErrorTable, slots) == 16);
const GuestFunction* Table() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_689ErrorCodeE"}, reinterpret_cast<const DerivedTypeInfo*>(Rtti_nid_no_patch(Class::Object))->base};
    static const ErrorTable table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_689ErrorCodeD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_689ErrorCodeD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_689ErrorCode13handleFailureEv_nid_postfix),
    }}};
    return table.slots.data();
}
constexpr const char* Standard[]{
    "U_ZERO_ERROR",
    "U_ILLEGAL_ARGUMENT_ERROR",
    "U_MISSING_RESOURCE_ERROR",
    "U_INVALID_FORMAT_ERROR",
    "U_FILE_ACCESS_ERROR",
    "U_INTERNAL_PROGRAM_ERROR",
    "U_MESSAGE_PARSE_ERROR",
    "U_MEMORY_ALLOCATION_ERROR",
    "U_INDEX_OUTOFBOUNDS_ERROR",
    "U_PARSE_ERROR",
    "U_INVALID_CHAR_FOUND",
    "U_TRUNCATED_CHAR_FOUND",
    "U_ILLEGAL_CHAR_FOUND",
    "U_INVALID_TABLE_FORMAT",
    "U_INVALID_TABLE_FILE",
    "U_BUFFER_OVERFLOW_ERROR",
    "U_UNSUPPORTED_ERROR",
    "U_RESOURCE_TYPE_MISMATCH",
    "U_ILLEGAL_ESCAPE_SEQUENCE",
    "U_UNSUPPORTED_ESCAPE_SEQUENCE",
    "U_NO_SPACE_AVAILABLE",
    "U_CE_NOT_FOUND_ERROR",
    "U_PRIMARY_TOO_LONG_ERROR",
    "U_STATE_TOO_OLD_ERROR",
    "U_TOO_MANY_ALIASES_ERROR",
    "U_ENUM_OUT_OF_SYNC_ERROR",
    "U_INVARIANT_CONVERSION_ERROR",
    "U_INVALID_STATE_ERROR",
    "U_COLLATOR_VERSION_MISMATCH",
    "U_USELESS_COLLATOR_ERROR",
    "U_NO_WRITE_PERMISSION",
    "U_INPUT_TOO_LONG_ERROR",
};
constexpr const char* Warnings[]{
    "U_USING_FALLBACK_WARNING",
    "U_USING_DEFAULT_WARNING",
    "U_SAFECLONE_ALLOCATED_WARNING",
    "U_STATE_OLD_WARNING",
    "U_STRING_NOT_TERMINATED_WARNING",
    "U_SORT_KEY_TOO_SHORT_WARNING",
    "U_AMBIGUOUS_ALIAS_WARNING",
    "U_DIFFERENT_UCA_VERSION",
    "U_PLUGIN_CHANGED_LEVEL_WARNING",
};
constexpr const char* Transliterator[]{
    "U_BAD_VARIABLE_DEFINITION",
    "U_MALFORMED_RULE",
    "U_MALFORMED_SET",
    "U_MALFORMED_SYMBOL_REFERENCE",
    "U_MALFORMED_UNICODE_ESCAPE",
    "U_MALFORMED_VARIABLE_DEFINITION",
    "U_MALFORMED_VARIABLE_REFERENCE",
    "U_MISMATCHED_SEGMENT_DELIMITERS",
    "U_MISPLACED_ANCHOR_START",
    "U_MISPLACED_CURSOR_OFFSET",
    "U_MISPLACED_QUANTIFIER",
    "U_MISSING_OPERATOR",
    "U_MISSING_SEGMENT_CLOSE",
    "U_MULTIPLE_ANTE_CONTEXTS",
    "U_MULTIPLE_CURSORS",
    "U_MULTIPLE_POST_CONTEXTS",
    "U_TRAILING_BACKSLASH",
    "U_UNDEFINED_SEGMENT_REFERENCE",
    "U_UNDEFINED_VARIABLE",
    "U_UNQUOTED_SPECIAL",
    "U_UNTERMINATED_QUOTE",
    "U_RULE_MASK_ERROR",
    "U_MISPLACED_COMPOUND_FILTER",
    "U_MULTIPLE_COMPOUND_FILTERS",
    "U_INVALID_RBT_SYNTAX",
    "U_INVALID_PROPERTY_PATTERN",
    "U_MALFORMED_PRAGMA",
    "U_UNCLOSED_SEGMENT",
    "U_ILLEGAL_CHAR_IN_SEGMENT",
    "U_VARIABLE_RANGE_EXHAUSTED",
    "U_VARIABLE_RANGE_OVERLAP",
    "U_ILLEGAL_CHARACTER",
    "U_INTERNAL_TRANSLITERATOR_ERROR",
    "U_INVALID_ID",
    "U_INVALID_FUNCTION",
};
constexpr const char* Format[]{
    "U_UNEXPECTED_TOKEN",
    "U_MULTIPLE_DECIMAL_SEPARATORS",
    "U_MULTIPLE_EXPONENTIAL_SYMBOLS",
    "U_MALFORMED_EXPONENTIAL_PATTERN",
    "U_MULTIPLE_PERCENT_SYMBOLS",
    "U_MULTIPLE_PERMILL_SYMBOLS",
    "U_MULTIPLE_PAD_SPECIFIERS",
    "U_PATTERN_SYNTAX_ERROR",
    "U_ILLEGAL_PAD_POSITION",
    "U_UNMATCHED_BRACES",
    "U_UNSUPPORTED_PROPERTY",
    "U_UNSUPPORTED_ATTRIBUTE",
    "U_ARGUMENT_TYPE_MISMATCH",
    "U_DUPLICATE_KEYWORD",
    "U_UNDEFINED_KEYWORD",
    "U_DEFAULT_KEYWORD_MISSING",
    "U_DECIMAL_NUMBER_SYNTAX_ERROR",
    "U_FORMAT_INEXACT_ERROR",
    "U_NUMBER_ARG_OUTOFBOUNDS_ERROR",
    "U_NUMBER_SKELETON_SYNTAX_ERROR",
};
constexpr const char* Break[]{
    "U_BRK_INTERNAL_ERROR",
    "U_BRK_HEX_DIGITS_EXPECTED",
    "U_BRK_SEMICOLON_EXPECTED",
    "U_BRK_RULE_SYNTAX",
    "U_BRK_UNCLOSED_SET",
    "U_BRK_ASSIGN_ERROR",
    "U_BRK_VARIABLE_REDFINITION",
    "U_BRK_MISMATCHED_PAREN",
    "U_BRK_NEW_LINE_IN_QUOTED_STRING",
    "U_BRK_UNDEFINED_VARIABLE",
    "U_BRK_INIT_ERROR",
    "U_BRK_RULE_EMPTY_SET",
    "U_BRK_UNRECOGNIZED_OPTION",
    "U_BRK_MALFORMED_RULE_TAG",
};
constexpr const char* Regex[]{
    "U_REGEX_INTERNAL_ERROR",
    "U_REGEX_RULE_SYNTAX",
    "U_REGEX_INVALID_STATE",
    "U_REGEX_BAD_ESCAPE_SEQUENCE",
    "U_REGEX_PROPERTY_SYNTAX",
    "U_REGEX_UNIMPLEMENTED",
    "U_REGEX_MISMATCHED_PAREN",
    "U_REGEX_NUMBER_TOO_BIG",
    "U_REGEX_BAD_INTERVAL",
    "U_REGEX_MAX_LT_MIN",
    "U_REGEX_INVALID_BACK_REF",
    "U_REGEX_INVALID_FLAG",
    "U_REGEX_LOOK_BEHIND_LIMIT",
    "U_REGEX_SET_CONTAINS_STRING",
    "U_REGEX_OCTAL_TOO_BIG",
    "U_REGEX_MISSING_CLOSE_BRACKET",
    "U_REGEX_INVALID_RANGE",
    "U_REGEX_STACK_OVERFLOW",
    "U_REGEX_TIME_OUT",
    "U_REGEX_STOPPED_BY_CALLER",
    "U_REGEX_PATTERN_TOO_BIG",
    "U_REGEX_INVALID_CAPTURE_GROUP_NAME",
};
constexpr const char* Idna[]{
    "U_STRINGPREP_PROHIBITED_ERROR",
    "U_STRINGPREP_UNASSIGNED_ERROR",
    "U_STRINGPREP_CHECK_BIDI_ERROR",
    "U_IDNA_STD3_ASCII_RULES_ERROR",
    "U_IDNA_ACE_PREFIX_ERROR",
    "U_IDNA_VERIFICATION_ERROR",
    "U_IDNA_LABEL_TOO_LONG_ERROR",
    "U_IDNA_ZERO_LENGTH_LABEL_ERROR",
    "U_IDNA_DOMAIN_NAME_TOO_LONG_ERROR",
};
constexpr const char* Plugin[]{
    "U_PLUGIN_TOO_HIGH",
    "U_PLUGIN_DIDNT_SET_LEVEL",
};
const char* Name(std::int32_t code) {
    if (code >= 0 && code < 0 + static_cast<std::int32_t>(std::size(Standard))) return Standard[code - 0];
    if (code >= -128 && code < -128 + static_cast<std::int32_t>(std::size(Warnings))) return Warnings[code - -128];
    if (code >= 65536 && code < 65536 + static_cast<std::int32_t>(std::size(Transliterator))) return Transliterator[code - 65536];
    if (code >= 65792 && code < 65792 + static_cast<std::int32_t>(std::size(Format))) return Format[code - 65792];
    if (code >= 66048 && code < 66048 + static_cast<std::int32_t>(std::size(Break))) return Break[code - 66048];
    if (code >= 66304 && code < 66304 + static_cast<std::int32_t>(std::size(Regex))) return Regex[code - 66304];
    if (code >= 66560 && code < 66560 + static_cast<std::int32_t>(std::size(Idna))) return Idna[code - 66560];
    if (code >= 66816 && code < 66816 + static_cast<std::int32_t>(std::size(Plugin))) return Plugin[code - 66816];
    return "[BOGUS UErrorCode]";
}
}

extern "C" {
void APS5_VABI _ZN6icu_689ErrorCodeD1Ev_nid_postfix(Icu::Abi::ErrorCode* self) {
    Require(self).vtable = Table();
}

void APS5_VABI _ZN6icu_689ErrorCodeD2Ev_nid_postfix(Icu::Abi::ErrorCode* self) {
    Require(self).vtable = Table();
}

void APS5_VABI _ZN6icu_689ErrorCodeD0Ev_nid_postfix(Icu::Abi::ErrorCode* self) {
    Require(self).vtable = Table();
    Free_nid_no_patch(self);
}

std::int32_t APS5_VABI _ZN6icu_689ErrorCode5resetEv_nid_postfix(Icu::Abi::ErrorCode* self) {
    auto& object = Require(self);
    const auto result = object.errorCode;
    object.errorCode = 0;
    return result;
}

void APS5_VABI _ZNK6icu_689ErrorCode13handleFailureEv_nid_postfix(const Icu::Abi::ErrorCode*) {
}

void APS5_VABI _ZNK6icu_689ErrorCode13assertSuccessEv_nid_postfix(const Icu::Abi::ErrorCode* self) {
    if (Require(self).errorCode > 0) Virtual_nid_no_patch<void (APS5_VABI *)(const ErrorCode*)>(self, 2)(self);
}

const char* APS5_VABI _ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(const Icu::Abi::ErrorCode* self) {
    return Name(Require(self).errorCode);
}

}
