#include "prx/libSceIcu/NativeUText.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UText* APS5_VABI utext_close(UText* ut) {
    return Icu::NativeUTextClose(ut);
}

UText* APS5_VABI utext_openUChars(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    return Icu::NativeUTextOpenUChars(ut, s, length, *status);
}

UText* APS5_VABI utext_setup(UText* ut, std::int32_t extraSpace, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    return Icu::NativeUTextSetup(ut, extraSpace, *status);
}

}
