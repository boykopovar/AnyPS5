#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "line %u: %s\n", location.line(), message);
    std::abort();
}

template<class TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using StateBytes = std::array<unsigned char, sizeof(LibreSsl3Ssl3State)>;

StateBytes Pattern() {
    StateBytes bytes;
    for (std::size_t index = 0; index < bytes.size(); ++index)
        bytes[index] = static_cast<unsigned char>(index * 13 + 5);
    return bytes;
}

template<class TValue>
TValue ReadAt(const StateBytes& bytes, std::size_t offset) {
    TValue value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void CheckVerifiedChain() {
    const StateBytes bytes = Pattern();
    LibreSsl3Ssl3State state;
    std::memcpy(&state, bytes.data(), sizeof(state));
    LibreSsl3Ssl ssl;
    std::memset(&ssl, 0x5A, sizeof(ssl));
    ssl.s3 = &state;
    std::array<unsigned char, sizeof(ssl)> sslBytes;
    std::memcpy(sslBytes.data(), &ssl, sizeof(ssl));

    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == ReadAt<LibreSsl3Stack*>(bytes, 520), "chain read from offset 520 of the SSL3 state");
    Require(std::memcmp(&state, bytes.data(), sizeof(state)) == 0, "SSL3 state unchanged");
    Require(std::memcmp(&ssl, sslBytes.data(), sizeof(ssl)) == 0, "SSL unchanged");

    char marker = 0;
    auto* chain = reinterpret_cast<LibreSsl3Stack*>(&marker);
    state.verifiedChain = chain;
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == chain, "stored chain");
    state.verifiedChain = nullptr;
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == nullptr, "no chain before verification");

    ssl.s3 = nullptr;
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == nullptr, "an SSL without SSL3 state has no chain");
    Require(ThrowsInvalidArgument([] { SSL_get0_verified_chain_nid_postfix(nullptr); }), "null SSL");
}

void CheckPeerQuicTransportParams() {
    const StateBytes bytes = Pattern();
    LibreSsl3Ssl3State state;
    std::memcpy(&state, bytes.data(), sizeof(state));
    LibreSsl3Ssl ssl;
    std::memset(&ssl, 0x5A, sizeof(ssl));
    ssl.s3 = &state;
    std::array<unsigned char, sizeof(ssl)> sslBytes;
    std::memcpy(sslBytes.data(), &ssl, sizeof(ssl));

    const std::uint8_t sentinel = 0;
    const std::uint8_t* params = &sentinel;
    std::size_t length = 77;
    SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &params, &length);
    Require(params == ReadAt<const std::uint8_t*>(bytes, 1000), "parameters read from offset 1000 of the SSL3 state");
    Require(length == ReadAt<std::uint64_t>(bytes, 1008), "length read from offset 1008 of the SSL3 state");
    Require(std::memcmp(&state, bytes.data(), sizeof(state)) == 0, "SSL3 state unchanged");
    Require(std::memcmp(&ssl, sslBytes.data(), sizeof(ssl)) == 0, "SSL unchanged");

    std::uint8_t received[] = {0x01, 0x02, 0x44, 0x00};
    state.peerQuicTransportParams = received;
    state.peerQuicTransportParamsLength = sizeof(received);
    params = &sentinel;
    length = 77;
    SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &params, &length);
    Require(params == received && length == sizeof(received), "borrowed peer parameters");
    Require(received[0] == 0x01 && received[1] == 0x02 && received[2] == 0x44 && received[3] == 0x00, "parameters not copied or changed");

    state.peerQuicTransportParams = nullptr;
    state.peerQuicTransportParamsLength = 0;
    params = &sentinel;
    length = 77;
    SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &params, &length);
    Require(params == nullptr && length == 0, "no parameters received");

    params = &sentinel;
    length = 77;
    Require(ThrowsInvalidArgument([&] { SSL_get_peer_quic_transport_params_nid_postfix(&ssl, nullptr, &length); }), "null parameter output");
    Require(length == 77, "length untouched without a parameter output");
    Require(ThrowsInvalidArgument([&] { SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &params, nullptr); }), "null length output");
    Require(params == &sentinel, "parameters untouched without a length output");
    Require(ThrowsInvalidArgument([&] { SSL_get_peer_quic_transport_params_nid_postfix(nullptr, &params, &length); }), "null SSL");
    ssl.s3 = nullptr;
    Require(ThrowsInvalidArgument([&] { SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &params, &length); }), "SSL without SSL3 state");
    Require(params == &sentinel && length == 77, "outputs untouched after a rejected call");
}

}

int main() {
    CheckVerifiedChain();
    CheckPeerQuicTransportParams();
    return 0;
}
