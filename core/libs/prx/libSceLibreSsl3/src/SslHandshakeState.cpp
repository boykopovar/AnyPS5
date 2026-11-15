#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {

LibreSsl3Stack* APS5_VABI SSL_get0_verified_chain_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl)
        throw std::invalid_argument("SSL_get0_verified_chain requires an SSL");
    if (!ssl->s3) return nullptr;
    return ssl->s3->verifiedChain;
}

void APS5_VABI SSL_get_peer_quic_transport_params_nid_postfix(const LibreSsl3Ssl* ssl, const std::uint8_t** outParams, std::size_t* outParamsLength) {
    if (!ssl || !ssl->s3)
        throw std::invalid_argument("SSL_get_peer_quic_transport_params requires an SSL with SSL3 state");
    if (!outParams || !outParamsLength)
        throw std::invalid_argument("SSL_get_peer_quic_transport_params requires both outputs");
    *outParams = ssl->s3->peerQuicTransportParams;
    *outParamsLength = ssl->s3->peerQuicTransportParamsLength;
}

}
