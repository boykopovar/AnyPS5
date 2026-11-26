#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

}

extern "C" {

std::int64_t APS5_VABI SSL_SESSION_get_timeout_nid_postfix(const void* session) {
    if (!session) return 0;
    std::int64_t timeout;
    if (!GuestCopyIn_nid_no_patch(&timeout, static_cast<const unsigned char*>(session) + 168, sizeof(timeout))) {
        NotImplemented_nid_no_patch(__func__);
    }
    return timeout;
}

std::int64_t APS5_VABI SSL_SESSION_set_time_nid_postfix(void* session, std::int64_t time) {
    if (!session) return 0;
    if (!GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(session) + 176, &time, sizeof(time))) {
        NotImplemented_nid_no_patch(__func__);
    }
    return time;
}

int APS5_VABI SSL_SESSION_get_protocol_version_nid_postfix(const void* session) {
    if (!session) throw std::invalid_argument("SSL_SESSION_get_protocol_version: null session");
    std::int32_t version;
    if (!GuestCopyIn_nid_no_patch(&version, session, sizeof(version))) {
        NotImplemented_nid_no_patch(__func__);
    }
    return version;
}

std::uint64_t APS5_VABI SSL_SESSION_get_master_key_nid_postfix(const void* session, unsigned char* out, std::uint64_t outLength) {
    if (!session) return 0;
    std::uint64_t length;
    if (!GuestCopyIn_nid_no_patch(&length, static_cast<const unsigned char*>(session) + 8, sizeof(length))) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (length > 48) length = 48;
    if (out == nullptr) return length;
    const std::uint64_t copied = length > outLength ? outLength : length;
    if (copied != 0) {
        std::array<unsigned char, 48> key{};
        if (!GuestCopyIn_nid_no_patch(key.data(), static_cast<const unsigned char*>(session) + 16, copied)
            || !GuestCopyOut_nid_no_patch(out, key.data(), copied)) {
            NotImplemented_nid_no_patch(__func__);
        }
    }
    return copied;
}

}
