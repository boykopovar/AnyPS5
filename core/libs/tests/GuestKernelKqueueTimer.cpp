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
    std::fprintf(stderr, "kqueue timer check failed at line %u\n", static_cast<unsigned>(location.line()));
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

    KernelEvent change{};
    change.ident = 1;
    change.filter = -7; // EVFILT_TIMER
    change.flags = 0x0001 | 0x0010; // EV_ADD | EV_ONESHOT
    change.fflags = 0x0004; // NOTE_USECONDS
    change.data = 10000;
    change.udata = 123;

    KernelEvent event{};
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    *__error_nid_postfix() = 0;
    change.fflags = 0x0002; // NOTE_MSECONDS
    change.data = 5;
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    Require(close_nid_postfix(kq) == 0);

    return 0;
}
