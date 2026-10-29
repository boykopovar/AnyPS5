#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

static_assert(sizeof(GuestSigaction) == 32);

static_assert(sizeof(GuestMsghdr) == 48);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "underscore entry check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int TcpLevel = 6;
constexpr int TcpNoDelay = 1;
constexpr int UnknownTcpOption = 0x7777;
constexpr int OutOfBand = 0x1;
constexpr int Truncated = 0x10;
constexpr int Interrupted = 4;
constexpr int BadDescriptor = 9;
constexpr int BadAddress = 14;
constexpr int Invalid = 22;
constexpr int NoProtocolOption = 42;
constexpr int NotSupported = 45;

using Address = std::array<std::uint8_t, 16>;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static bool Filled(const Address& address, std::uint8_t value) {
    return std::all_of(address.begin(), address.end(), [value](std::uint8_t byte) { return byte == value; });
}

static void StreamSockets() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0);
    Address address{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(_bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(Fails(_bind_nid_postfix(listener, address.data(), address.size()), Invalid));
    Require(_listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(_getsockname_nid_postfix(listener, address.data(), &size) == 0 && size == 16);
    Require(address[0] == 16 && address[1] == Inet && (address[2] != 0 || address[3] != 0) && address[4] == 127 && address[7] == 1);

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0);
    Require(Fails(_accept_nid_postfix(client, nullptr, nullptr), Invalid));
    Require(_connect_nid_postfix(client, address.data(), address.size()) == 0);
    Address peer;
    peer.fill(0xa5);
    size = peer.size();
    const int accepted = _accept_nid_postfix(listener, peer.data(), &size);
    Require(accepted >= 0 && size == 16);
    Address local{};
    size = local.size();
    Require(_getsockname_nid_postfix(client, local.data(), &size) == 0 && size == 16 && local == peer);
    Address remote{};
    size = remote.size();
    Require(_getpeername_nid_postfix(client, remote.data(), &size) == 0 && size == 16 && remote == address);
    remote.fill(0);
    size = remote.size();
    Require(_getpeername_nid_postfix(accepted, remote.data(), &size) == 0 && size == 16 && remote == local);

    int value = -1;
    std::uint32_t valueSize = sizeof(value);
    Require(_getsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &value, &valueSize) == 0 && value == 0 && valueSize == sizeof(value));
    const int enabled = 1;
    Require(_setsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &enabled, sizeof(enabled)) == 0);
    valueSize = sizeof(value);
    Require(_getsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &value, &valueSize) == 0 && value == 4 && valueSize == sizeof(value));
    Require(Fails(_setsockopt_nid_postfix(client, TcpLevel, UnknownTcpOption, &enabled, sizeof(enabled)), NoProtocolOption));

    const char text[] = "underscore stream";
    Require(_sendto_nid_postfix(client, text, sizeof(text), 0, nullptr, 0) == sizeof(text));
    char received[sizeof(text)]{};
    Address sender;
    sender.fill(0xa5);
    std::uint32_t senderSize = sender.size();
    Require(_recvfrom_nid_postfix(accepted, received, sizeof(received), 0, sender.data(), &senderSize) == sizeof(text));
    Require(std::memcmp(received, text, sizeof(text)) == 0 && senderSize == 0 && Filled(sender, 0xa5));

    for (const int descriptor : {accepted, client, listener}) Require(close_nid_postfix(descriptor) == 0);
    Address untouched;
    untouched.fill(0xa5);
    std::uint32_t untouchedSize = 7;
    value = -1;
    Require(Fails(_getsockname_nid_postfix(listener, untouched.data(), &untouchedSize), BadDescriptor));
    Require(Fails(_getpeername_nid_postfix(client, untouched.data(), &untouchedSize), BadDescriptor));
    Require(Fails(_accept_nid_postfix(listener, untouched.data(), &untouchedSize), BadDescriptor));
    Require(Fails(_getsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &value, &untouchedSize), BadDescriptor));
    Require(Fails(_listen_nid_postfix(listener, 4), BadDescriptor));
    Require(Filled(untouched, 0xa5) && untouchedSize == 7 && value == -1);
}

