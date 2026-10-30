#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

bool ValidProtocols(const unsigned char* protocols, std::size_t size) {
    if (!protocols || size == 0) return false;
    for (std::size_t offset = 0; offset < size;) {
        const std::size_t length = protocols[offset++];
        if (length == 0 || length > size - offset) return false;
        offset += length;
    }
    return true;
}

}

extern "C" int APS5_VABI SSL_select_next_proto_nid_postfix(unsigned char** output, unsigned char* outputLength,
    const unsigned char* peer, std::uint32_t peerLength, const unsigned char* supported, std::uint32_t supportedLength) {
    if (!output || !outputLength || !ValidProtocols(peer, peerLength) || !ValidProtocols(supported, supportedLength))
        NotImplemented_nid_no_patch("SSL_select_next_proto: null output or invalid protocol list");
    for (std::size_t offered = 0; offered < peerLength; offered += 1 + peer[offered]) {
        for (std::size_t local = 0; local < supportedLength; local += 1 + supported[local]) {
            if (peer[offered] != supported[local] || std::memcmp(peer + offered + 1, supported + local + 1, peer[offered]) != 0) continue;
            *output = const_cast<unsigned char*>(peer + offered + 1);
            *outputLength = peer[offered];
            return 1;
        }
    }
    *output = const_cast<unsigned char*>(supported + 1);
    *outputLength = supported[0];
    return 2;
}
