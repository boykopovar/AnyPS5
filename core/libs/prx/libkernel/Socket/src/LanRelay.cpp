#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <utility>
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
#include <poll.h>
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

void WriteU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    WriteU16(out, static_cast<std::uint16_t>(value >> 16));
    WriteU16(out, static_cast<std::uint16_t>(value));
}

std::uint16_t ReadU16(const std::vector<std::uint8_t>& in, std::size_t offset) {
    return static_cast<std::uint16_t>((in[offset] << 8) | in[offset + 1]);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& in, std::size_t offset) {
    return (static_cast<std::uint32_t>(ReadU16(in, offset)) << 16) | ReadU16(in, offset + 2);
}

std::uint32_t ProcessId() {
#ifdef _WIN32
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

std::uint32_t NewInstanceId() {
    std::random_device entropy;
    std::uint64_t seed = (static_cast<std::uint64_t>(entropy()) << 32) | entropy();
    seed ^= static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    seed ^= static_cast<std::uint64_t>(ProcessId()) << 32;
    seed = (seed ^ (seed >> 30)) * 0xbf58476d1ce4e5b9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94d049bb133111ebULL;
    seed ^= seed >> 31;
    return static_cast<std::uint32_t>(seed);
}

bool WaitReadable(std::uintptr_t handle, int timeoutMilliseconds) {
#ifdef _WIN32
    WSAPOLLFD entry{};
    entry.fd = static_cast<SocketHandle>(handle);
    entry.events = POLLRDNORM;
    return WSAPoll(&entry, 1, timeoutMilliseconds) > 0 && (entry.revents & POLLRDNORM) != 0;
#else
    pollfd entry{};
    entry.fd = static_cast<SocketHandle>(handle);
    entry.events = POLLIN;
    return poll(&entry, 1, timeoutMilliseconds) > 0 && (entry.revents & POLLIN) != 0;
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

std::vector<std::uint8_t> EncodeFrame(const Frame& frame) {
    if (frame.DestinationPort == 0) throw std::invalid_argument("LAN relay: destination port is 0");
    if (frame.Payload.size() > MaxFramePayload) throw std::length_error("LAN relay: payload is too large");
    std::vector<std::uint8_t> datagram;
    datagram.reserve(FrameHeaderSize + frame.Payload.size());
    datagram.push_back(FrameVersion);
    WriteU32(datagram, frame.Instance);
    WriteU16(datagram, frame.SourcePort);
    WriteU16(datagram, frame.DestinationPort);
    WriteU16(datagram, static_cast<std::uint16_t>(frame.Payload.size()));
    datagram.insert(datagram.end(), frame.Payload.begin(), frame.Payload.end());
    return datagram;
}

std::optional<Frame> DecodeFrame(const std::vector<std::uint8_t>& datagram) {
    if (datagram.size() < FrameHeaderSize || datagram[0] != FrameVersion) return std::nullopt;
    Frame frame;
    frame.Instance = ReadU32(datagram, 1);
    frame.SourcePort = ReadU16(datagram, 5);
    frame.DestinationPort = ReadU16(datagram, 7);
    const std::size_t length = ReadU16(datagram, 9);
    if (frame.DestinationPort == 0 || datagram.size() != FrameHeaderSize + length) return std::nullopt;
    frame.Payload.assign(datagram.begin() + FrameHeaderSize, datagram.end());
    return frame;
}

bool IsOwnFrame(const Frame& frame, std::uint32_t instance) {
    return frame.Instance == instance;
}

bool IsBroadcast(std::uint32_t destination, const std::vector<Ipv4Interface>& interfaces) {
    if (destination == 0xFFFFFFFFu) return true;
    for (const auto& entry : interfaces) {
        if (entry.Netmask == 0xFFFFFFFFu) continue;
        if ((entry.Address | ~entry.Netmask) == destination) return true;
    }
    return false;
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
    instance = NewInstanceId();
}

void Relay::Close() {
    if (!IsOpen()) return;
    CloseNative(static_cast<SocketHandle>(handle));
    handle = NoSocket;
    port = 0;
    instance = 0;
}

bool Relay::IsOpen() const {
    return handle != NoSocket;
}

std::uint16_t Relay::Port() const {
    return port;
}

std::uint32_t Relay::Instance() const {
    return instance;
}

const std::vector<Endpoint>& Relay::Peers() const {
    return peers;
}

void Relay::SetPeers(std::vector<Endpoint> value) {
    peers = std::move(value);
}

bool Relay::SendToPeers(std::uint16_t sourcePort, std::uint16_t destinationPort, const std::vector<std::uint8_t>& payload) {
    bool sent = true;
    for (const auto& peer : peers) sent = SendFrame(peer, sourcePort, destinationPort, payload) && sent;
    return sent;
}

bool Relay::SendDatagram(const Endpoint& peer, const std::vector<std::uint8_t>& datagram) {
    if (!IsOpen()) return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(peer.Port);
    if (inet_pton(AF_INET, peer.Host.c_str(), &address.sin_addr) != 1) return false;
    const auto sent = sendto(static_cast<SocketHandle>(handle), reinterpret_cast<const char*>(datagram.data()), static_cast<int>(datagram.size()), 0,
        reinterpret_cast<sockaddr*>(&address), sizeof address);
    return sent >= 0 && static_cast<std::size_t>(sent) == datagram.size();
}

bool Relay::SendFrame(const Endpoint& peer, std::uint16_t sourcePort, std::uint16_t destinationPort, const std::vector<std::uint8_t>& payload) {
    Frame frame;
    frame.Instance = instance;
    frame.SourcePort = sourcePort;
    frame.DestinationPort = destinationPort;
    frame.Payload = payload;
    return SendDatagram(peer, EncodeFrame(frame));
}

std::optional<Frame> Relay::Receive(int timeoutMilliseconds) {
    if (!IsOpen()) return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
    std::vector<std::uint8_t> buffer(MaxDatagramSize);
    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (!WaitReadable(handle, remaining > 0 ? static_cast<int>(remaining) : 0)) return std::nullopt;
        const auto count = recvfrom(static_cast<SocketHandle>(handle), reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, nullptr, nullptr);
        if (count < 0) return std::nullopt;
        const std::vector<std::uint8_t> datagram(buffer.begin(), buffer.begin() + count);
        auto frame = DecodeFrame(datagram);
        if (frame && !IsOwnFrame(*frame, instance)) return frame;
        if (std::chrono::steady_clock::now() >= deadline) return std::nullopt;
    }
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
    relay->SetPeers(settings.Peers);
    return relay;
}

}
