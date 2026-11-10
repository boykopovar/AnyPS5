#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

const char* APS5_VABI sceLibcInternalSystemErrorCategoryName_nid_no_patch(const void*) noexcept {
    return "system";
}
APS5_EXPORT("dXy+lFOiaQA", sceLibcInternalSystemErrorCategoryName_nid_no_patch);

const char* APS5_VABI sceLibcInternalGenericErrorCategoryName_nid_no_patch(const void*) noexcept {
    return "generic";
}
APS5_EXPORT("xvVR0CBPFAM", sceLibcInternalGenericErrorCategoryName_nid_no_patch);

const char* APS5_VABI sceLibcInternalIostreamErrorCategoryName_nid_no_patch(const void*) noexcept {
    return "iostream";
}
APS5_EXPORT("sb2vivqtLS0", sceLibcInternalIostreamErrorCategoryName_nid_no_patch);

const char* APS5_VABI sceLibcInternalFutureErrorCategoryName_nid_no_patch(const void*) noexcept {
    return "future";
}
APS5_EXPORT("Q0VsWTapQ4M", sceLibcInternalFutureErrorCategoryName_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(const void*) noexcept {
    return 1;
}
APS5_EXPORT("FOsY+JAyXow", sceLibcInternalCodecvtBaseMaxLength_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtBaseEncoding_nid_no_patch(const void*) noexcept {
    return 1;
}
APS5_EXPORT("MwAySqTo+-M", sceLibcInternalCodecvtBaseEncoding_nid_no_patch);

}
