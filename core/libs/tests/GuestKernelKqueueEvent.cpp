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

static constexpr std::int16_t EvfiltRead = -1;
static constexpr std::uint16_t EvAdd = 0x0001;
static constexpr std::uint16_t EvEnable = 0x0004;
static constexpr std::uint16_t EvClear = 0x0020;
static constexpr int GuestEfault = 14;
} // namespace

int main() {
    int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    int pipefd[2];
    Require(pipe_nid_postfix(pipefd) == 0);

    KernelEvent change{};
    change.ident = static_cast<std::uint64_t>(pipefd[0]);
    change.filter = EvfiltRead;
    change.flags = EvAdd | EvEnable | EvClear;
    change.fflags = 0;
    change.data = 0;
    change.udata = reinterpret_cast<void*>(static_cast<std::uintptr_t>(42));

    KernelEvent event{};
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, reinterpret_cast<KernelEvent*>(0x10), 1, nullptr, 0, nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    Require(close_nid_postfix(kq) == 0);
    Require(close_nid_postfix(pipefd[0]) == 0);
    Require(close_nid_postfix(pipefd[1]) == 0);

    return 0;
}
