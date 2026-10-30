#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "uprops.h"
#include <cstdint>

namespace {

struct CharTypeEnumeration {
    Icu::CharTypeRange range;
    const void* context;
};

UBool U_CALLCONV ForwardCharTypeRange(const void* context, UChar32 start, UChar32 limit, UCharCategory type) {
    const auto& enumeration = *static_cast<const CharTypeEnumeration*>(context);
    return enumeration.range(enumeration.context, start, limit, type) != 0;
}

bool FromDataFile(UPropertySource source) {
    switch (source) {
    case UPROPS_SRC_NFKC:
    case UPROPS_SRC_NFKC_CF:
    case UPROPS_SRC_INPC:
    case UPROPS_SRC_INSC:
    case UPROPS_SRC_VO:
    case UPROPS_SRC_EMOJI: return true;
    default: return false;
    }
}

bool LayoutProperty(UPropertySource source) {
    return source == UPROPS_SRC_INPC || source == UPROPS_SRC_INSC || source == UPROPS_SRC_VO;
}

UProperty Property(std::int32_t which) {
    return static_cast<UProperty>(which);
}

}

extern "C" {

std::int8_t APS5_VABI u_charType_nid_postfix(std::int32_t character) {
    return u_charType(character);
}

void APS5_VABI u_enumCharTypes_nid_postfix(Icu::CharTypeRange range, const void* context) {
    if (!range) return;
    const CharTypeEnumeration enumeration{range, context};
    u_enumCharTypes(ForwardCharTypeRange, &enumeration);
}

std::int8_t APS5_VABI u_islower_nid_postfix(std::int32_t character) {
    return u_islower(character);
}

std::int8_t APS5_VABI u_isupper_nid_postfix(std::int32_t character) {
    return u_isupper(character);
}

std::int8_t APS5_VABI u_istitle_nid_postfix(std::int32_t character) {
    return u_istitle(character);
}

std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t character) {
    return u_isdigit(character);
}

std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t character) {
    return u_isxdigit(character);
}

std::int8_t APS5_VABI u_isalpha_nid_postfix(std::int32_t character) {
    return u_isalpha(character);
}

std::int8_t APS5_VABI u_isalnum_nid_postfix(std::int32_t character) {
    return u_isalnum(character);
}

std::int8_t APS5_VABI u_isdefined_nid_postfix(std::int32_t character) {
    return u_isdefined(character);
}

std::int8_t APS5_VABI u_isbase_nid_postfix(std::int32_t character) {
    return u_isbase(character);
}

std::int8_t APS5_VABI u_iscntrl_nid_postfix(std::int32_t character) {
    return u_iscntrl(character);
}

std::int8_t APS5_VABI u_ispunct_nid_postfix(std::int32_t character) {
    return u_ispunct(character);
}

std::int8_t APS5_VABI u_isgraph_nid_postfix(std::int32_t character) {
    return u_isgraph(character);
}

std::int8_t APS5_VABI u_isprint_nid_postfix(std::int32_t character) {
    return u_isprint(character);
}

std::int8_t APS5_VABI u_isIDStart_nid_postfix(std::int32_t character) {
    return u_isIDStart(character);
}

std::int8_t APS5_VABI u_isIDPart_nid_postfix(std::int32_t character) {
    return u_isIDPart(character);
}

std::int8_t APS5_VABI u_isIDIgnorable_nid_postfix(std::int32_t character) {
    return u_isIDIgnorable(character);
}

std::int8_t APS5_VABI u_isJavaIDStart_nid_postfix(std::int32_t character) {
    return u_isJavaIDStart(character);
}

std::int8_t APS5_VABI u_isJavaIDPart_nid_postfix(std::int32_t character) {
    return u_isJavaIDPart(character);
}

std::int8_t APS5_VABI u_isUAlphabetic_nid_postfix(std::int32_t character) {
    return u_isUAlphabetic(character);
}

