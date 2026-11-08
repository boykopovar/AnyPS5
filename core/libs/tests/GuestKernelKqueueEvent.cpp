#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "kqueue flags check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

struct KernelEvent {
    std::uint64_t ident;
    std::int16_t filter;
    std::uint16_t flags;
    std::uint32_t fflags;
    std::int64_t data;
    std::uint64_t udata;
    std::uint64_t ext[4];
};
} // namespace

int main() {
    int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    int pipefd[2];
    Require(pipe_nid_postfix(pipefd) == 0);

    KernelEvent change{};
    change.ident = static_cast<std::uint64_t>(pipefd[0]);
    change.filter = -1; // EVFILT_READ
    change.flags = 0x0001 | 0x0004 | 0x0020; // EV_ADD | EV_ENABLE | EV_CLEAR
    change.fflags = 0;
    change.data = 0;
    change.udata = 42;

    KernelEvent event{};
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, reinterpret_cast<KernelEvent*>(0x10), 1, nullptr, 0, nullptr) == -1);
    Require(*__error_nid_postfix() == 14); // EFAULT

    Require(close_nid_postfix(kq) == 0);
    Require(close_nid_postfix(pipefd[0]) == 0);
    Require(close_nid_postfix(pipefd[1]) == 0);

    return 0;
}
