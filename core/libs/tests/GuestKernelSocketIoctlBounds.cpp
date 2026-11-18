#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void AssertErrnoInRange() {
    const int err = *__error_nid_postfix();
    Require(err >= 1 && err <= 96);
}

class GuardedPage {
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (data == nullptr) {
            std::abort();
        }
        DWORD previous = 0;
        if (VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) == 0) {
            std::abort();
        }
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            std::abort();
        }
        size = static_cast<std::size_t>(pageSize);
        data = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) {
            std::abort();
        }
        if (mprotect(data + size, size, PROT_NONE) != 0) {
            std::abort();
        }
#endif
    }

    void SetFirstPageReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        if (VirtualProtect(data, size, protect, &previous) == 0) {
            std::abort();
        }
#else
        const int protect = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        if (mprotect(data, size, protect) != 0) {
            std::abort();
        }
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
};

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotTerminal = 25;
constexpr int ErrorAddrNotAvailable = 49;

constexpr std::uint64_t NonblockingRequest = 0x8004667e;
constexpr std::uint64_t ReadableRequest = 0x4004667f;
constexpr std::uint64_t CloseOnExecRequest = 0x20006601;
constexpr std::uint64_t ClearCloseOnExecRequest = 0x20006602;

constexpr std::uint64_t MalformedRequestZero = 0;
constexpr std::uint64_t MalformedRequestZeroLenIn = 0x80006601;
constexpr std::uint64_t MalformedRequestZeroLenOut = 0x40006601;
constexpr std::uint64_t ForeignGroupRequest = 0x80047801;
constexpr std::uint64_t SupportedGroupUnknownRequest = 0x80046605;

std::array<std::uint8_t, 16> LoopbackAddress() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    return address;
}