std::int8_t APS5_VABI u_isULowercase_nid_postfix(std::int32_t character) {
    return u_isULowercase(character);
}

std::int8_t APS5_VABI u_isUUppercase_nid_postfix(std::int32_t character) {
    return u_isUUppercase(character);
}

std::int32_t APS5_VABI u_charDigitValue_nid_postfix(std::int32_t character) {
    return u_charDigitValue(character);
}

std::int32_t APS5_VABI u_digit_nid_postfix(std::int32_t character, std::int8_t radix) {
    return u_digit(character, radix);
}

double APS5_VABI u_getNumericValue_nid_postfix(std::int32_t character) {
    return u_getNumericValue(character);
}

void APS5_VABI u_charAge_nid_postfix(std::int32_t character, std::uint8_t* version) {
    u_charAge(character, version);
}

std::uint8_t APS5_VABI u_getCombiningClass_nid_postfix(std::int32_t character) {
    return u_getCombiningClass(character);
}

std::int32_t APS5_VABI u_charDirection_nid_postfix(std::int32_t character) {
    return u_charDirection(character);
}

std::int8_t APS5_VABI u_isMirrored_nid_postfix(std::int32_t character) {
    return u_isMirrored(character);
}

std::int32_t APS5_VABI u_charMirror_nid_postfix(std::int32_t character) {
    return u_charMirror(character);
}

std::int32_t APS5_VABI u_getBidiPairedBracket_nid_postfix(std::int32_t character) {
    return u_getBidiPairedBracket(character);
}

std::int32_t APS5_VABI u_foldCase_nid_postfix(std::int32_t character, std::uint32_t options) {
    return u_foldCase(character, options);
}

std::int32_t APS5_VABI u_getPropertyEnum_nid_postfix(const char* alias) {
    return u_getPropertyEnum(alias);
}

const char* APS5_VABI u_getPropertyName_nid_postfix(std::int32_t property, std::int32_t choice) {
    return u_getPropertyName(Property(property), static_cast<UPropertyNameChoice>(choice));
}

std::int32_t APS5_VABI u_getPropertyValueEnum_nid_postfix(std::int32_t property, const char* alias) {
    return u_getPropertyValueEnum(Property(property), alias);
}

const char* APS5_VABI u_getPropertyValueName_nid_postfix(std::int32_t property, std::int32_t value, std::int32_t choice) {
    return u_getPropertyValueName(Property(property), value, static_cast<UPropertyNameChoice>(choice));
}

std::int32_t APS5_VABI u_getIntPropertyMinValue_nid_postfix(std::int32_t property) {
    return u_getIntPropertyMinValue(Property(property));
}

std::int32_t APS5_VABI u_getIntPropertyMaxValue_nid_postfix(std::int32_t property) {
    if (property >= UCHAR_INT_START && property < UCHAR_INT_LIMIT && LayoutProperty(uprops_getSource(Property(property))))
        NotImplemented_nid_no_patch("u_getIntPropertyMaxValue: property from a data file");
    return u_getIntPropertyMaxValue(Property(property));
}

std::int8_t APS5_VABI u_hasBinaryProperty_nid_postfix(std::int32_t character, std::int32_t property) {
    if (property >= UCHAR_BINARY_START && property < UCHAR_BINARY_LIMIT && FromDataFile(uprops_getSource(Property(property))))
        NotImplemented_nid_no_patch("u_hasBinaryProperty: property from a data file");
    return u_hasBinaryProperty(character, Property(property));
}

std::int32_t APS5_VABI u_getIntPropertyValue_nid_postfix(std::int32_t character, std::int32_t property) {
    if (property >= UCHAR_BINARY_START && property < UCHAR_INT_LIMIT && FromDataFile(uprops_getSource(Property(property))))
        NotImplemented_nid_no_patch("u_getIntPropertyValue: property from a data file");
    return u_getIntPropertyValue(character, Property(property));
}

}
