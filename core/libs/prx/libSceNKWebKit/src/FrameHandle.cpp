#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

std::uint64_t APS5_VABI WKFrameHandleGetFrameID(const void* frameHandleRef) {
    return NKWebKit::Cast<NKWebKit::FrameHandle>(frameHandleRef, __func__)->frameID;
}

const void* APS5_VABI WKFrameInfoCreateFrameHandleRef(const void* frameInfoRef) {
    const auto* info = NKWebKit::Cast<NKWebKit::FrameInfo>(frameInfoRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>(info->frameID));
}

}
