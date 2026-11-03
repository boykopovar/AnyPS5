#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>

namespace {

void ValidateReason(std::int32_t reason) {
    if (reason < 0 || reason > 7) APS5_INVALID_ARG_EX;
}

void ClearError(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    *error = 0;
}

bool IsDefaultIgnorable(std::int32_t codePoint) {
    constexpr std::array<std::array<std::int32_t, 2>, 17> ranges{{
        {0x00ad, 0x00ad}, {0x034f, 0x034f}, {0x061c, 0x061c},
        {0x115f, 0x1160}, {0x17b4, 0x17b5}, {0x180b, 0x180f},
        {0x200b, 0x200f}, {0x202a, 0x202e}, {0x2060, 0x206f},
        {0x3164, 0x3164}, {0xfe00, 0xfe0f}, {0xfeff, 0xfeff},
        {0xffa0, 0xffa0}, {0xfff0, 0xfff8}, {0x1bca0, 0x1bca3},
        {0x1d173, 0x1d17a}, {0xe0000, 0xe0fff}
    }};
    for (const auto& range : ranges)
        if (codePoint >= range[0] && codePoint <= range[1]) return true;
    return false;
}

bool ContextSkips(const void* context, std::int32_t reason) {
    return !context || (*static_cast<const char*>(context) == 'i' && reason == 0);
}

}

extern "C" {

void APS5_VABI UCNV_FROM_U_CALLBACK_STOP_nid_postfix(const void*, Icu::ConverterFromUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason == 0 && IsDefaultIgnorable(codePoint)) ClearError(error);
}

void APS5_VABI UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason <= 2 && ((reason == 0 && IsDefaultIgnorable(codePoint)) || ContextSkips(context, reason))) ClearError(error);
}

void APS5_VABI UCNV_TO_U_CALLBACK_STOP_nid_postfix(const void*, Icu::ConverterToUnicodeArgs*, const char*, std::int32_t, std::int32_t reason, std::int32_t*) {
    ValidateReason(reason);
}

void APS5_VABI UCNV_TO_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs*, const char*, std::int32_t, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason <= 2 && ContextSkips(context, reason)) ClearError(error);
}

}
