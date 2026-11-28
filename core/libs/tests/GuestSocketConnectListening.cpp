#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketSelectDeadline.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorInvalid = 22;
constexpr int ErrorConnectionReset = 54;
constexpr std::size_t Page = 0x4000;

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

void VerifySelectDeadline() {
    using Clock = std::chrono::steady_clock;
    const auto epoch = Clock::time_point{};
    const auto preEpoch = Clock::time_point(Clock::duration(-1));
    Require(KernelSocketSelect::DeadlineAfter(preEpoch, 0, 0) == preEpoch,
        "pre-epoch zero timeout must preserve start");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point(std::chrono::seconds(-1)), 1, 500000) ==
            Clock::time_point(std::chrono::milliseconds(500)),
        "pre-epoch -1s plus (1s, 500000us) must equal +0.5s");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), 0, 0) == Clock::time_point::min(),
        "min() zero timeout must preserve min()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), 0, 0) == Clock::time_point::max(),
        "max() zero timeout must preserve max()");

    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::min(), 0) == Clock::time_point::min(),
        "INT64_MIN seconds must saturate to min()");
    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::min(), -1) == Clock::time_point::min(),
        "INT64_MIN seconds with -1us must saturate to min()");
    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min()) ==
            Clock::time_point::min(),
        "INT64_MIN seconds and microseconds must saturate to min()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), 0, -1) == Clock::time_point::min(),
        "min() minus 1us must saturate to min()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min() + std::chrono::seconds(1), -2, 0) == Clock::time_point::min(),
        "underflow past min() must saturate to min()");

    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::max(), 0) == Clock::time_point::max(),
        "INT64_MAX seconds must saturate to max()");
    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::max(), 1000000) == Clock::time_point::max(),
        "INT64_MAX seconds plus 1s in microseconds must saturate to max()");
    Require(KernelSocketSelect::DeadlineAfter(epoch, std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max()) ==
            Clock::time_point::max(),
        "INT64_MAX seconds and microseconds must saturate to max()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), 0, 1) == Clock::time_point::max(),
        "max() plus 1us must saturate to max()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max() - std::chrono::seconds(1), 2, 0) == Clock::time_point::max(),
        "overflow past max() must saturate to max()");

    Require(KernelSocketSelect::DeadlineAfter(epoch, 1, -500000) == Clock::time_point(std::chrono::milliseconds(500)),
        "mixed-sign (1, -500000) must normalize to +0.5s");
    Require(KernelSocketSelect::DeadlineAfter(epoch, -1, 500000) == Clock::time_point(std::chrono::milliseconds(-500)),
        "mixed-sign (-1, 500000) must normalize to -0.5s");
    Require(KernelSocketSelect::DeadlineAfter(epoch, 1, -1500000) == Clock::time_point(std::chrono::milliseconds(-500)),
        "mixed-sign (1, -1500000) must normalize to -0.5s");
    Require(KernelSocketSelect::DeadlineAfter(epoch, -1, 1500000) == Clock::time_point(std::chrono::milliseconds(500)),
        "mixed-sign (-1, 1500000) must normalize to +0.5s");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max() - std::chrono::seconds(2), 3, -1500000) ==
            Clock::time_point::max() - std::chrono::milliseconds(500),
        "large positive seconds with negative microseconds must not saturate prematurely");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min() + std::chrono::seconds(2), -3, 1500000) ==
            Clock::time_point::min() + std::chrono::milliseconds(500),
        "large negative seconds with positive microseconds must not saturate prematurely");

    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), std::numeric_limits<std::int64_t>::max(), 0) ==
            Clock::time_point::max(),
        "full-range INT64_MAX from min() must saturate to max()");
    Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), std::numeric_limits<std::int64_t>::min(), 0) ==
            Clock::time_point::min(),
        "full-range INT64_MIN from max() must saturate to min()");

    if constexpr (std::is_same_v<Clock::duration, std::chrono::nanoseconds>) {
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), 15000000000LL, 0).time_since_epoch().count() ==
                5776627963145224192LL,
            "min() plus 15e9s must span past INT64_MAX ticks without duration overflow");
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), 18446744073LL, 709551LL) ==
                Clock::time_point::max() - std::chrono::nanoseconds(615),
            "exact sub-saturation min()-to-max() span must reach max() - 615ns");
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::min(), 18446744073LL, 709552LL) == Clock::time_point::max(),
            "min()-to-max() span exceeding max() by 385ns must saturate to max()");
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), -15000000000LL, 0).time_since_epoch().count() ==
                -5776627963145224193LL,
            "max() minus 15e9s must span past INT64_MAX ticks without duration overflow");
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), -18446744073LL, -709551LL) ==
                Clock::time_point::min() + std::chrono::nanoseconds(615),
            "exact sub-saturation max()-to-min() span must reach min() + 615ns");
        Require(KernelSocketSelect::DeadlineAfter(Clock::time_point::max(), -18446744073LL, -709552LL) == Clock::time_point::min(),
            "max()-to-min() span exceeding min() by 385ns must saturate to min()");
    }
}

}

