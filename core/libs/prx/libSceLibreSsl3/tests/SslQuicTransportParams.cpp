#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "SSL_set_quic_transport_params check failed at line %d\n", line);
    std::abort();
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    LibreSsl3Ssl ssl{};
    const std::array<std::uint8_t, 3> first{0x01, 0x02, 0x03};
    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, first.data(), first.size()) == 1);
    Require(ssl.quicTransportParams != nullptr && ssl.quicTransportParamsLength == 3);
    Require(std::memcmp(ssl.quicTransportParams, first.data(), first.size()) == 0);

    const std::array<std::uint8_t, 1> second{0xff};
    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, second.data(), second.size()) == 1);
    Require(ssl.quicTransportParamsLength == 1 && ssl.quicTransportParams[0] == 0xff);

    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, nullptr, 0) == 1);
    Require(ssl.quicTransportParams != nullptr && ssl.quicTransportParamsLength == 0);
    free_nid_postfix(ssl.quicTransportParams);

    bool threw = false;
    try { SSL_set_quic_transport_params_nid_postfix(nullptr, first.data(), first.size()); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { SSL_set_quic_transport_params_nid_postfix(&ssl, nullptr, 1); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    return 0;
}
