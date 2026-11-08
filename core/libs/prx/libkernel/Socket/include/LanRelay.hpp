#ifndef CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace LanRelay {

struct Endpoint {
    std::string Host;
    std::uint16_t Port = 0;
};

Endpoint ParseEndpoint(const std::string& text);
std::vector<Endpoint> ParsePeers(const std::string& text);

struct Settings {
    std::optional<Endpoint> Listen;
    std::vector<Endpoint> Peers;
};

Settings ReadSettings(const char* listen, const char* peers);

class Relay {
public:
    Relay() = default;
    Relay(const Relay&) = delete;
    Relay& operator=(const Relay&) = delete;
    ~Relay();

    void Open(const Endpoint& listen);
    void Close();
    bool IsOpen() const;
    std::uint16_t Port() const;

private:
    std::uintptr_t handle = ~std::uintptr_t(0);
    std::uint16_t port = 0;
};

std::unique_ptr<Relay> StartRelay(const char* listen, const char* peers, std::FILE* log);

}

#endif
