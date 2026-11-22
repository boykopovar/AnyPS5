#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_TESTS_FAKESOCKET_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_TESTS_FAKESOCKET_HPP

#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/include/Api.hpp"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>

namespace FakeSocketSupport {

inline constexpr int GuestSocketLevel = 0xffff;
inline constexpr int GuestSendTimeout = 0x1005;
inline constexpr int GuestReceiveTimeout = 0x1006;
inline constexpr int FakeDescriptor = 0x3f00;

inline constexpr int HostWouldBlockCode =
#ifdef _WIN32
    WSAEWOULDBLOCK;
#else
    EAGAIN;
#endif

inline constexpr int HostInterruptedCode =
#ifdef _WIN32
    WSAEINTR;
#else
    EINTR;
#endif

inline constexpr int HostBadDescriptorCode =
#ifdef _WIN32
    WSAEBADF;
#else
    EBADF;
#endif

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

inline void SetSocketTimeout(int fd, int option, int milliseconds) {
    const GuestTimeval time{milliseconds / 1000, static_cast<std::int64_t>(milliseconds % 1000) * 1000};
    if (setsockopt_nid_postfix(fd, GuestSocketLevel, option, &time, sizeof(time)) != 0) {
        std::fprintf(stderr, "failed: set the socket timeout\n");
        std::abort();
    }
}

struct FakeSocket : KernelSocketPoll::NativeOwner {
    std::shared_ptr<void> backing;
    std::uintptr_t native = 0;
    std::atomic<int> calls{0};
    std::atomic<int> sendCalls{0};
    std::atomic<int> failNext{0};
};

inline std::mutex& FakeMutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::shared_ptr<FakeSocket>& CurrentFake() {
    static std::shared_ptr<FakeSocket> fake;
    return fake;
}

inline bool FakeLookup(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native) {
    std::lock_guard lock(FakeMutex());
    const auto& fake = CurrentFake();
    if (descriptor != FakeDescriptor || !fake) return false;
    *owner = fake;
    *native = fake->native;
    return true;
}

inline int FakeTransfer(KernelSocketPoll::NativeOwner* owner, bool send, void* buffer, int length) {
    auto& fake = *static_cast<FakeSocket*>(owner);
    ++fake.calls;
    if (send) ++fake.sendCalls;
    if (const int error = fake.failNext.exchange(0)) {
#ifdef _WIN32
        WSASetLastError(error);
#else
        errno = error;
#endif
        return -1;
    }
#ifdef _WIN32
    return send ? ::send(static_cast<SOCKET>(fake.native), static_cast<const char*>(buffer), length, 0)
                : ::recv(static_cast<SOCKET>(fake.native), static_cast<char*>(buffer), length, 0);
#else
    return static_cast<int>(send ? ::send(static_cast<int>(fake.native), buffer, static_cast<std::size_t>(length), MSG_NOSIGNAL)
                                 : ::recv(static_cast<int>(fake.native), buffer, static_cast<std::size_t>(length), 0));
#endif
}

inline std::shared_ptr<FakeSocket> InstallFake(int backingFd) {
    auto fake = std::make_shared<FakeSocket>();
    if (!KernelLookupSocketNative_nid_no_patch(backingFd, &fake->backing, &fake->native)) {
        std::fprintf(stderr, "failed: look up the backing socket\n");
        std::abort();
    }
    fake->transfer = FakeTransfer;
    {
        std::lock_guard lock(FakeMutex());
        CurrentFake() = fake;
    }
    KernelSetSocketNativeLookup_nid_no_patch(FakeLookup);
    return fake;
}

inline bool OwnerProvidesTransfer(int descriptor) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    if (!KernelLookupSocketNative_nid_no_patch(descriptor, &owner, &native)) return false;
    const auto* socket = static_cast<const KernelSocketPoll::NativeOwner*>(owner.get());
    return socket && socket->transfer;
}

inline bool SendTimeoutIsUnreachable(int descriptor) {
#ifdef __APPLE__
    return OwnerProvidesTransfer(descriptor);
#else
    static_cast<void>(descriptor);
    return false;
#endif
}

inline void RemoveFake() {
    std::lock_guard lock(FakeMutex());
    CurrentFake().reset();
}

}

#endif
