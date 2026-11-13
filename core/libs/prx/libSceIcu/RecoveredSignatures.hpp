#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include <cstdint>

struct UBreakIterator;
struct UCalendar;
struct UCollator;
struct UConstrainedFieldPosition;
struct UDateFormat;
struct UDateIntervalFormat;
struct UDateTimePatternGenerator;
struct UFieldPosition;
struct UFieldPositionIterator;
struct UFormattedDateInterval;
struct UFormattedList;
struct UFormattedNumber;
struct UFormattedValue;
struct UIDNA;
struct UIDNAInfo;
struct UListFormatter;
struct ULocaleDisplayNames;
struct UNumberFormat;
struct UNumberFormatter;
struct UNumberingSystem;
struct UParseError;
struct UPluralRules;
struct URelativeDateTimeFormatter;
struct UResourceBundle;
struct USet;
struct UStringSearch;
struct UText;

using UCalendarDateFields = std::int32_t;
using UDate = double;
using UListFormatterType = std::int32_t;
using UListFormatterWidth = std::int32_t;

enum UColAttribute : std::int32_t {
    UCOL_FRENCH_COLLATION,
    UCOL_ALTERNATE_HANDLING,
    UCOL_CASE_FIRST,
    UCOL_CASE_LEVEL,
    UCOL_NORMALIZATION_MODE,
    UCOL_DECOMPOSITION_MODE = UCOL_NORMALIZATION_MODE,
    UCOL_STRENGTH,
    UCOL_HIRAGANA_QUATERNARY_MODE,
    UCOL_NUMERIC_COLLATION,
    UCOL_ATTRIBUTE_COUNT
};

enum UColAttributeValue : std::int32_t {
    UCOL_DEFAULT = -1,
    UCOL_PRIMARY = 0,
    UCOL_SECONDARY = 1,
    UCOL_TERTIARY = 2,
    UCOL_DEFAULT_STRENGTH = UCOL_TERTIARY,
    UCOL_CE_STRENGTH_LIMIT,
    UCOL_QUATERNARY = 3,
    UCOL_IDENTICAL = 15,
    UCOL_STRENGTH_LIMIT,
    UCOL_OFF = 16,
    UCOL_ON = 17,
    UCOL_SHIFTED = 20,
    UCOL_NON_IGNORABLE = 21,
    UCOL_LOWER_FIRST = 24,
    UCOL_UPPER_FIRST = 25,
    UCOL_ATTRIBUTE_VALUE_COUNT
};

enum UDateTimePatternMatchOptions : std::int32_t {
    UDATPG_MATCH_NO_OPTIONS = 0,
    UDATPG_MATCH_HOUR_FIELD_LENGTH = 1 << 11,
    UDATPG_MATCH_MINUTE_FIELD_LENGTH = 1 << 12,
    UDATPG_MATCH_SECOND_FIELD_LENGTH = 1 << 13,
    UDATPG_MATCH_ALL_FIELDS_LENGTH = (1 << 16) - 1
};

extern "C" {

UCalendar* APS5_VABI ucal_clone_nid_postfix(const UCalendar* cal, std::int32_t* status);
void APS5_VABI ucal_close_nid_postfix(UCalendar* cal);
std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar* cal, UCalendarDateFields field, std::int32_t* status);
std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(
    const char16_t* id,
    std::int32_t len,
    char16_t* result,
    std::int32_t resultCapacity,
    std::int8_t* isSystemID,
    std::int32_t* status);
std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t*);
UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t*);
std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void* calendar, std::int32_t displayNameType,
                                                               const char* locale, char16_t* result,
                                                               std::int32_t resultLength, std::int32_t* status);
UCalendar* APS5_VABI ucal_open_nid_postfix(const char16_t* zoneId, std::int32_t length, const char* locale, std::int32_t calendarType, std::int32_t* status);
void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* cal, UDate date, std::int32_t* status);
void APS5_VABI ucal_setMillis_nid_postfix(UCalendar* cal, UDate dateTime, std::int32_t* status);

void APS5_VABI ucol_close_nid_postfix(UCollator* coll);
std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t attr, std::int32_t* status);
std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator* coll, std::int32_t* dest, std::int32_t destCapacity, std::int32_t* status);
USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator* coll, std::int32_t* status);
UCollator* APS5_VABI ucol_open_nid_postfix(const char* locale, std::int32_t* status);
void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute attr, UColAttributeValue value, std::int32_t* status);

