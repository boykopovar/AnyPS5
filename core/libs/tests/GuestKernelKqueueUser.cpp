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
    std::fprintf(stderr, "kqueue user check failed at line %u\n", static_cast<unsigned>(location.line()));
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
    change.filter = -11; // EVFILT_USER
    change.flags = 0x0001 | 0x0020; // EV_ADD | EV_CLEAR
    change.fflags = 0;
    change.data = 0;
    change.udata = 99;

    KernelEvent event{};
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    change.flags = 0;
    change.fflags = 0x01000000 | 0xc0000000 | 0x123; // NOTE_TRIGGER | NOTE_FFCOPY | 0x123
    change.data = 456;
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    Require(kevent_nid_postfix(kq, nullptr, 0, &event, 1, nullptr) == 1);
    Require(event.ident == 1 && event.filter == -11 && event.fflags == 0x123 && event.data == 456);

    Require(close_nid_postfix(kq) == 0);

    return 0;
}
