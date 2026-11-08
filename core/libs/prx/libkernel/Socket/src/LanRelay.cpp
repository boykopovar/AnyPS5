#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <stdexcept>

namespace LanRelay {

Endpoint ParseEndpoint(const std::string& text) {
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) throw std::invalid_argument("missing port");
    Endpoint endpoint;
    endpoint.Host = text.substr(0, colon);
    const std::string port = text.substr(colon + 1);
    if (port.empty()) throw std::invalid_argument("missing port");
    std::uint32_t value = 0;
    for (const char digit : port) {
        if (digit < '0' || digit > '9') throw std::invalid_argument("port is not a number");
        value = value * 10 + static_cast<std::uint32_t>(digit - '0');
    }
    endpoint.Port = static_cast<std::uint16_t>(value);
    return endpoint;
}

}