static void DatagramSockets() {
    const int receiver = socket_nid_postfix(Inet, Datagram, 0);
    const int sender = socket_nid_postfix(Inet, Datagram, 0);
    Require(receiver >= 0 && sender >= 0);
    Address destination{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(_bind_nid_postfix(receiver, destination.data(), destination.size()) == 0);
    std::uint32_t size = destination.size();
    Require(_getsockname_nid_postfix(receiver, destination.data(), &size) == 0 && size == 16 && (destination[2] != 0 || destination[3] != 0));
    Require(Fails(_listen_nid_postfix(receiver, 1), NotSupported));

    const char datagram[] = "underscore datagram";
    Require(Fails(_sendto_nid_postfix(sender, datagram, sizeof(datagram), OutOfBand, destination.data(), destination.size()), NotSupported));
    Require(_sendto_nid_postfix(sender, datagram, sizeof(datagram), 0, destination.data(), destination.size()) == sizeof(datagram));
    Address senderName{};
    size = senderName.size();
    Require(_getsockname_nid_postfix(sender, senderName.data(), &size) == 0 && size == 16);
    char buffer[32]{};
    Address source{};
    size = source.size();
    Require(_recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, source.data(), &size) == sizeof(datagram));
    Require(std::strcmp(buffer, datagram) == 0 && size == 16 && source[0] == 16 && source[1] == Inet);
    Require(source[2] == senderName[2] && source[3] == senderName[3] && source[4] == 127 && source[7] == 1);

    char head[] = "scatter ";
    char tail[] = "gather";
    KernelIovec parts[2]{{head, 8}, {tail, 6}};
    GuestMsghdr outgoing{destination.data(), static_cast<std::uint32_t>(destination.size()), parts, 2, nullptr, 0, 0};
    Require(_sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    char first[4]{};
    char second[16]{};
    KernelIovec pieces[2]{{first, sizeof(first)}, {second, sizeof(second)}};
    source.fill(0);
    GuestMsghdr incoming{source.data(), static_cast<std::uint32_t>(source.size()), pieces, 2, nullptr, 0, -1};
    Require(_recvmsg_nid_postfix(receiver, &incoming, 0) == 14);
    Require(std::memcmp(first, "scat", 4) == 0 && std::memcmp(second, "ter gather", 10) == 0);
    Require(incoming.flags == 0 && incoming.nameLength == 16 && source[2] == senderName[2] && source[3] == senderName[3] && source[4] == 127);
    Require(_sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    char shortBuffer[5]{};
    KernelIovec single{shortBuffer, sizeof(shortBuffer)};
    GuestMsghdr truncated{nullptr, 0, &single, 1, nullptr, 0, 0};
    Require(_recvmsg_nid_postfix(receiver, &truncated, 0) == 5 && std::memcmp(shortBuffer, "scatt", 5) == 0 && truncated.flags == Truncated);
    Require(Fails(_sendmsg_nid_postfix(sender, nullptr, 0), BadAddress));
    Require(Fails(_recvmsg_nid_postfix(receiver, nullptr, 0), BadAddress));

    Require(Fails(_connect_nid_postfix(sender, destination.data(), 15), Invalid));
    Require(_connect_nid_postfix(sender, destination.data(), destination.size()) == 0);
    Address peer{};
    size = peer.size();
    Require(_getpeername_nid_postfix(sender, peer.data(), &size) == 0 && size == 16 && peer == destination);
    Require(_sendto_nid_postfix(sender, datagram, sizeof(datagram), 0, nullptr, 0) == sizeof(datagram));
    std::memset(buffer, 0, sizeof(buffer));
    Require(_recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, nullptr, nullptr) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);

    Require(close_nid_postfix(receiver) == 0 && close_nid_postfix(sender) == 0);
    std::memset(buffer, 0x5a, sizeof(buffer));
    source.fill(0xa5);
    size = 9;
    const int enabled = 1;
    Require(Fails(_recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, source.data(), &size), BadDescriptor));
    Require(Fails(_recvmsg_nid_postfix(receiver, &incoming, 0), BadDescriptor));
    Require(Fails(_sendto_nid_postfix(sender, datagram, sizeof(datagram), 0, nullptr, 0), BadDescriptor));
    Require(Fails(_sendmsg_nid_postfix(sender, &outgoing, 0), BadDescriptor));
    Require(Fails(_bind_nid_postfix(receiver, destination.data(), destination.size()), BadDescriptor));
    Require(Fails(_connect_nid_postfix(sender, destination.data(), destination.size()), BadDescriptor));
    Require(Fails(_setsockopt_nid_postfix(sender, TcpLevel, TcpNoDelay, &enabled, sizeof(enabled)), BadDescriptor));
    Require(std::all_of(std::begin(buffer), std::end(buffer), [](char byte) { return byte == 0x5a; }));
    Require(Filled(source, 0xa5) && size == 9);
}

static std::set<std::string> Names(const char* records, int bytes) {
    std::set<std::string> names;
    for (int offset = 0; offset < bytes;) {
        std::uint16_t recordLength = 0;
        std::memcpy(&recordLength, records + offset + 4, sizeof(recordLength));
        Require(recordLength >= 12 && offset + recordLength <= bytes);
        names.emplace(records + offset + 8, static_cast<std::uint8_t>(records[offset + 7]));
        offset += recordLength;
    }
    return names;
}

