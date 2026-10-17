#include "prx/libc/include/SonyCrt.hpp"

#include <climits>
#include <cstdlib>
#include <mutex>

std::mutex& SonyConstraintMutex() {
    static std::mutex mutex;
    return mutex;
}

SonyConstraintHandler& SonyConstraintHandlerSlot() {
    static SonyConstraintHandler handler = SonyDefaultConstraintHandler;
    return handler;
}

void SonyDefaultConstraintHandler(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
    std::abort();
}

void SonyIgnoreHandlerS(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
}

void* SonySetConstraintHandlerS(void* handler) {
    std::lock_guard<std::mutex> lock(SonyConstraintMutex());
    void* previous = reinterpret_cast<void*>(SonyConstraintHandlerSlot());
    SonyConstraintHandlerSlot() = handler ? reinterpret_cast<SonyConstraintHandler>(handler) : SonyDefaultConstraintHandler;
    return previous;
}

void SonyInvokeConstraintHandler(const char* message, void* pointer, int error) {
    SonyConstraintHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(SonyConstraintMutex());
        handler = SonyConstraintHandlerSlot();
    }
    if (handler) handler(message, pointer, error);
}

namespace {

char g_sonyLconvEmpty[] = "";
char g_sonyLconvPoint[] = ".";
char g_sonyLconvFalseText[] = "false";
char g_sonyLconvTrueText[] = "true";

}

// Fixed C-locale table (same values the header used to build per TU). One
// definition here is shared by libc and libSceAmpr.
SonyLconv& SonyAmprStorage() {
    static SonyLconv value = [] {
        SonyLconv init{};
        for (auto& pointer : init.monetary) pointer = g_sonyLconvEmpty;
        for (auto& byte : init.chars) byte = CHAR_MAX;
        init.decimal_point = g_sonyLconvPoint;
        init.grouping = g_sonyLconvEmpty;
        init.thousands_sep = g_sonyLconvEmpty;
        init.frac_grouping = g_sonyLconvEmpty;
        init.frac_sep = g_sonyLconvEmpty;
        init.false_text = g_sonyLconvFalseText;
        init.true_text = g_sonyLconvTrueText;
        init.no_text = g_sonyLconvEmpty;
        init.yes_text = g_sonyLconvEmpty;
        return init;
    }();
    return value;
}

SonyLconvCompact& SonyCompactStorage() {
    static SonyLconvCompact value = [] {
        SonyLconvCompact init{};
        init.decimal_point = g_sonyLconvPoint;
        init.thousands_sep = g_sonyLconvEmpty;
        init.grouping = g_sonyLconvEmpty;
        init.int_curr_symbol = g_sonyLconvEmpty;
        init.currency_symbol = g_sonyLconvEmpty;
        init.mon_decimal_point = g_sonyLconvEmpty;
        init.mon_thousands_sep = g_sonyLconvEmpty;
        init.mon_grouping = g_sonyLconvEmpty;
        init.positive_sign = g_sonyLconvEmpty;
        init.negative_sign = g_sonyLconvEmpty;
        init.int_frac_digits = CHAR_MAX;
        init.frac_digits = CHAR_MAX;
        init.p_cs_precedes = CHAR_MAX;
        init.p_sep_by_space = CHAR_MAX;
        init.n_cs_precedes = CHAR_MAX;
        init.n_sep_by_space = CHAR_MAX;
        init.p_sign_posn = CHAR_MAX;
        init.n_sign_posn = CHAR_MAX;
        init.int_p_cs_precedes = CHAR_MAX;
        init.int_p_sep_by_space = CHAR_MAX;
        init.int_n_cs_precedes = CHAR_MAX;
        init.int_n_sep_by_space = CHAR_MAX;
        init.int_p_sign_posn = CHAR_MAX;
        init.int_n_sign_posn = CHAR_MAX;
        return init;
    }();
    return value;
}

SonyLconv* SonyLocaleconv(void) {
    return &SonyAmprStorage();
}

void* SonyLconvFor(SonyLconvVariant variant) {
    if (variant == SonyLconvVariant::Compact0) return static_cast<void*>(&SonyCompactStorage());
    return static_cast<void*>(&SonyAmprStorage());
}

void* SonyLocaleconvEx(SonyLconvVariant variant) {
    return SonyLconvFor(variant);
}
