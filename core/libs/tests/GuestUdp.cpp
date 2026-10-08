#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int APS5_VABI getsockopt_nid_postfix(int, int, int, void*, std::uint32_t*);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI ioctl_nid_postfix(int, std::uint64_t, void*);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
const char* APS5_VABI inet_ntop_nid_postfix(int, const void*, char*, std::uint32_t);
int APS5_VABI inet_pton_nid_postfix(int, const char*, void*);
int* APS5_VABI __error_nid_postfix();
}
struct Iovec {
    void* base;
    std::uint64_t length;
};
struct Msghdr {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};
extern "C" {
std::int64_t APS5_VABI sendmsg_nid_postfix(int, const Msghdr*, int);
std::int64_t APS5_VABI recvmsg_nid_postfix(int, Msghdr*, int);
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 17);
    Require(receiver >= 0 && sender >= 0 && receiver != sender);
    std::array<unsigned char, 16> destination{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, destination.data(), destination.size()) == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(receiver, destination.data(), &size) == 0 && size == 16);
    Require(destination[2] || destination[3]);
    int enabled = 1;
    Require(setsockopt_nid_postfix(sender, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    int option = 0;
    std::uint32_t optionSize = sizeof(option);
    Require(getsockopt_nid_postfix(sender, 0xffff, 0x20, &option, &optionSize) == 0 && option != 0);
    Require(setsockopt_nid_postfix(sender, 0xffff, 12345, &enabled, sizeof(enabled)) == -1);
    Require(*__error_nid_postfix() == 42);
    const int unsized = socket_nid_postfix(2, 2, 0);
    std::array<unsigned char, 16> unsizedAddress{0, 2, 0, 0, 127, 0, 0, 1};
    Require(unsized >= 0 && bind_nid_postfix(unsized, unsizedAddress.data(), unsizedAddress.size()) == 0);
    std::uint32_t unsizedLength = unsizedAddress.size();
    Require(getsockname_nid_postfix(unsized, unsizedAddress.data(), &unsizedLength) == 0 && unsizedAddress[0] == 16);
    Require(close_nid_postfix(unsized) == 0);
    Require(ioctl_nid_postfix(receiver, 0x8004667e, &enabled) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    Require(fcntl_nid_postfix(receiver, 4, 2) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 2);
    Require(fcntl_nid_postfix(receiver, 4, 6) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    Require(fcntl_nid_postfix(receiver, 4, 0x8000) == -1);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    char buffer[64]{};
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 35);
    const char message[] = "guest UDP loopback";
    Require(sendto_nid_postfix(sender, message, sizeof(message), 0x20000, destination.data(), destination.size()) == sizeof(message));
    int queued = 0;
    for (int i = 0; i < 100 && queued == 0; ++i) {
        Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0);
        if (!queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(queued == sizeof(message));
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 2, nullptr, nullptr) == sizeof(message));
    std::array<unsigned char, 16> source{};
    size = source.size();
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, source.data(), &size) == sizeof(message));
    Require(std::strcmp(buffer, message) == 0 && source[1] == 2 && source[4] == 127);
    Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0 && queued == 0);
    Require(ioctl_nid_postfix(receiver, 123, &queued) == -1 && *__error_nid_postfix() == 25);
    char head[] = "scatter ";
    char tail[] = "gather";
    Iovec parts[2]{{head, 8}, {tail, 6}};
    Msghdr outgoing{destination.data(), static_cast<std::uint32_t>(destination.size()), parts, 2, nullptr, 0, 0};
    Require(sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    const auto wait = [&] {
        queued = 0;
        for (int i = 0; i < 100 && queued == 0; ++i) {
            Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0);
            if (!queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(queued != 0);
    };
    wait();
    char first[4]{}, second[16]{};
    Iovec pieces[2]{{first, sizeof(first)}, {second, sizeof(second)}};
    source.fill(0);
    unsigned char control[8]{};
    Msghdr incoming{source.data(), static_cast<std::uint32_t>(source.size()), pieces, 2, control, sizeof(control), -1};
    Require(recvmsg_nid_postfix(receiver, &incoming, 0) == 14);
    Require(std::memcmp(first, "scat", 4) == 0 && std::memcmp(second, "ter gather", 10) == 0);
    Require(incoming.flags == 0 && incoming.controlLength == 0 && incoming.nameLength == 16 && source[1] == 2 && source[4] == 127);
    Require(sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    wait();
    char shortBuffer[5]{};
    Iovec single{shortBuffer, sizeof(shortBuffer)};
    Msghdr truncated{nullptr, 0, &single, 1, nullptr, 0, 0};
    Require(recvmsg_nid_postfix(receiver, &truncated, 0) == 5);
    Require(std::memcmp(shortBuffer, "scatt", 5) == 0 && truncated.flags == 0x10);
    Require(recvmsg_nid_postfix(receiver, &truncated, 0) == -1 && *__error_nid_postfix() == 35);
    Msghdr oversized{nullptr, 0, parts, 1025, nullptr, 0, 0};
    Require(sendmsg_nid_postfix(sender, &oversized, 0) == -1 && *__error_nid_postfix() == 40);
    Require(recvmsg_nid_postfix(receiver, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(close_nid_postfix(receiver) == 0);
    Require(close_nid_postfix(receiver) == -1 && *__error_nid_postfix() == 9);
    Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == -1);
    Require(close_nid_postfix(sender) == 0);
    unsigned char parsed[16]{};
    Require(inet_pton_nid_postfix(2, "127.0.0.1", parsed) == 1);
    Require(parsed[0] == 127 && parsed[1] == 0 && parsed[2] == 0 && parsed[3] == 1);
    char text[64]{};
    Require(inet_ntop_nid_postfix(2, parsed, text, sizeof(text)) == text);
    Require(std::strcmp(text, "127.0.0.1") == 0);
    Require(inet_pton_nid_postfix(28, "::1", parsed) == 1);
    Require(inet_ntop_nid_postfix(28, parsed, text, sizeof(text)) == text);
    Require(std::strcmp(text, "::1") == 0);
    Require(inet_pton_nid_postfix(99, "127.0.0.1", parsed) == -1 && *__error_nid_postfix() == 47);
    Require(inet_pton_nid_postfix(2, "not-an-address", parsed) == 0);
    Require(inet_ntop_nid_postfix(2, parsed, text, 4) == nullptr && *__error_nid_postfix() == 28);
    const int stream = socket_nid_postfix(2, 1, 0);
    Require(stream >= 0);
    Require(close_nid_postfix(stream) == 0);
}
