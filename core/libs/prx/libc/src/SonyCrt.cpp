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

void APS5_VABI SonyDefaultConstraintHandler(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
    std::abort();
}

extern "C" void SonyIgnoreHandlerS_nid_no_patch(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
}

extern "C" void* SonySetConstraintHandlerS_nid_no_patch(void* handler) {
    std::lock_guard<std::mutex> lock(SonyConstraintMutex());
    void* previous = reinterpret_cast<void*>(SonyConstraintHandlerSlot());
    SonyConstraintHandlerSlot() = handler ? reinterpret_cast<SonyConstraintHandler>(handler) : SonyDefaultConstraintHandler;
    return previous;
}

extern "C" void SonyInvokeConstraintHandler_nid_no_patch(const char* message, void* pointer, int error) {
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

}

extern "C" SonyLconv* SonyLocaleconv_nid_no_patch(void) {
    return &SonyAmprStorage();
}
