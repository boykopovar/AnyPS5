#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <stdexcept>

namespace {

class ErrnoGuard {
public:
    ErrnoGuard() : savedErrno(errno) {}
    ~ErrnoGuard() { errno = savedErrno; }

private:
    int savedErrno;
};

WebRtcFileWrapper CopyInWrapper(const WebRtcFileWrapper* wrapper, const char* message) {
    ErrnoGuard errnoGuard;
    WebRtcFileWrapper local{};
    if (!GuestPtr<const WebRtcFileWrapper>(wrapper).CopyIn(local))
        throw std::invalid_argument(message);
    return local;
}

bool WritableWrapper(WebRtcFileWrapper* wrapper) {
    ErrnoGuard errnoGuard;
    return wrapper != nullptr && GuestWritable_nid_no_patch(wrapper, sizeof(*wrapper));
}

bool CopyOutWrapper(WebRtcFileWrapper* wrapper, const WebRtcFileWrapper& local) {
    ErrnoGuard errnoGuard;
    return GuestPtr<WebRtcFileWrapper>(wrapper).CopyOut(local);
}

}

extern "C" {

bool APS5_VABI _ZN6webrtc11FileWrapper5CloseEv_nid_postfix(WebRtcFileWrapper* wrapper) {
    WebRtcFileWrapper local = CopyInWrapper(wrapper, "FileWrapper::Close: null wrapper");
    if (!local.file)
        return true;
    if (!WritableWrapper(wrapper))
        throw std::invalid_argument("FileWrapper::Close: unwritable wrapper");
    const bool success = fclose_nid_postfix(local.file) == 0;
    local.file = nullptr;
    if (!CopyOutWrapper(wrapper, local))
        throw std::invalid_argument("FileWrapper::Close: unwritable wrapper");
    return success;
}

bool APS5_VABI _ZN6webrtc11FileWrapper5FlushEv_nid_postfix(WebRtcFileWrapper* wrapper) {
    const WebRtcFileWrapper local = CopyInWrapper(wrapper, "FileWrapper::Flush: no open stream");
    if (!local.file)
        throw std::invalid_argument("FileWrapper::Flush: no open stream");
    return fflush_nid_postfix(local.file) == 0;
}

bool APS5_VABI _ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix(WebRtcFileWrapper* wrapper, const void* data, std::size_t length) {
    const WebRtcFileWrapper local = CopyInWrapper(wrapper, "FileWrapper::Write: no open stream");
    if (!local.file)
        throw std::invalid_argument("FileWrapper::Write: no open stream");
    if (!data && length != 0)
        throw std::invalid_argument("FileWrapper::Write: null data");
    std::array<unsigned char, 4096> chunk{};
    if (length == 0)
        return fwrite_nid_postfix(chunk.data(), 1, 0, local.file) == 0;
    std::size_t offset = 0;
    while (offset < length) {
        const std::size_t toRead = std::min(chunk.size(), length - offset);
        {
            ErrnoGuard errnoGuard;
            if (!GuestBytes(static_cast<const unsigned char*>(data) + offset).CopyIn(chunk.data(), toRead))
                throw std::invalid_argument("FileWrapper::Write: unreadable data");
        }
        if (fwrite_nid_postfix(chunk.data(), 1, toRead, local.file) != toRead)
            return false;
        offset += toRead;
    }
    return true;
}

void APS5_VABI _ZN6webrtc11FileWrapperC1EOS0__nid_postfix(WebRtcFileWrapper* wrapper, WebRtcFileWrapper* other) {
    if (!wrapper || !other || wrapper == other)
        throw std::invalid_argument("FileWrapper move: invalid object");
    WebRtcFileWrapper source = CopyInWrapper(other, "FileWrapper move: invalid object");
    if (!WritableWrapper(wrapper) || !WritableWrapper(other))
        throw std::invalid_argument("FileWrapper move: invalid object");
    const WebRtcFileWrapper destination{source.file};
    source.file = nullptr;
    if (!CopyOutWrapper(wrapper, destination) || !CopyOutWrapper(other, source))
        throw std::invalid_argument("FileWrapper move: invalid object");
}

}