std::array<std::uint8_t, 16> CreateBoundListener(int* listener) {
    *listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(*listener >= 0);
    auto address = LoopbackAddress();
    Require(bind_nid_postfix(*listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(*listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(*listener, address.data(), &size) == 0);
    return address;
}

void TestDescriptorBounds() {
    int on = 1;
    int queued = -1;

    for (const int fd : {-1, -42, -1000, 1000000, 0x7fffffff}) {
        for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
            Require(call(fd, NonblockingRequest, &on) == -1);
            Require(*__error_nid_postfix() == ErrorBadDescriptor);
            AssertErrnoInRange();

            Require(call(fd, ReadableRequest, &queued) == -1);
            Require(*__error_nid_postfix() == ErrorBadDescriptor);
            AssertErrnoInRange();

            Require(call(fd, CloseOnExecRequest, nullptr) == -1);
            Require(*__error_nid_postfix() == ErrorBadDescriptor);
            AssertErrnoInRange();

            Require(call(fd, ClearCloseOnExecRequest, nullptr) == -1);
            Require(*__error_nid_postfix() == ErrorBadDescriptor);
            AssertErrnoInRange();
        }
    }

    const int sock4 = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock4 >= 0);
    Require(close_nid_postfix(sock4) == 0);
    Require(ioctl_nid_postfix(sock4, NonblockingRequest, &on) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();
    Require(_ioctl_nid_postfix(sock4, ReadableRequest, &queued) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();

    const int sock6 = socket_nid_postfix(AfInet6, SockStream, 0);
    Require(sock6 >= 0);
    Require(close_nid_postfix(sock6) == 0);
    Require(ioctl_nid_postfix(sock6, NonblockingRequest, &on) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();
    Require(_ioctl_nid_postfix(sock6, ReadableRequest, &queued) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(ioctl_nid_postfix(pair[0], NonblockingRequest, &on) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
    Require(ioctl_nid_postfix(pipeFds[0], NonblockingRequest, &on) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();
    Require(ioctl_nid_postfix(pipeFds[1], ReadableRequest, &queued) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
    AssertErrnoInRange();
}

void TestNullAndFaultPointers() {
    const int sock4 = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock4 >= 0);
    const int dgram4 = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgram4 >= 0);
    const int sock6 = socket_nid_postfix(AfInet6, SockStream, 0);
    Require(sock6 >= 0);

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int descriptors[] = {sock4, dgram4, sock6, pair[0], pair[1], pipeFds[0], pipeFds[1]};

    for (const int fd : descriptors) {
        for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
            Require(call(fd, NonblockingRequest, nullptr) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            Require(call(fd, ReadableRequest, nullptr) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            void* const lowFault = reinterpret_cast<void*>(std::uintptr_t{0x10});
            Require(call(fd, NonblockingRequest, lowFault) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            Require(call(fd, ReadableRequest, lowFault) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();
        }
    }

    GuardedPage guard;
    void* const faultAddress = guard.data + guard.size;
    void* const boundaryAddress = guard.data + guard.size - 2;

    for (const int fd : descriptors) {
        for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
            Require(call(fd, NonblockingRequest, faultAddress) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            Require(call(fd, ReadableRequest, faultAddress) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            Require(call(fd, NonblockingRequest, boundaryAddress) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();

            Require(call(fd, ReadableRequest, boundaryAddress) == -1);
            Require(*__error_nid_postfix() == ErrorFault);
            AssertErrnoInRange();
        }
    }

    int* validBuffer = reinterpret_cast<int*>(guard.data);
    *validBuffer = 1;

    guard.SetFirstPageReadOnly(true);

    Require(ioctl_nid_postfix(sock4, ReadableRequest, guard.data) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    Require(_ioctl_nid_postfix(sock4, ReadableRequest, guard.data) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    Require(ioctl_nid_postfix(sock4, NonblockingRequest, guard.data) == 0);
    Require(_ioctl_nid_postfix(sock4, NonblockingRequest, guard.data) == 0);

    guard.SetFirstPageReadOnly(false);

    Require(ioctl_nid_postfix(sock4, ReadableRequest, guard.data) == 0);
    Require(*validBuffer == 0);
    Require(_ioctl_nid_postfix(sock4, ReadableRequest, guard.data) == 0);
    Require(*validBuffer == 0);

    Require(close_nid_postfix(sock4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(sock6) == 0);
    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestRequestBounds() {
    const int sock4 = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock4 >= 0);
    const int sock6 = socket_nid_postfix(AfInet6, SockStream, 0);
    Require(sock6 >= 0);

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    int dummy = 0;

    for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
        Require(call(sock4, MalformedRequestZero, &dummy) == -1);
        const int err0 = *__error_nid_postfix();
        Require(err0 == ErrorNotTerminal || err0 == ErrorInvalid);
        AssertErrnoInRange();

        Require(call(sock4, MalformedRequestZeroLenIn, &dummy) == -1);
        const int err1 = *__error_nid_postfix();
        Require(err1 == ErrorNotTerminal || err1 == ErrorInvalid);
        AssertErrnoInRange();

        Require(call(sock4, MalformedRequestZeroLenOut, &dummy) == -1);
        const int err2 = *__error_nid_postfix();
        Require(err2 == ErrorNotTerminal || err2 == ErrorInvalid);
        AssertErrnoInRange();

        Require(call(sock4, ForeignGroupRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorAddrNotAvailable);
        AssertErrnoInRange();

        Require(call(sock6, ForeignGroupRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorAddrNotAvailable);
        AssertErrnoInRange();

        Require(call(pair[0], ForeignGroupRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorNotTerminal);
        AssertErrnoInRange();

        Require(call(sock4, SupportedGroupUnknownRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorNotTerminal);
        AssertErrnoInRange();

        Require(call(sock6, SupportedGroupUnknownRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorNotTerminal);
        AssertErrnoInRange();

        Require(call(pair[0], SupportedGroupUnknownRequest, &dummy) == -1);
        Require(*__error_nid_postfix() == ErrorNotTerminal);
        AssertErrnoInRange();
    }

    Require(ioctl_nid_postfix(sock4, CloseOnExecRequest, nullptr) == 0);
    Require(_ioctl_nid_postfix(sock4, ClearCloseOnExecRequest, nullptr) == 0);
    Require(ioctl_nid_postfix(sock6, CloseOnExecRequest, nullptr) == 0);
    Require(_ioctl_nid_postfix(sock6, ClearCloseOnExecRequest, nullptr) == 0);
    Require(ioctl_nid_postfix(pair[0], CloseOnExecRequest, nullptr) == 0);
    Require(_ioctl_nid_postfix(pair[0], ClearCloseOnExecRequest, nullptr) == 0);
    Require(ioctl_nid_postfix(pipeFds[0], CloseOnExecRequest, nullptr) == 0);
    Require(_ioctl_nid_postfix(pipeFds[0], ClearCloseOnExecRequest, nullptr) == 0);

    Require(close_nid_postfix(sock4) == 0);
    Require(close_nid_postfix(sock6) == 0);
    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestFionbioModes() {
    const int sock4 = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock4 >= 0);
    const int dgram4 = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgram4 >= 0);
    const int sock6 = socket_nid_postfix(AfInet6, SockStream, 0);
    Require(sock6 >= 0);
    const int dgram6 = socket_nid_postfix(AfInet6, SockDgram, 0);
    Require(dgram6 >= 0);

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int descriptors[] = {sock4, dgram4, sock6, dgram6, pair[0], pair[1], pipeFds[0], pipeFds[1]};

    for (const int fd : descriptors) {
        for (const int mode : {1, 0, 42, -1, 0x7fffffff}) {
            int value = mode;
            Require(ioctl_nid_postfix(fd, NonblockingRequest, &value) == 0);
            value = mode;
            Require(_ioctl_nid_postfix(fd, NonblockingRequest, &value) == 0);
        }
    }

    Require(close_nid_postfix(sock4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(sock6) == 0);
    Require(close_nid_postfix(dgram6) == 0);
    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestFionreadDataTracking() {
    int listener = -1;
    const auto address = CreateBoundListener(&listener);

    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(client >= 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    int available = -1;
    Require(ioctl_nid_postfix(client, ReadableRequest, &available) == 0);
    Require(available == 0);
    Require(ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == 0);

    std::array<char, 32> sendBuffer{};
    std::memset(sendBuffer.data(), 'K', sendBuffer.size());
    Require(send_nid_postfix(client, sendBuffer.data(), sendBuffer.size(), 0) == static_cast<std::int64_t>(sendBuffer.size()));

    available = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == static_cast<int>(sendBuffer.size()));

    std::array<char, 32> recvBuffer{};
    Require(recv_nid_postfix(server, recvBuffer.data(), 12, 0) == 12);

    available = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == 20);

    Require(recv_nid_postfix(server, recvBuffer.data() + 12, 20, 0) == 20);

    available = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(listener) == 0);

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    available = -1;
    Require(ioctl_nid_postfix(pair[0], ReadableRequest, &available) == 0);
    Require(available == 0);
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(send_nid_postfix(pair[0], sendBuffer.data(), 16, 0) == 16);

    available = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &available) == 0);
    Require(available == 16);

    Require(recv_nid_postfix(pair[1], recvBuffer.data(), 16, 0) == 16);

    available = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    available = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(write_nid_postfix(pipeFds[1], sendBuffer.data(), 24) == 24);

    available = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &available) == 0);
    Require(available == 24);

    Require(read_nid_postfix(pipeFds[0], recvBuffer.data(), 24) == 24);

    available = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestShutdownReceiveInteraction() {
    int listener = -1;
    const auto address = CreateBoundListener(&listener);

    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(client >= 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    Require(shutdown_nid_postfix(server, 0) == 0);

    int available = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == 0);

    available = -1;
    Require(_ioctl_nid_postfix(server, ReadableRequest, &available) == 0);
    Require(available == 0);

    Require(ioctl_nid_postfix(server, ReadableRequest, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    Require(_ioctl_nid_postfix(server, ReadableRequest, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    GuardedPage guard;
    Require(ioctl_nid_postfix(server, ReadableRequest, guard.data + guard.size) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    Require(_ioctl_nid_postfix(server, ReadableRequest, guard.data + guard.size) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
    AssertErrnoInRange();

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(listener) == 0);
}

} // namespace

int main() {
    TestDescriptorBounds();
    TestNullAndFaultPointers();
    TestRequestBounds();
    TestFionbioModes();
    TestFionreadDataTracking();
    TestShutdownReceiveInteraction();
    return 0;
}
