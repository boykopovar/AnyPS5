#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string_view>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr std::int64_t Efault = static_cast<std::int64_t>(static_cast<int>(0x8041010eu));

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

struct Sockets {
    int sender = -1;
    int receiver = -1;
    std::array<unsigned char, 16> destination{16, 2, 0, 0, 127, 0, 0, 1};
};

Sockets Open() {
    Sockets sockets;
    sockets.receiver = sceNetSocket("sendmsg-faults-receiver", 2, 2, 17);
    sockets.sender = sceNetSocket("sendmsg-faults-sender", 2, 2, 17);
    Require(sockets.receiver >= 0 && sockets.sender >= 0);
    Require(sceNetBind_nid_postfix(sockets.receiver, sockets.destination.data(), sockets.destination.size()) == 0);
    std::uint32_t size = sockets.destination.size();
    Require(sceNetGetsockname(sockets.receiver, sockets.destination.data(), &size) == 0 && size == sockets.destination.size());
    return sockets;
}

void RequireFault(std::int64_t result) {
    Require(result == Efault);
    Require(*sceNetErrnoLoc() == 14);
}

void RequireNothingSent(Sockets& sockets) {
    const char marker[] = "marker";
    Require(sceNetSendto(sockets.sender, marker, sizeof(marker), 0, sockets.destination.data(), sockets.destination.size()) == sizeof(marker));
    char received[64]{};
    Require(sceNetRecv(sockets.receiver, received, sizeof(received), 0) == sizeof(marker));
    Require(std::memcmp(received, marker, sizeof(marker)) == 0);
}

void RequireSend(Sockets& sockets, const NetMsghdr* message) {
    Require(sceNetSendmsg(sockets.sender, message, 0) == 8);
    unsigned char received[64]{};
    Require(sceNetRecv(sockets.receiver, received, sizeof(received), 0) == 8);
    for (std::size_t i = 0; i < 8; ++i) Require(received[i] == 0x5a);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    Require(mode == "payload" || mode == "iovec" || mode == "header" || mode == "name");
    Require(sceNetInit_nid_postfix() == 0);
    auto sockets = Open();

    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page * 3, 0, 0, &physical) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, Page * 3, 3, 0, physical, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapping);
    std::memset(bytes, 0x5a, Page * 3);
    auto* first = bytes;
    auto* second = bytes + Page;
    auto* third = bytes + Page * 2;

    auto* message = reinterpret_cast<NetMsghdr*>(first + 0x100);
    auto* vector = reinterpret_cast<NetIovec*>(first + 0x200);
    *vector = NetIovec{third, 8};
    *message = NetMsghdr{sockets.destination.data(), sockets.destination.size(), vector, 1, nullptr, 0, 0};
    RequireSend(sockets, message);

    *sceNetErrnoLoc() = 0;
    if (mode == "payload") {
        *vector = NetIovec{second + Page - 8, 16};
        Require(sceKernelMprotect(third, Page, 0) == 0);
        RequireFault(sceNetSendmsg(sockets.sender, message, 0));
        *sceNetErrnoLoc() = 0;
        RequireFault(sceNetSendto(sockets.sender, second + Page - 8, 16, 0, sockets.destination.data(), sockets.destination.size()));
        Require(sceKernelMprotect(third, Page, 3) == 0);
        *vector = NetIovec{third, 8};
    } else if (mode == "iovec") {
        auto* crossing = reinterpret_cast<NetIovec*>(second - sizeof(void*));
        const unsigned char* payload = third;
        std::memcpy(crossing, &payload, sizeof(payload));
        const std::uint64_t length = 8;
        std::memcpy(second, &length, sizeof(length));
        message->iov = crossing;
        Require(sceKernelMprotect(second, Page, 0) == 0);
        RequireFault(sceNetSendmsg(sockets.sender, message, 0));
        Require(sceKernelMprotect(second, Page, 3) == 0);
        message->iov = vector;
    } else if (mode == "header") {
        auto* crossing = reinterpret_cast<NetMsghdr*>(second - sizeof(void*));
        std::memcpy(crossing, message, sizeof(*message));
        Require(sceKernelMprotect(second, Page, 0) == 0);
        RequireFault(sceNetSendmsg(sockets.sender, crossing, 0));
        Require(sceKernelMprotect(second, Page, 3) == 0);
    } else {
        *vector = NetIovec{first + 0x400, 8};
        auto* name = third;
        std::memcpy(name, sockets.destination.data(), sockets.destination.size());
        message->name = name;
        Require(sceKernelMprotect(third, Page, 0) == 0);
        RequireFault(sceNetSendmsg(sockets.sender, message, 0));
        *sceNetErrnoLoc() = 0;
        const char data[8]{};
        RequireFault(sceNetSendto(sockets.sender, data, sizeof(data), 0, name, sockets.destination.size()));
        Require(sceKernelMprotect(third, Page, 3) == 0);
        message->name = sockets.destination.data();
    }
    RequireNothingSent(sockets);
    RequireSend(sockets, message);

    Require(sceNetSocketClose(sockets.sender) == 0);
    Require(sceNetSocketClose(sockets.receiver) == 0);
    std::printf("guest net sendmsg %s fault test passed\n", argv[1]);
    return 0;
}
