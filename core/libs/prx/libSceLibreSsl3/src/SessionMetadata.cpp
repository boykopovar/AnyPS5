#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" {

std::int64_t APS5_VABI SSL_SESSION_get_timeout_nid_postfix(const void* session) {
    if (!session) return 0;
    std::int64_t timeout;
    std::memcpy(&timeout, static_cast<const unsigned char*>(session) + 168, sizeof(timeout));
    return timeout;
}

std::int64_t APS5_VABI SSL_SESSION_set_time_nid_postfix(void* session, std::int64_t time) {
    if (!session) return 0;
    std::memcpy(static_cast<unsigned char*>(session) + 176, &time, sizeof(time));
    return time;
}

int APS5_VABI SSL_SESSION_get_protocol_version_nid_postfix(const void* session) {
    if (!session) throw std::invalid_argument("SSL_SESSION_get_protocol_version: null session");
    std::int32_t version;
    std::memcpy(&version, session, sizeof(version));
    return version;
}

std::uint64_t APS5_VABI SSL_SESSION_get_master_key_nid_postfix(const void* session, unsigned char* out, std::uint64_t outLength) {
    if (!session) return 0;
    std::uint64_t length;
    std::memcpy(&length, static_cast<const unsigned char*>(session) + 8, sizeof(length));
    if (length > 48) length = 48;
    if (out == nullptr) return length;
    const std::uint64_t copied = length > outLength ? outLength : length;
    if (copied != 0) std::memcpy(out, static_cast<const unsigned char*>(session) + 16, copied);
    return copied;
}

}
