#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <stdexcept>

namespace LanRelay {
namespace {

void CheckIPv4(const std::string& host) {
    if (host.empty()) throw std::invalid_argument("missing host");
    int parts = 0;
    std::uint32_t value = 0;
    bool digits = false;
    for (const char symbol : host) {
        if (symbol >= '0' && symbol <= '9') {
            value = value * 10 + static_cast<std::uint32_t>(symbol - '0');
            if (value > 255) throw std::invalid_argument("host octet out of range");
            digits = true;
        } else if (symbol == '.' && digits) {
            ++parts;
            value = 0;
            digits = false;
        } else {
            throw std::invalid_argument("host is not an IPv4 address");
        }
    }
    if (!digits || parts != 3) throw std::invalid_argument("host is not an IPv4 address");
}

}

Endpoint ParseEndpoint(const std::string& text) {
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) throw std::invalid_argument("missing port");
    Endpoint endpoint;
    endpoint.Host = text.substr(0, colon);
    CheckIPv4(endpoint.Host);
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
