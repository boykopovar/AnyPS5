#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"

static void Require(bool condition) {
    if (!condition) std::abort();
}

static bool Failed(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static void ClearError() {
    *__error_nid_postfix() = 0;
}

static void CheckForwardedDatagram() {
    const int receiver = sceNetSocket("forward-receiver", 2, 2, 17);
    const int sender = sceNetSocket("forward-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, local.data(), local.size()) == 0);
    Require(bind_nid_postfix(sender, local.data(), local.size()) == 0);
    std::array<unsigned char, 16> receiverAddress{};
    std::array<unsigned char, 16> senderAddress{};
    std::uint32_t size = receiverAddress.size();
    Require(getsockname_nid_postfix(receiver, receiverAddress.data(), &size) == 0 && size == receiverAddress.size());
    Require((receiverAddress[2] | receiverAddress[3]) != 0);
    size = senderAddress.size();
    Require(getsockname_nid_postfix(sender, senderAddress.data(), &size) == 0 && size == senderAddress.size());
    const char payload[] = "ping";
    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, receiverAddress.data(), receiverAddress.size()) == sizeof(payload));
    char received[8]{};
    std::array<unsigned char, 16> from{};
    size = from.size();
    Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 0, from.data(), &size) == sizeof(payload));
    Require(std::strcmp(received, payload) == 0 && size == from.size());
    Require(from[2] == senderAddress[2] && from[3] == senderAddress[3]);
    Require(sceNetSocketClose(receiver) == 0 && sceNetSocketClose(sender) == 0);
}

static void CheckForwardedErrors() {
    const int socket = sceNetSocket("forward-errors", 2, 2, 17);
    Require(socket >= 0);
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    ClearError();
    Require(Failed(bind_nid_postfix(socket, local.data(), 1), 22));
    Require(bind_nid_postfix(socket, local.data(), local.size()) == 0);
    char received[8]{};
    ClearError();
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0x80, nullptr, nullptr), 35));
    ClearError();
    Require(Failed(getsockname_nid_postfix(socket, nullptr, nullptr), 22));
    ClearError();
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, local.data(), 1), 22));
    Require(sceNetSocketClose(socket) == 0);
}

static void CheckUnknownDescriptor() {
    constexpr int unknown = 0x7ffffff0;
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    std::uint32_t size = local.size();
    char byte = 0;
    ClearError();
    Require(Failed(bind_nid_postfix(unknown, local.data(), local.size()), 9));
    ClearError();
    Require(Failed(getsockname_nid_postfix(unknown, local.data(), &size), 9));
    ClearError();
    Require(Failed(sendto_nid_postfix(unknown, &byte, 1, 0, local.data(), local.size()), 9));
    ClearError();
    Require(Failed(recvfrom_nid_postfix(unknown, &byte, 1, 0, nullptr, nullptr), 9));
}

int main() {
    CheckForwardedDatagram();
    CheckForwardedErrors();
    CheckUnknownDescriptor();
    return 0;
}
