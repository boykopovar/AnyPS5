#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static_assert(sizeof(NetMsghdr) == 48 && offsetof(NetMsghdr, iov) == 16 && offsetof(NetMsghdr, flags) == 44);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static DWORD Protection(const void* address) {
    MEMORY_BASIC_INFORMATION information{};
    Require(VirtualQuery(address, &information, sizeof(information)) != 0, "query host protection");
    return information.Protect;
}

int main(int argc, char** argv) {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t capacity = 32;
    constexpr const char* names[] = {"recv", "recvfrom", "read", "sceKernelRead", "sceNetRecv", "sceNetRecvfrom", "sceNetRecvmsg"};
    Require(argc == 2 || argc == 3, "receive API argument required");
    Require(argc != 3 || std::strcmp(argv[2], "tcp") == 0, "unknown transport");
    const bool stream = argc == 3;
    const auto selected = std::find_if(std::begin(names), std::end(names), [&](const char* name) {
        return std::strcmp(name, argv[1]) == 0;
    });
    Require(selected != std::end(names), "unknown receive API");
    const auto api = selected - std::begin(names);
    const bool net = api >= 4;
    const bool from = api == 1 || api == 5;
    const bool peek = api != 2 && api != 3;
    const auto create = [&] {
        return net ? sceNetSocket(nullptr, 2, stream ? 1 : 2, stream ? 6 : 17) :
            socket_nid_postfix(2, stream ? 1 : 2, stream ? 6 : 17);

    };
    int receiver = create();
    const int sender = create();
    Require(receiver >= 0 && sender >= 0, "create sockets");
    std::array<unsigned char, 16> destination{16, 2, 0, 0, 127, 0, 0, 1};
    Require((net ? sceNetBind_nid_postfix(receiver, destination.data(), destination.size()) :
        bind_nid_postfix(receiver, destination.data(), destination.size())) == 0, "bind receiver");
    auto senderAddress = destination;
    Require((net ? sceNetBind_nid_postfix(sender, senderAddress.data(), senderAddress.size()) :
        bind_nid_postfix(sender, senderAddress.data(), senderAddress.size())) == 0, "bind sender");
    std::uint32_t size = destination.size();
    Require((net ? sceNetGetsockname(receiver, destination.data(), &size) :
        getsockname_nid_postfix(receiver, destination.data(), &size)) == 0, "receiver address");
    size = senderAddress.size();
    Require((net ? sceNetGetsockname(sender, senderAddress.data(), &size) :
        getsockname_nid_postfix(sender, senderAddress.data(), &size)) == 0, "sender address");
    if (stream) {
        Require((net ? sceNetListen(receiver, 1) : listen_nid_postfix(receiver, 1)) == 0, "listen on loopback");
        Require((net ? sceNetConnect(sender, destination.data(), destination.size()) :
            connect_nid_postfix(sender, destination.data(), destination.size())) == 0, "connect sender");
        const int accepted = net ? sceNetAccept(receiver, nullptr, nullptr) : accept_nid_postfix(receiver, nullptr, nullptr);
        Require(accepted >= 0, "accept sender");
        Require((net ? sceNetSocketClose(receiver) : close_nid_postfix(receiver)) == 0, "close listener");
        receiver = accepted;
    }
    const auto nonblocking = [&](bool enabled) {
        const int value = enabled;
        Require((net ? sceNetSetsockopt(receiver, 0xffff, 0x1200, &value, sizeof(value)) :
            fcntl_nid_postfix(receiver, 4, enabled ? 4 : 0)) == 0, "set receiver mode");
    };
    nonblocking(true);
    const char payload[] = "watched socket data";
    const auto send = [&] {
        Require((net ? sceNetSendto(sender, payload, sizeof(payload), 0, stream ? nullptr : destination.data(),
            stream ? 0 : destination.size()) : sendto_nid_postfix(sender, payload, sizeof(payload), 0,
            stream ? nullptr : destination.data(), stream ? 0 : destination.size())) == sizeof(payload),
            "send packet");
    };
    const auto receive = [&](void* buffer, std::size_t bytes, int flags, void* address, std::uint32_t* addressLength) {
        switch (api) {
            case 0: return recv_nid_postfix(receiver, buffer, bytes, flags);
            case 1: return recvfrom_nid_postfix(receiver, buffer, bytes, flags, address, addressLength);
            case 2: return read_nid_postfix(receiver, buffer, bytes);
            case 3: return sceKernelRead(receiver, buffer, bytes);
            case 4: return sceNetRecv(receiver, buffer, bytes, flags);
            case 5: return sceNetRecvfrom(receiver, buffer, bytes, flags, address, addressLength);
            default: {
                const auto first = std::min<std::size_t>(bytes, 3);
                NetIovec vectors[]{{buffer, first}, {nullptr, 0}, {static_cast<char*>(buffer) + first, bytes - first}};
                NetMsghdr message{nullptr, 0, vectors, 3, nullptr, 0, 0};
                return sceNetRecvmsg(receiver, &message, flags);
            }
        }
    };
    const auto waitForPacket = [&] {
        std::array<char, capacity> buffer{};
        int* error = net ? sceNetErrnoLoc() : __error_nid_postfix();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        std::int64_t result;
        do {
            result = net ? sceNetRecv(receiver, buffer.data(), buffer.size(), 2) :
                recv_nid_postfix(receiver, buffer.data(), buffer.size(), 2);

            if (result == sizeof(payload) || (result < 0 && *error != 35)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        Require(result == sizeof(payload), "wait for complete packet");
    };
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &physical) == 0, "allocate direct memory");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page * 2, 3, 0, physical, 0) == 0, "map read/write memory");
    void* twin = nullptr;
    Require(sceKernelMapDirectMemory(&twin, page * 2, 3, 0, physical, 0) == 0, "map read/write memory a second time");
    auto* buffer = static_cast<char*>(mapping) + page - 7;
    const auto collect = [&] {
        std::array<void*, 16> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), page * 2,
            pages.data(), &count, true), "collect tracked writes");
        return count;
    };
    const auto reset = [&] {
        const GuestArena::HostWrite destination(buffer - 1, capacity + 2);
        Require(destination.Open(), "reset writable receive buffer");
        std::memset(buffer - 1, '!', capacity + 2);
    };
    int* error = net ? sceNetErrnoLoc() : __error_nid_postfix();
    for (const std::size_t bytes : {std::size_t{0}, std::size_t{4}, sizeof(payload), capacity}) {
        send();
        waitForPacket();
        for (const int flags : {2, 0}) {
            if (flags == 2 && !peek) continue;
            reset();
            collect();
            Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + capacity - 1) == PAGE_READONLY,
                "write tracking arms both guest pages");
            std::array<unsigned char, 32> address{};
            size = address.size();
            *error = 123;
            const auto result = receive(buffer, bytes, flags, stream ? nullptr : address.data(), stream ? nullptr : &size);
            const auto expected = std::min(bytes, sizeof(payload));
            if (result != static_cast<std::int64_t>(expected)) {
                std::fprintf(stderr, "%s capacity=%zu flags=%d result=%lld errno=%d\n", argv[1], bytes, flags,
                    static_cast<long long>(result), *error);
                return 1;
            }
            Require(*error == 123, "successful receive preserves errno");
            Require(buffer[-1] == '!' && std::memcmp(buffer, payload, expected) == 0 &&
                std::all_of(buffer + expected, buffer + capacity + 1, [](char value) { return value == '!'; }),
                "received data and buffer guards");
            if (from && !stream) Require(size == senderAddress.size() && std::memcmp(address.data(), senderAddress.data(), size) == 0,
                "sender address preserved");
            if (flags == 0 && ((bytes == 0 && !peek) || (stream && expected < sizeof(payload)))) {
                std::array<char, capacity> retained{};
                Require(receive(retained.data(), retained.size(), 0, nullptr, nullptr) == sizeof(payload) - expected &&
                    std::memcmp(retained.data(), payload + expected, sizeof(payload) - expected) == 0,
                    "short stream receive or zero-byte read retains unread data");
            }
            if (bytes != 0) Require(collect() != 0, "receive reported as a tracked write");
            collect();
            Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + capacity - 1) == PAGE_READONLY,
                "receive releases the host write guard");
        }
    }
    if (from && !stream) {
        std::int64_t outputPhysical = 0;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &outputPhysical) == 0,
            "allocate sender output memory");
        void* outputMapping = nullptr;
        Require(sceKernelMapDirectMemory(&outputMapping, page * 2, 3, 0, outputPhysical, 0) == 0,
            "map sender output memory");
        void* outputTwin = nullptr;
        Require(sceKernelMapDirectMemory(&outputTwin, page * 2, 3, 0, outputPhysical, 0) == 0,
            "map sender output memory a second time");
        const auto collectOutputs = [&] {
            std::array<void*, 16> pages{};
            std::size_t count = pages.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(outputMapping), page * 2,
                pages.data(), &count, true), "collect sender output writes");
            return count;
        };
        for (const int mask : {1, 2, 3}) {
            std::array<unsigned char, 32> hostAddress{};
            std::uint32_t hostLength = hostAddress.size();
            auto* address = (mask & 1) ? static_cast<unsigned char*>(outputMapping) + 16 : hostAddress.data();
            auto* length = (mask & 2) ? reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(outputMapping) + page + 16) :
                &hostLength;
            {
                const GuestArena::HostWrite addressDestination(address, hostAddress.size());
                const GuestArena::HostWrite lengthDestination(length, sizeof(*length));
                Require(addressDestination.Open() && lengthDestination.Open(), "initialize sender outputs");
                std::memset(address, 0xa5, hostAddress.size());
                *length = hostAddress.size();
            }
            collectOutputs();
            Require(collectOutputs() == 0, "sender outputs initially clean");
            if (mask & 1) Require(Protection(address) == PAGE_READONLY, "sender address is watched and armed");
            if (mask & 2) Require(Protection(length) == PAGE_READONLY, "sender length is watched and armed");
            send();
            waitForPacket();
            reset();
            collect();
            *error = 123;
            Require(receive(buffer, capacity, 0, address, length) == sizeof(payload) && *error == 123 &&
                std::memcmp(buffer, payload, sizeof(payload)) == 0, "receive into watched sender outputs");
            Require(*length == senderAddress.size() && std::memcmp(address, senderAddress.data(), *length) == 0,
                "watched sender outputs are correct");
            const auto expectedPages = static_cast<std::size_t>(((mask & 1) != 0) + ((mask & 2) != 0));
            Require(collectOutputs() == expectedPages, "each watched sender output is tracked");
            Require(collectOutputs() == 0, "sender output writes cleared");
            if (mask & 1) Require(Protection(address) == PAGE_READONLY, "sender address rearmed");
            if (mask & 2) Require(Protection(length) == PAGE_READONLY, "sender length rearmed");
        }
        if (api == 5) {
            for (const bool readOnlyAddress : {true, false}) {
                auto* address = static_cast<unsigned char*>(outputMapping) + 16;
                auto* length = reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(outputMapping) + page + 16);
                {
                    const GuestArena::HostWrite addressDestination(address, 32);
                    const GuestArena::HostWrite lengthDestination(length, sizeof(*length));
                    Require(addressDestination.Open() && lengthDestination.Open(), "initialize refused sender outputs");
                    std::memset(address, 0xa5, 32);
                    *length = 32;
                }
                void* refused = static_cast<char*>(outputMapping) + (readOnlyAddress ? 0 : page);
                Require(sceKernelMprotect(refused, page, 1) == 0, "make one sender output read-only");
                send();
                waitForPacket();
                std::array<char, capacity> recovery{};
                Require(receive(recovery.data(), recovery.size(), 0, address, length) == static_cast<int>(0x8041010eu) && *error == 14,
                    "read-only sender output fails with EFAULT");
                Require(*length == 32 && std::all_of(address, address + 32, [](unsigned char value) { return value == 0xa5; }),
                    "refused sender outputs remain unchanged");
                Require(receive(recovery.data(), recovery.size(), 0, nullptr, nullptr) == sizeof(payload) &&
                    std::memcmp(recovery.data(), payload, sizeof(payload)) == 0, "refused sender output preserves queued data");
                Require(sceKernelMprotect(refused, page, 3) == 0, "restore sender output permission");
            }
            auto* address = static_cast<unsigned char*>(outputMapping) + 16;
            Require(sceKernelMprotect(outputMapping, page, 1) == 0, "make an unused sender address read-only");
            std::uint32_t zero = 0;
            std::array<char, capacity> recovery{};
            send();
            waitForPacket();
            Require(receive(recovery.data(), recovery.size(), 0, address, &zero) == sizeof(payload) && zero == 0 &&
                std::memcmp(recovery.data(), payload, sizeof(payload)) == 0, "zero-capacity sender address needs no write access");
            Require(sceKernelMprotect(outputMapping, page, 3) == 0, "restore unused sender address permission");
            address = static_cast<unsigned char*>(outputMapping) + page - senderAddress.size();
            Require(sceKernelMprotect(static_cast<char*>(outputMapping) + page, page, 1) == 0,
                "make bytes beyond the sender address read-only");
            std::uint32_t oversized = 32;
            send();
            waitForPacket();
            Require(receive(recovery.data(), recovery.size(), 0, address, &oversized) == sizeof(payload) &&
                oversized == senderAddress.size() && std::memcmp(address, senderAddress.data(), oversized) == 0,
                "guard only the sender address bytes the receive writes");
            Require(sceKernelMprotect(static_cast<char*>(outputMapping) + page, page, 3) == 0,
                "restore bytes beyond the sender address");
        }
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(outputTwin), page * 2) == 0, "unmap the second sender output mapping");
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(outputMapping), page * 2) == 0, "unmap sender outputs");
        Require(sceKernelReleaseDirectMemory(outputPhysical, page * 2) == 0, "release sender output memory");
    }
    reset();
    collect();
    *error = 0;
    const auto empty = receive(buffer, capacity, 0, nullptr, nullptr);
    const auto expectedAgain = api == 3 ? static_cast<int>(0x80020023u) :
        net ? static_cast<int>(0x80410123u) : -1;
    Require(empty == expectedAgain && *error == 35, "empty nonblocking receive reports EAGAIN");
    collect();
    Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + capacity - 1) == PAGE_READONLY,
        "failed receive releases the host write guard");
    if (api == 6) {
        Require(sceKernelMprotect(static_cast<char*>(mapping) + page, page, 1) == 0,
            "make the later receive vector read-only");
        send();
        waitForPacket();
        Require(receive(buffer, capacity, 0, nullptr, nullptr) == static_cast<int>(0x8041010eu) && *error == 14,
            "a later read-only vector fails with EFAULT");
        Require(std::all_of(buffer - 1, buffer + capacity + 1, [](char value) { return value == '!'; }),
            "a rejected vector list leaves every destination unchanged");
        collect();
        Require(Protection(buffer) == PAGE_READONLY, "a later vector failure releases earlier guards");
        std::array<char, capacity> recovery{};
        Require(receive(recovery.data(), recovery.size(), 0, nullptr, nullptr) == sizeof(payload),
            "a rejected vector list preserves queued data");
        Require(sceKernelMprotect(static_cast<char*>(mapping) + page, page, 3) == 0,
            "restore the later receive vector");
        NetIovec vectors[]{{buffer, 3}, {nullptr, 0}, {buffer + 3, capacity - 3}};
        auto* message = reinterpret_cast<NetMsghdr*>(static_cast<char*>(twin) + 32);
        const NetMsghdr initial{nullptr, 0, vectors, 3, nullptr, 123, -1};
        {
            const GuestArena::HostWrite destination(message, sizeof(*message));
            Require(destination.Open(), "initialize the receive message");
            std::memcpy(message, &initial, sizeof(initial));
        }
        Require(sceKernelMprotect(twin, page, 1) == 0, "make the receive message read-only");
        send();
        waitForPacket();
        Require(sceNetRecvmsg(receiver, message, 0) == static_cast<int>(0x8041010eu) && *error == 14,
            "read-only message outputs fail with EFAULT");
        Require(message->flags == -1 && message->control_length == 123, "rejected message outputs unchanged");
        Require(receive(recovery.data(), recovery.size(), 0, nullptr, nullptr) == sizeof(payload),
            "a rejected message preserves queued data");
        Require(sceKernelMprotect(twin, page, 3) == 0, "restore receive message write permission");
        reset();
        collect();
    }
    Require(sceKernelMprotect(mapping, page * 2, 1) == 0, "make guest memory read-only");
    send();
    waitForPacket();
    *error = 0;
    const auto rejected = receive(buffer, capacity, 0, nullptr, nullptr);
    const auto expectedError = api == 3 ? static_cast<int>(0x8002000eu) :
        net ? static_cast<int>(0x8041010eu) : -1;
    Require(rejected == expectedError && *error == 14, "read-only destination fails with EFAULT");
    Require(std::all_of(buffer - 1, buffer + capacity + 1, [](char value) { return value == '!'; }),
        "read-only destination unchanged");
    std::array<char, capacity> recovery{};
    Require(receive(recovery.data(), recovery.size(), 0, nullptr, nullptr) == sizeof(payload) &&
        std::memcmp(recovery.data(), payload, sizeof(payload)) == 0, "rejected write preserves queued packet");
    send();
    waitForPacket();
    *error = 123;
    Require(receive(buffer, 0, peek ? 2 : 0, nullptr, nullptr) == 0 && *error == 123,
        "zero-byte receive does not require a writable destination");
    Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + capacity - 1) == PAGE_READONLY,
        "zero-byte receive preserves read-only protection");
    Require(receive(recovery.data(), recovery.size(), 0, nullptr, nullptr) == sizeof(payload) &&
        std::memcmp(recovery.data(), payload, sizeof(payload)) == 0, "zero-byte peek or read retains queued data");
    Require(sceKernelMprotect(mapping, page * 2, 3) == 0, "restore guest write permission");
    reset();
    collect();
    nonblocking(false);
    std::int64_t result = -1;
    int workerError = 0;
    std::thread worker([&] {
        int* slot = net ? sceNetErrnoLoc() : __error_nid_postfix();
        *slot = 123;
        result = receive(buffer, capacity, 0, nullptr, nullptr);
        workerError = *slot;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (Protection(buffer) != PAGE_READWRITE && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(Protection(buffer) == PAGE_READWRITE, "blocking receive opens its destination");
    collect();
    Require(Protection(buffer) == PAGE_READWRITE && Protection(buffer + capacity - 1) == PAGE_READWRITE,
        "collection does not rearm an in-flight receive");
    send();
    worker.join();
    Require(result == sizeof(payload) && workerError == 123 && std::memcmp(buffer, payload, sizeof(payload)) == 0,
        "blocking receive completes and preserves errno");
    Require(collect() != 0, "completion remains dirty after an in-flight collection");
    Require(Protection(buffer) == PAGE_READONLY, "completed receive can be rearmed");
    Require((net ? sceNetSocketClose(sender) : close_nid_postfix(sender)) == 0, "close sender");
    Require((net ? sceNetSocketClose(receiver) : close_nid_postfix(receiver)) == 0, "close receiver");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(twin), page * 2) == 0, "unmap the second direct memory mapping");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), page * 2) == 0, "unmap direct memory");
    Require(sceKernelReleaseDirectMemory(physical, page * 2) == 0, "release direct memory");
}
