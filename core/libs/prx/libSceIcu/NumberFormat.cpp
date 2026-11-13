#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t FormatSymbolCount = 28;

constexpr std::int32_t FormatStyleCount = 17;

} // namespace

extern "C" {

UNumberFormat* APS5_VABI unum_clone_nid_postfix(const UNumberFormat*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unum_close_nid_postfix(UNumberFormat* fmt) {
    if (!fmt) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const UNumberFormat*, const char*, std::int32_t,
                                                      char16_t* result, std::int32_t resultLength, UFieldPosition*,
                                                      std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if ((result == nullptr && resultLength != 0) || resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI unum_formatDouble_nid_postfix(const UNumberFormat*, double, char16_t*, std::int32_t, UFieldPosition*, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI unum_getSymbol_nid_postfix(const UNumberFormat* format, std::int32_t symbol, char16_t*, std::int32_t, std::int32_t* status) {
    if (status == nullptr) return 0;
    if (Icu::Failure(status)) return 0;
    if (format == nullptr || symbol < 0 || symbol >= FormatSymbolCount) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

UNumberFormat* APS5_VABI unum_open_nid_postfix(std::int32_t style, const char16_t*, std::int32_t, const char*, UParseError*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (style < 0 || style >= FormatStyleCount) {
        *status = Icu::UnsupportedError;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI unum_getTextAttribute_nid_postfix(const UNumberFormat*, std::int32_t, char16_t*, std::int32_t, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unum_setAttribute_nid_postfix(UNumberFormat*, std::int32_t, std::int32_t) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unum_setTextAttribute_nid_postfix(UNumberFormat*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

}
