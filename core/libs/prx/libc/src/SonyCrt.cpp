#include "prx/libc/include/SonyCrt.hpp"

#include <climits>
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
    (void)pointer;
    (void)error;
    fputs_nid_postfix(message, __stderrp_nid_postfix);
    fputc_nid_postfix('\n', __stderrp_nid_postfix);
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

extern "C" SonyLconv* SonyLocaleconv_nid_no_patch(void) {
    static char empty[] = "";
    static char point[] = ".";
    static char falseText[] = "false";
    static char trueText[] = "true";
    static SonyLconv value = [] {
        SonyLconv init{};
        for (auto& pointer : init.monetary) pointer = empty;
        for (auto& byte : init.chars) byte = CHAR_MAX;
        init.decimal_point = point;
        init.grouping = empty;
        init.thousands_sep = empty;
        init.frac_grouping = empty;
        init.frac_sep = empty;
        init.false_text = falseText;
        init.true_text = trueText;
        init.no_text = empty;
        init.yes_text = empty;
        return init;
    }();
    return &value;
}
