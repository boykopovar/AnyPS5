#include "prx/libSceLibcInternal/include/CodecvtUtfMetadata.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"

extern "C" {

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(const void* self, void* state, char* first, char* last, char** next) {
    return CodecvtUtf32Unshift_nid_no_patch(self, state, first, last, next);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix(const void* self) {
    return CodecvtUtf32Encoding_nid_no_patch(self);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix(const void* self) {
    return CodecvtUtf32MaxLength_nid_no_patch(self);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(const void* self, void* state, char* first, char* last, char** next) {
    return CodecvtUtf16Unshift_nid_no_patch(self, state, first, last, next);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix(const void* self) {
    return CodecvtUtf16MaxLength_nid_no_patch(self);
}

}
