#ifndef CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace LanRelay {

struct Endpoint {
    std::string Host;
    std::uint16_t Port = 0;
};

Endpoint ParseEndpoint(const std::string& text);
std::vector<Endpoint> ParsePeers(const std::string& text);

}

#endif
