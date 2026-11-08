#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <stdexcept>
#include <vector>

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

std::uint16_t ParsePort(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("missing port");
    std::uint32_t value = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') throw std::invalid_argument("port is not a number");
        value = value * 10 + static_cast<std::uint32_t>(digit - '0');
        if (value > 65535) throw std::invalid_argument("port out of range");
    }
    if (value == 0) throw std::invalid_argument("port out of range");
    return static_cast<std::uint16_t>(value);
}

}

Endpoint ParseEndpoint(const std::string& text) {
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) throw std::invalid_argument("missing port");
    Endpoint endpoint;
    endpoint.Host = text.substr(0, colon);
    CheckIPv4(endpoint.Host);
    endpoint.Port = ParsePort(text.substr(colon + 1));
    return endpoint;
}

std::vector<Endpoint> ParsePeers(const std::string& text) {
    std::vector<Endpoint> peers;
    if (text.empty()) return peers;
    std::size_t start = 0;
    while (true) {
        const auto comma = text.find(',', start);
        const auto entry = ParseEndpoint(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        bool duplicate = false;
        for (const auto& peer : peers) duplicate = duplicate || (peer.Host == entry.Host && peer.Port == entry.Port);
        if (!duplicate) peers.push_back(entry);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return peers;
}

Settings ReadSettings(const char* listen, const char* peers) {
    Settings settings;
    const std::string listenText = listen != nullptr ? listen : "";
    const std::string peersText = peers != nullptr ? peers : "";
    if (listenText.empty()) {
        if (!peersText.empty()) throw std::invalid_argument("ANYPS5_LAN_PEERS needs ANYPS5_LAN_LISTEN");
        return settings;
    }
    try {
        settings.Listen = ParseEndpoint(listenText);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("ANYPS5_LAN_LISTEN: ") + error.what());
    }
    try {
        settings.Peers = ParsePeers(peersText);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("ANYPS5_LAN_PEERS: ") + error.what());
    }
    return settings;
}

}