int main() {
    VerifySelectDeadline();
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket failed");
    std::array<std::uint8_t, 16> address{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0, "bind failed");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &length) == 0 && length == address.size(), "getsockname failed");
    Require(listen_nid_postfix(listener, 4) == 0, "listen failed");

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, address.data(), address.size()) == -1 && *__error_nid_postfix() == ErrorNotSupported,
        "connect on a listening socket must fail with EOPNOTSUPP (45)");

    const int plain = socket_nid_postfix(Inet, Stream, 0);
    Require(plain >= 0, "socket failed");
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(plain, nullptr, address.size()) == -1, "connect with a null address must fail");
    const int nullError = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, nullptr, address.size()) == -1 && *__error_nid_postfix() == nullError,
        "connect on a listening socket with a null address must fail as on any socket, not with EOPNOTSUPP");
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(plain, address.data(), 1) == -1, "connect with a 1-byte address must fail");
    const int shortError = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, address.data(), 1) == -1 && *__error_nid_postfix() == shortError,
        "connect on a listening socket with a 1-byte address must fail as on any socket, not with EOPNOTSUPP");
    Require(nullError != ErrorNotSupported && shortError != ErrorNotSupported, "address errors must not be EOPNOTSUPP");
    const int connected = socket_nid_postfix(Inet, Stream, 0);
    Require(connected >= 0 && connect_nid_postfix(connected, address.data(), address.size()) == 0, "connect to the listener failed");
    *__error_nid_postfix() = 0;
    Require(listen_nid_postfix(connected, 1) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "listen on a connected socket must fail with EINVAL (22)");
    Require(close_nid_postfix(connected) == 0, "close failed");
    Require(close_nid_postfix(plain) == 0, "close failed");
    std::array<std::uint8_t, 28> wrongFamily{28, 28};
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, wrongFamily.data(), wrongFamily.size()) == -1 && *__error_nid_postfix() == ErrorNotSupported,
        "connect on a listening socket with an address of another family must fail with EOPNOTSUPP (45)");

    std::array<std::uint8_t, 16> peer{};
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(listener, peer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept with an address buffer and no length pointer must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(-1, peer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept must check the length pointer before the descriptor, as FreeBSD accept1 does");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(-1, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "accept on an invalid descriptor without an address must fail with EBADF (9)");

    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page * 3, 0, 0, &physical) == 0, "allocating direct memory failed");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, Page * 3, 3, 0, physical, 0) == 0, "mapping direct memory failed");
    auto* readOnly = static_cast<std::uint8_t*>(mapping) + Page;
    auto* unmapped = readOnly + Page;
    auto* readOnlyLength = reinterpret_cast<std::uint32_t*>(readOnly);
    auto* unreadableLength = reinterpret_cast<std::uint32_t*>(unmapped);
    *readOnlyLength = peer.size();
    Require(sceKernelMprotect(readOnly, Page, 1) == 0 && sceKernelMprotect(unmapped, Page, 0) == 0, "mprotect failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(-1, peer.data(), unreadableLength) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept must read the address length before the descriptor and fail with EFAULT (14) when it cannot");

    const int faults = socket_nid_postfix(Inet, Stream, 0);
    std::array<std::uint8_t, 16> faultsAddress{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(faults >= 0 && bind_nid_postfix(faults, faultsAddress.data(), faultsAddress.size()) == 0, "bind failed");
    length = faultsAddress.size();
    Require(getsockname_nid_postfix(faults, faultsAddress.data(), &length) == 0 && listen_nid_postfix(faults, 4) == 0, "listen failed");
    const Timeval wait{5, 0};
    const auto pending = [&] {
        const int client = socket_nid_postfix(Inet, Stream, 0);
        Require(client >= 0 && connect_nid_postfix(client, faultsAddress.data(), faultsAddress.size()) == 0 &&
                setsockopt_nid_postfix(client, 0xffff, 0x1006, &wait, sizeof(wait)) == 0,
            "connect to the listener failed");
        return client;
    };
    const auto closedByAccept = [](int client) {
        char byte = 0;
        *__error_nid_postfix() = 0;
        const auto received = recv_nid_postfix(client, &byte, sizeof(byte), 0);
        Require(received == 0 || (received == -1 && *__error_nid_postfix() == ErrorConnectionReset),
            "a failed accept must close the connection it took off the queue");
        Require(close_nid_postfix(client) == 0, "close failed");
    };

    int pendingClient = pending();
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(faults, peer.data(), unreadableLength) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept with an unreadable address length must fail with EFAULT (14)");
    int taken = accept_nid_postfix(faults, nullptr, nullptr);
    Require(taken >= 0, "accept with an unreadable address length must leave the connection queued");
    const char byte = 'q';
    char received = 0;
    Require(send_nid_postfix(pendingClient, &byte, sizeof(byte), 0) == 1 && recv_nid_postfix(taken, &received, sizeof(received), 0) == 1 &&
            received == byte,
        "the queued connection must reach the next accept");
    Require(close_nid_postfix(taken) == 0 && close_nid_postfix(pendingClient) == 0, "close failed");

    pendingClient = pending();
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(faults, peer.data(), readOnlyLength) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept with a read-only address length must fail with EFAULT (14)");
    closedByAccept(pendingClient);

    for (auto* name : {readOnly + 64, unmapped}) {
        pendingClient = pending();
        std::uint32_t capacity = peer.size();
        *__error_nid_postfix() = 0;
        Require(accept_nid_postfix(faults, name, &capacity) == -1 && *__error_nid_postfix() == ErrorFault,
            "accept with an unwritable address buffer must fail with EFAULT (14)");
        closedByAccept(pendingClient);
    }

    for (const std::uint32_t requested : {0u, 8u}) {
        pendingClient = pending();
        std::array<std::uint8_t, 28> clamped{};
        clamped.fill(0xee);
        std::uint32_t capacity = requested;
        taken = accept_nid_postfix(faults, requested == 0 ? unmapped : clamped.data(), &capacity);
        Require(taken >= 0 && capacity == requested, "accept must clamp the reported address length to the capacity");
        Require(requested == 0 || (clamped[0] == 16 && clamped[1] == Inet && clamped[8] == 0xee), "accept must copy only the clamped peer address");
        Require(close_nid_postfix(taken) == 0 && close_nid_postfix(pendingClient) == 0, "close failed");
    }

    pendingClient = pending();
    std::array<std::uint8_t, 28> name{};
    std::uint32_t capacity = name.size();
    taken = accept_nid_postfix(faults, name.data(), &capacity);
    Require(taken >= 0 && capacity == 16 && name[0] == 16 && name[1] == Inet, "accept must report the 16-byte AF_INET peer address");
    Require(close_nid_postfix(taken) == 0 && close_nid_postfix(pendingClient) == 0 && close_nid_postfix(faults) == 0, "close failed");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), Page * 3) == 0, "munmap failed");
    Require(sceKernelReleaseDirectMemory(physical, Page * 3) == 0, "releasing direct memory failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0, "a refused connect must leave the listener working");
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");
    Require(close_nid_postfix(accepted) == 0 && close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0, "close failed");
    return 0;
}
