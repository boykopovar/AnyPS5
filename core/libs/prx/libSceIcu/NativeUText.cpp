#include "prx/libSceIcu/NativeUText.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/utext.h"
#include <cstdint>

namespace {

constexpr std::int32_t OpenFlag = 4;

const UTextFuncs* UCharsFuncs() {
    static const UTextFuncs* const funcs = [] {
        UText text = UTEXT_INITIALIZER;
        UErrorCode status = U_ZERO_ERROR;
        utext_openUChars(&text, u"", 0, &status);
        const UTextFuncs* table = text.pFuncs;
        utext_close(&text);
        return table;
    }();
    return funcs;
}

void RequireNativeProvider(const UText* ut, const char* function) {
    if (!ut || ut->magic != UTEXT_MAGIC || (ut->flags & OpenFlag) == 0) return;
    if (ut->pFuncs == UCharsFuncs() || (ut->pFuncs && !ut->pFuncs->close)) return;
    NotImplemented_nid_no_patch(function);
}

UErrorCode* Native(std::int32_t& status) {
    return reinterpret_cast<UErrorCode*>(&status);
}

}

namespace Icu {

UText* NativeUTextSetup(UText* ut, std::int32_t extraSpace, std::int32_t& status) {
    if (U_SUCCESS(static_cast<UErrorCode>(status))) RequireNativeProvider(ut, "utext_setup: guest UText provider close");
    return utext_setup(ut, extraSpace, Native(status));
}

UText* NativeUTextOpenUChars(UText* ut, const char16_t* text, std::int64_t length, std::int32_t& status) {
    if (U_SUCCESS(static_cast<UErrorCode>(status))) RequireNativeProvider(ut, "utext_openUChars: guest UText provider close");
    return utext_openUChars(ut, text, length, Native(status));
}

UText* NativeUTextClose(UText* ut) {
    RequireNativeProvider(ut, "utext_close: guest UText provider close");
    return utext_close(ut);
}

}