static void DirectoryEntries() {
    const auto root = std::filesystem::path("anyps5-underscore-entries-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    { std::ofstream stream(root / "entry"); }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 4096> records{};
    std::int64_t base = -1;
    const int read = _getdirentries_nid_postfix(directory, records.data(), static_cast<int>(records.size()), &base);
    Require(read > 0 && base == 0);
    const std::set<std::string> listed{".", "..", "entry"};
    Require(Names(records.data(), read) == listed);
    Require(sceKernelLseek(directory, 0, 0) == 0);
    std::array<char, 4096> expected{};
    std::int64_t expectedBase = -1;
    Require(getdirentries_nid_postfix(directory, expected.data(), static_cast<int>(expected.size()), &expectedBase) == read);
    Require(expectedBase == base && std::memcmp(expected.data(), records.data(), static_cast<std::size_t>(read)) == 0);
    std::int64_t end = -1;
    Require(_getdirentries_nid_postfix(directory, records.data(), static_cast<int>(records.size()), &end) == 0 && end > 0);

    records.fill(0x5a);
    std::int64_t untouched = 1234;
    Require(Fails(_getdirentries_nid_postfix(directory, records.data(), 0, &untouched), Invalid));
    Require(Fails(_getdirentries_nid_postfix(directory, nullptr, 64, &untouched), BadAddress));
    Require(sceKernelClose(directory) == 0);
    Require(Fails(_getdirentries_nid_postfix(directory, records.data(), static_cast<int>(records.size()), &untouched), BadDescriptor));
    Require(untouched == 1234 && std::all_of(records.begin(), records.end(), [](char byte) { return byte == 0x5a; }));
    std::filesystem::remove_all(root);
}

volatile std::sig_atomic_t received = 0;

void APS5_VABI Handler(int value) {
    received = value;
}

static bool Same(const GuestSigaction& left, const GuestSigaction& right) {
    return left.handler == right.handler && left.flags == right.flags && std::memcmp(&left.mask, &right.mask, sizeof(left.mask)) == 0;
}

static void Signals() {
    const auto handler = reinterpret_cast<std::uintptr_t>(Handler);
    const GuestSigaction term{handler, 0, {{0x4000, 0, 0, 0x80000000u}}};
    GuestSigaction initial{};
    Require(_sigaction_nid_postfix(15, nullptr, &initial) == 0);
    GuestSigaction previous;
    std::memset(&previous, 0xa5, sizeof(previous));
    Require(_sigaction_nid_postfix(15, &term, &previous) == 0 && Same(previous, initial));
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 15);
    GuestSigaction current;
    std::memset(&current, 0xa5, sizeof(current));
    Require(_sigaction_nid_postfix(15, nullptr, &current) == 0 && Same(current, term));

    GuestSigaction untouched;
    std::memset(&untouched, 0xa5, sizeof(untouched));
    for (const int invalid : {0, 129}) {
        std::memset(&current, 0xa5, sizeof(current));
        Require(Fails(_sigaction_nid_postfix(invalid, &term, &current), Invalid) && std::memcmp(&current, &untouched, sizeof(current)) == 0);
    }
    for (const int fixed : {9, 17}) {
        std::memset(&current, 0xa5, sizeof(current));
        Require(Fails(_sigaction_nid_postfix(fixed, &term, &current), Invalid) && std::memcmp(&current, &untouched, sizeof(current)) == 0);
    }
    Require(_sigaction_nid_postfix(15, &initial, &current) == 0 && Same(current, term));

    const GuestSigaction user{handler, 0, {}};
    GuestSigaction initialUser{};
    Require(_sigaction_nid_postfix(30, &user, &initialUser) == 0);
    const GuestSignalSet blocked{{1u << 29, 0, 0, 0}};
    const GuestSignalSet empty{{0, 0, 0, 0}};
    GuestSignalSet saved{};
    Require(sigprocmask_nid_postfix(3, &blocked, &saved) == 0);
    received = 0;
    Require(raise_nid_postfix(30) == 0 && received == 0);
    Require(Fails(_sigsuspend_nid_postfix(&empty), Interrupted) && received == 30);
    GuestSignalSet restored{};
    Require(sigprocmask_nid_postfix(1, nullptr, &restored) == 0 && std::memcmp(&restored, &blocked, sizeof(restored)) == 0);
    Require(sigprocmask_nid_postfix(3, &saved, nullptr) == 0);
    Require(_sigaction_nid_postfix(30, &initialUser, nullptr) == 0);
}

int main() {
    StreamSockets();
    DatagramSockets();
    DirectoryEntries();
    Signals();
}
