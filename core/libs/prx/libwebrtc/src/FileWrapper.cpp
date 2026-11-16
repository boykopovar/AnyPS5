#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include <stdexcept>
#include <utility>

extern "C" {

bool APS5_VABI _ZN6webrtc11FileWrapper5CloseEv_nid_postfix(WebRtcFileWrapper* wrapper) {
    if (!wrapper) throw std::invalid_argument("FileWrapper::Close: null wrapper");
    if (!wrapper->file) return true;
    const bool success = fclose_nid_postfix(wrapper->file) == 0;
    wrapper->file = nullptr;
    return success;
}

bool APS5_VABI _ZN6webrtc11FileWrapper5FlushEv_nid_postfix(WebRtcFileWrapper* wrapper) {
    if (!wrapper || !wrapper->file) throw std::invalid_argument("FileWrapper::Flush: no open stream");
    return fflush_nid_postfix(wrapper->file) == 0;
}

bool APS5_VABI _ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix(WebRtcFileWrapper* wrapper, const void* data, std::size_t length) {
    if (!wrapper || !wrapper->file) throw std::invalid_argument("FileWrapper::Write: no open stream");
    if (!data && length != 0) throw std::invalid_argument("FileWrapper::Write: null data");
    return fwrite_nid_postfix(data, 1, length, wrapper->file) == length;
}

void APS5_VABI _ZN6webrtc11FileWrapperC1EOS0__nid_postfix(WebRtcFileWrapper* wrapper, WebRtcFileWrapper* other) {
    if (!wrapper || !other || wrapper == other) throw std::invalid_argument("FileWrapper move: invalid object");
    wrapper->file = std::exchange(other->file, nullptr);
}

}
