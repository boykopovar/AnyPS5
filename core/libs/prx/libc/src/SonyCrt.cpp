#include "prx/libc/include/SonyCrt.hpp"

#include <climits>
#include <cstring>
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
    if (__stderrp_nid_postfix == nullptr) return;
    if (message != nullptr) fputs_nid_postfix(message, __stderrp_nid_postfix);
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
    thread_local char empty[] = "";
    thread_local char point[] = ".";
    thread_local char falseText[] = "false";
    thread_local char trueText[] = "true";
    thread_local SonyLconv value{};

    std::memcpy(empty, "", sizeof(empty));
    std::memcpy(point, ".", sizeof(point));
    std::memcpy(falseText, "false", sizeof(falseText));
    std::memcpy(trueText, "true", sizeof(trueText));
    value = SonyLconv{};
    for (auto& pointer : value.monetary) pointer = empty;
    for (auto& byte : value.chars) byte = CHAR_MAX;
    value.decimal_point = point;
    value.grouping = empty;
    value.thousands_sep = empty;
    value.frac_grouping = empty;
    value.frac_sep = empty;
    value.false_text = falseText;
    value.true_text = trueText;
    value.no_text = empty;
    value.yes_text = empty;
    return &value;
}
