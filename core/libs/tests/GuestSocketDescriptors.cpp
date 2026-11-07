#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct GuestDescriptorSet {
    std::array<std::uint64_t, 1024 / 64> words{};

    void Add(int descriptor) {
        words[descriptor / 64] |= (std::uint64_t{1} << (descriptor % 64));
    }

    bool Has(int descriptor) const {
        return ((words[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
    }
};

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "socket descriptor check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int DescriptorSetSize = 1024;
constexpr int DuplicateCommand = 0;
constexpr int DuplicateCloseOnExec = 17;
constexpr int Inet = 2;
constexpr int Datagram = 2;
constexpr short In = 0x1;
constexpr int KernelBadDescriptor = static_cast<int>(0x80020009u);

template <typename TCall> static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static int Socket() {
    const int descriptor = socket_nid_postfix(Inet, Datagram, 0);
    Require(descriptor >= 0 && descriptor < DescriptorSetSize);
    return descriptor;
}

static int BoundSocket(std::array<unsigned char, 16>& address) {
    const int descriptor = Socket();
    address = {16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(descriptor, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(descriptor, address.data(), &size) == 0);
    return descriptor;
}

static int ConnectedSocket(const std::array<unsigned char, 16>& address) {
    const int descriptor = Socket();
    Require(connect_nid_postfix(descriptor, address.data(), address.size()) == 0);
    return descriptor;
}

static void WaitReadable(int descriptor) {
    GuestDescriptorSet read{};
    read.Add(descriptor);
    GuestTimeval limit{2, 0};
    Require(select_nid_postfix(descriptor + 1, read.words.data(), nullptr, nullptr, &limit) == 1);
    Require(read.Has(descriptor));
}

static void SelectSocketAndFile() {
    std::array<unsigned char, 16> address{};
    const int receiver = BoundSocket(address);
    const int sender = ConnectedSocket(address);
    const int file = open_nid_postfix("/dev/urandom", 0, 0);
    Require(file >= 0 && file < DescriptorSetSize && file != receiver && file != sender);
    const int highest = std::max({receiver, sender, file});
    {
        GuestDescriptorSet read{};
        read.Add(receiver);
        GuestTimeval none{0, 0};
        Require(select_nid_postfix(receiver + 1, read.words.data(), nullptr, nullptr, &none) == 0);
        Require(!read.Has(receiver));
    }
    {
        GuestDescriptorSet read{};
        read.Add(receiver);
        read.Add(file);
        GuestTimeval none{0, 0};
        Require(select_nid_postfix(highest + 1, read.words.data(), nullptr, nullptr, &none) == 1);
        Require(read.Has(file) && !read.Has(receiver));
    }
    const char byte = 'd';
    Require(send_nid_postfix(sender, &byte, 1, 0) == 1);
    WaitReadable(receiver);
    {
        GuestDescriptorSet read{};
        GuestDescriptorSet write{};
        read.Add(receiver);
        read.Add(file);
        write.Add(sender);
        GuestTimeval none{0, 0};
        Require(select_nid_postfix(highest + 1, read.words.data(), write.words.data(), nullptr, &none) == 3);
        Require(read.Has(receiver) && read.Has(file) && write.Has(sender));
    }
    char received = 0;
    Require(recv_nid_postfix(receiver, &received, 1, 0) == 1 && received == byte);
    for (const int descriptor : {receiver, sender, file}) Require(close_nid_postfix(descriptor) == 0);
}

static void CloseReleasesNumber() {
    const int socket = Socket();
    Require(close_nid_postfix(socket) == 0);
    FileStat status{};
    Require(fstat_nid_disambig1_nid_postfix(socket, &status) == -1 && *__error_nid_postfix() == 9);
    std::vector<int> files;
    while (files.size() < 8 && (files.empty() || files.back() != socket)) {
        const int file = open_nid_postfix("/dev/urandom", 0, 0);
        Require(file >= 0);
        files.push_back(file);
    }
    Require(files.back() == socket);
    unsigned char bytes[4]{};
    Require(read_nid_postfix(socket, bytes, sizeof(bytes)) == sizeof(bytes));
    GuestPollDescriptor entry{socket, In, 0};
    Require(poll_nid_postfix(&entry, 1, 0) == 1 && entry.revents == In);
    for (const int file : files) Require(close_nid_postfix(file) == 0);
    const int kernelClosed = Socket();
    Require(sceKernelClose(kernelClosed) == 0);
    Require(sceKernelClose(kernelClosed) == KernelBadDescriptor);
    entry = {kernelClosed, In, 0};
    Require(poll_nid_postfix(&entry, 1, 0) == 1 && entry.revents == 0x20);
}

static void DuplicateKeepsSocket() {
    std::array<unsigned char, 16> address{};
    const int receiver = BoundSocket(address);
    const int sender = ConnectedSocket(address);
    const int duplicate = dup_nid_postfix(receiver);
    Require(duplicate >= 0 && duplicate < DescriptorSetSize && duplicate != receiver && duplicate != sender);
    Require(close_nid_postfix(receiver) == 0);
    const char byte = 'u';
    Require(send_nid_postfix(sender, &byte, 1, 0) == 1);
    WaitReadable(duplicate);
    char received = 0;
    Require(recv_nid_postfix(duplicate, &received, 1, 0) == 1 && received == byte);
    Require(close_nid_postfix(duplicate) == 0);
    Require(close_nid_postfix(sender) == 0);
}

static void SocketNumbersStayInDescriptorSet() {
    const int socket = Socket();
    std::vector<int> files;
    for (;;) {
        const int file = open_nid_postfix("/dev/urandom", 0, 0);
        Require(file >= 0);
        if (file >= DescriptorSetSize) {
            Require(close_nid_postfix(file) == 0);
            break;
        }
        files.push_back(file);
    }
    Require(dup_nid_postfix(socket) == -1 && *__error_nid_postfix() == 24);
    Require(fcntl_nid_postfix(socket, DuplicateCommand, 0) == -1 && *__error_nid_postfix() == 24);
    Require(fcntl_nid_postfix(socket, DuplicateCommand, DescriptorSetSize) == -1 && *__error_nid_postfix() == 24);
    Require(fcntl_nid_postfix(socket, DuplicateCloseOnExec, 0) == -1 && *__error_nid_postfix() == 45);
    Require(socket_nid_postfix(Inet, Datagram, 0) == -1 && *__error_nid_postfix() == 24);
    Require(dup2_nid_postfix(socket, DescriptorSetSize) == -1 && *__error_nid_postfix() == 24);
    for (const int file : files) Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(socket) == 0);
}

static void SocketTypeErrors() {
    constexpr int PrototypeError = 41;
    constexpr int ProtocolError = 43;
    constexpr int FamilyError = 47;
    constexpr int Inet6 = 28;
    for (const int family : {Inet, Inet6}) {
        Require(socket_nid_postfix(family, 99, 0) == -1 && *__error_nid_postfix() == PrototypeError);
        Require(socket_nid_postfix(family, 4, 0) == -1 && *__error_nid_postfix() == PrototypeError);
        Require(socket_nid_postfix(family, 0, 0) == -1 && *__error_nid_postfix() == ProtocolError);
        Require(socket_nid_postfix(family, 99, 6) == -1 && *__error_nid_postfix() == ProtocolError);
        Require(socket_nid_postfix(family, 1, 17) == -1 && *__error_nid_postfix() == ProtocolError);
        Require(socket_nid_postfix(family, 2, 6) == -1 && *__error_nid_postfix() == ProtocolError);
    }
    Require(socket_nid_postfix(99, 1, 0) == -1 && *__error_nid_postfix() == FamilyError);
}

int main() {
    SocketTypeErrors();
    SelectSocketAndFile();
    CloseReleasesNumber();
    DuplicateKeepsSocket();
    SocketNumbersStayInDescriptorSet();
    return 0;
}
