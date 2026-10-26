#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorNotSocket = 38;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long result, int error) { return result == -1 && *__error_nid_postfix() == error; }

void CallsOnAnOpenPipeAreNotSocketCalls() {
    int ends[2];
    Require(pipe_nid_postfix(ends) == 0, "pipe failed");
    std::array<std::uint8_t, 16> address{16, 2};
    std::uint32_t size = address.size();
    char buffer[4];
    int option = 0;
    std::uint32_t optionSize = sizeof(option);
    Require(Fails(bind_nid_postfix(ends[0], address.data(), address.size()), ErrorNotSocket), "bind on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(listen_nid_postfix(ends[0], 1), ErrorNotSocket), "listen on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(accept_nid_postfix(ends[0], nullptr, nullptr), ErrorNotSocket), "accept on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(connect_nid_postfix(ends[0], address.data(), address.size()), ErrorNotSocket), "connect on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(shutdown_nid_postfix(ends[0], 1), ErrorNotSocket), "shutdown on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(getsockname_nid_postfix(ends[0], address.data(), &size), ErrorNotSocket), "getsockname on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(setsockopt_nid_postfix(ends[0], 0xffff, 4, &option, sizeof(option)), ErrorNotSocket), "setsockopt on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(getsockopt_nid_postfix(ends[0], 0xffff, 4, &option, &optionSize), ErrorNotSocket), "getsockopt on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(send_nid_postfix(ends[1], "x", 1, 0), ErrorNotSocket), "send on a pipe must fail with ENOTSOCK (38)");
    Require(Fails(recv_nid_postfix(ends[0], buffer, sizeof(buffer), 0), ErrorNotSocket), "recv on a pipe must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0, "close failed");
}

void ClosedAndInvalidDescriptorsAreBad() {
    int ends[2];
    Require(pipe_nid_postfix(ends) == 0, "pipe failed");
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0, "close failed");
    std::array<std::uint8_t, 16> address{16, 2};
    Require(Fails(bind_nid_postfix(ends[0], address.data(), address.size()), ErrorBadDescriptor), "bind on a closed descriptor must fail with EBADF (9)");
    Require(Fails(listen_nid_postfix(-1, 1), ErrorBadDescriptor), "listen on -1 must fail with EBADF (9)");
    Require(Fails(connect_nid_postfix(1 << 20, address.data(), address.size()), ErrorBadDescriptor), "connect on a descriptor that was never open must fail with EBADF (9)");
}

}

int main() {
    CallsOnAnOpenPipeAreNotSocketCalls();
    ClosedAndInvalidDescriptorsAreBad();
    return 0;
}
