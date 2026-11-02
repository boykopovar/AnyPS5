#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

std::uint64_t APS5_VABI WKFrameHandleGetFrameID(const void* frameHandleRef) {
    return NKWebKit::Cast<NKWebKit::FrameHandle>(frameHandleRef, __func__)->frameID;
}

}