void APS5_VABI udat_close_nid_postfix(UDateFormat*);
std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status);
std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status);
std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t type, std::int32_t index, char16_t* result,
                                                   std::int32_t resultLength, std::int32_t* status);
UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* fmt, std::int8_t localized, char16_t* result, std::int32_t resultLength, std::int32_t* status);

void APS5_VABI udatpg_close_nid_postfix(UDateTimePatternGenerator*);
std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(UDateTimePatternGenerator* dtpg, const char16_t* skeleton, std::int32_t length,
                                                         char16_t* bestPattern, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(
    UDateTimePatternGenerator* dtpg,
    const char16_t* skeleton,
    std::int32_t length,
    UDateTimePatternMatchOptions options,
    char16_t* bestPattern,
    std::int32_t capacity,
    std::int32_t* pErrorCode);
std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const UDateTimePatternGenerator* dtpg,
                                                              std::int32_t field,
                                                              std::int32_t width,
                                                              char16_t* fieldName,
                                                              std::int32_t capacity,
                                                              std::int32_t* status);
std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(UDateTimePatternGenerator* dtpg, const char16_t* pattern, std::int32_t length,
                                                     char16_t* skeleton, std::int32_t capacity, std::int32_t* errorCode);
UDateTimePatternGenerator* APS5_VABI udatpg_open_nid_postfix(const char* locale, std::int32_t* status);

void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter);
void APS5_VABI udtitvfmt_closeResult_nid_postfix(UFormattedDateInterval* result);
void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval*, std::int32_t*);
void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat* formatter, double fromDate, double toDate, UFormattedDateInterval* result, std::int32_t* status);
UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status);
UFormattedDateInterval* APS5_VABI udtitvfmt_openResult_nid_postfix(std::int32_t* status);

std::int32_t APS5_VABI ufieldpositer_next_nid_postfix(UFieldPositionIterator*, std::int32_t*, std::int32_t*);
void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator*);
UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t*);

const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength, std::int32_t* ec);
std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, std::int32_t* ec);

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna);
std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode);
std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode);
UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* pErrorCode);

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* names);
ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char* locale, std::int32_t* contexts, std::int32_t length, std::int32_t* status);

void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt);
void APS5_VABI ulistfmt_closeResult_nid_postfix(UFormattedList* result);
std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                   const std::int32_t* stringLengths, std::int32_t stringCount,
                                                   char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                          const std::int32_t* stringLengths, std::int32_t stringCount,
                                                          UFormattedList* uresult, std::int32_t* status);
UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char* locale, UListFormatterType type,
                                                           UListFormatterWidth width, std::int32_t* status);
UFormattedList* APS5_VABI ulistfmt_openResult_nid_postfix(std::int32_t* status);

UNumberFormat* APS5_VABI unum_clone_nid_postfix(const UNumberFormat* fmt, std::int32_t* status);
void APS5_VABI unum_close_nid_postfix(UNumberFormat*);
std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const UNumberFormat* fmt, const char* number, std::int32_t length,
                                                      char16_t* result, std::int32_t resultLength, UFieldPosition* pos,
                                                      std::int32_t* status);
std::int32_t APS5_VABI unum_formatDouble_nid_postfix(const UNumberFormat* format, double number, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status);
std::int32_t APS5_VABI unum_getSymbol_nid_postfix(const UNumberFormat* format, std::int32_t symbol, char16_t* buffer, std::int32_t size, std::int32_t* status);
UNumberFormat* APS5_VABI unum_open_nid_postfix(std::int32_t style, const char16_t* pattern, std::int32_t patternLength, const char* locale, UParseError* parseError, std::int32_t* status);

void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* formatter);
void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* result);
UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* status);

void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys);
UNumberingSystem* APS5_VABI unumsys_open_nid_postfix(const char* locale, std::int32_t* status);
UNumberingSystem* APS5_VABI unumsys_openByName_nid_postfix(const char* name, std::int32_t* status);

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules);
UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t* status);

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt);
std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter* formatter, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char* locale, UNumberFormat* formatter, std::int32_t width, std::int32_t context, std::int32_t* status);

void APS5_VABI ures_close(UResourceBundle* resB);

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength, const char* locale, UBreakIterator* breakIterator, std::int32_t* status);

UText* APS5_VABI utext_close(UText* ut);

}
