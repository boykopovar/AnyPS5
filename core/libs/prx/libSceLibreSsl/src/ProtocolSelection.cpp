#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>

namespace {

constexpr int Negotiated = 1;
constexpr int NoOverlap = 2;

const unsigned char* FirstOffered_nid_no_patch(const unsigned char* peer, std::uint32_t peerLength, const unsigned char* supported, std::uint32_t supportedLength) {
    for (std::uint32_t offered = 0; offered < peerLength; offered += 1u + peer[offered]) {
        for (std::uint32_t local = 0; local < supportedLength; local += 1u + supported[local]) {
            if (peer[offered] == supported[local] && std::memcmp(peer + offered + 1, supported + local + 1, peer[offered]) == 0)
                return peer + offered;
        }
    }
    return nullptr;
}

}

extern "C" int APS5_VABI SSL_select_next_proto_nid_postfix(unsigned char** output, unsigned char* outputLength,
    const unsigned char* peer, std::uint32_t peerLength, const unsigned char* supported, std::uint32_t supportedLength) {
    if (!output || !outputLength) throw std::invalid_argument("SSL_select_next_proto: null output");
    if (!supported || (!peer && peerLength != 0)) throw std::invalid_argument("SSL_select_next_proto: null protocol list");
    const unsigned char* result = FirstOffered_nid_no_patch(peer, peerLength, supported, supportedLength);
    const int status = result ? Negotiated : NoOverlap;
    if (!result) result = supported;
    *output = const_cast<unsigned char*>(result) + 1;
    *outputLength = result[0];
    return status;
}
