#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <stdexcept>

extern "C" void APS5_VABI SSL_set_accept_state_nid_postfix(LibreSsl3Ssl* ssl) {
    if (!ssl) throw std::invalid_argument("SSL_set_accept_state: null SSL");
    ssl->server = 1;
}
