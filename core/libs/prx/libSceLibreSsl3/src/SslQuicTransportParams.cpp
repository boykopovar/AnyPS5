#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstring>
#include <stdexcept>

extern "C" int APS5_VABI SSL_set_quic_transport_params_nid_postfix(LibreSsl3Ssl* ssl, const std::uint8_t* params, std::size_t paramsLength) {
    if (!ssl) throw std::invalid_argument("SSL_set_quic_transport_params requires an SSL");
    if (!params && paramsLength != 0) throw std::invalid_argument("SSL_set_quic_transport_params requires parameters");
    if (ssl->quicTransportParams) {
        std::memset(ssl->quicTransportParams, 0, ssl->quicTransportParamsLength);
        free_nid_postfix(ssl->quicTransportParams);
    }
    ssl->quicTransportParams = nullptr;
    ssl->quicTransportParamsLength = 0;
    auto* copy = static_cast<std::uint8_t*>(malloc_nid_postfix(paramsLength ? paramsLength : 1));
    if (!copy) return 0;
    if (paramsLength != 0) std::memcpy(copy, params, paramsLength);
    ssl->quicTransportParams = copy;
    ssl->quicTransportParamsLength = paramsLength;
    return 1;
}
