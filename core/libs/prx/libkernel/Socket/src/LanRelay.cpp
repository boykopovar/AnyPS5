#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <cerrno>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#endif

namespace LanRelay {
namespace {

constexpr std::uintptr_t NoSocket = ~std::uintptr_t(0);

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

int LastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void EnsureNetworking() {
#ifdef _WIN32
    static const int startup = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (startup != 0) throw std::runtime_error("LAN relay: WSAStartup failed, error " + std::to_string(startup));
#endif
}

void CloseNative(SocketHandle socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
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

Relay::~Relay() {
    Close();
}

void Relay::Open(const Endpoint& listen) {
    if (IsOpen()) throw std::logic_error("LAN relay: already open");
    EnsureNetworking();
    CheckIPv4(listen.Host);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(listen.Port);
    if (inet_pton(AF_INET, listen.Host.c_str(), &address.sin_addr) != 1) {
        throw std::invalid_argument("LAN relay: host is not an IPv4 address");
    }
    const SocketHandle descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor == static_cast<SocketHandle>(-1)) {
        throw std::runtime_error("LAN relay: socket failed, error " + std::to_string(LastError()));
    }
    if (bind(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
        const int error = LastError();
        CloseNative(descriptor);
        throw std::runtime_error("LAN relay: bind failed, error " + std::to_string(error));
    }
    sockaddr_in bound{};
    socklen_t length = sizeof bound;
    if (getsockname(descriptor, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        const int error = LastError();
        CloseNative(descriptor);
        throw std::runtime_error("LAN relay: getsockname failed, error " + std::to_string(error));
    }
    handle = static_cast<std::uintptr_t>(descriptor);
    port = ntohs(bound.sin_port);
}

void Relay::Close() {
    if (!IsOpen()) return;
    CloseNative(static_cast<SocketHandle>(handle));
    handle = NoSocket;
    port = 0;
}

bool Relay::IsOpen() const {
    return handle != NoSocket;
}

std::uint16_t Relay::Port() const {
    return port;
}

std::unique_ptr<Relay> StartRelay(const char* listen, const char* peers, std::FILE* log) {
    Settings settings;
    try {
        settings = ReadSettings(listen, peers);
    } catch (const std::invalid_argument& error) {
        if (log != nullptr) std::fprintf(log, "%s, LAN relay is off\n", error.what());
        return nullptr;
    }
    if (!settings.Listen) return nullptr;
    auto relay = std::make_unique<Relay>();
    try {
        relay->Open(*settings.Listen);
    } catch (const std::exception& error) {
        if (log != nullptr) std::fprintf(log, "%s, LAN relay is off\n", error.what());
        return nullptr;
    }
    return relay;
}

}
