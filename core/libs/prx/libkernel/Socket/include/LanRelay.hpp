#ifndef CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SOCKET_INCLUDE_LANRELAY_HPP

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct sockaddr;

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

constexpr std::uint8_t FrameVersion = 1;
constexpr std::size_t FrameHeaderSize = 11;
constexpr std::size_t MaxDatagramSize = 65507;
constexpr std::size_t MaxFramePayload = MaxDatagramSize - FrameHeaderSize;

struct Frame {
    std::uint32_t Instance = 0;
    std::uint16_t SourcePort = 0;
    std::uint16_t DestinationPort = 0;
    std::vector<std::uint8_t> Payload;
};

std::vector<std::uint8_t> EncodeFrame(const Frame& frame);
std::optional<Frame> DecodeFrame(const std::vector<std::uint8_t>& datagram);

struct Ipv4Interface {
    std::uint32_t Address = 0;
    std::uint32_t Netmask = 0;
};

bool IsBroadcast(std::uint32_t destination, const std::vector<Ipv4Interface>& interfaces);
bool IsMulticast(std::uint32_t destination);
bool IsOwnFrame(const Frame& frame, std::uint32_t instance);

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
    std::uint32_t Instance() const;
    const std::vector<Endpoint>& Peers() const;
    void SetPeers(std::vector<Endpoint> value);

    bool SendDatagram(const Endpoint& peer, const std::vector<std::uint8_t>& datagram);
    bool SendFrame(const Endpoint& peer, std::uint16_t sourcePort, std::uint16_t destinationPort, const std::vector<std::uint8_t>& payload);
    std::optional<Frame> Receive(int timeoutMilliseconds);
    bool SendToPeers(std::uint16_t sourcePort, std::uint16_t destinationPort, const std::vector<std::uint8_t>& payload);
    bool StartDelivery();
    void StopDelivery();

private:
    std::uintptr_t handle = ~std::uintptr_t(0);
    std::uint16_t port = 0;
    std::uint32_t instance = 0;
    std::vector<Endpoint> peers;
    std::atomic<bool> delivering{false};
    std::thread receiver;
    std::unique_ptr<Relay> localSender;

    void Deliver();
};

std::unique_ptr<Relay> StartRelay(const char* listen, const char* peers, std::FILE* log);
bool WithActiveRelay(const std::function<void(Relay&)>& action);
bool HostIpv4AddressOfIndex(std::uint32_t index, std::uint32_t& networkAddress);
void ForwardLanSend(std::uintptr_t native, const sockaddr* destination, const void* buffer, std::size_t length);

}

#endif
