#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr int ClockMonotonic = 4;
constexpr int Sigusr1 = 30;
constexpr int SigBlock = 1;
constexpr int SigevSignal = 1;
constexpr std::int64_t Millisecond = 1000000;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what);
    std::exit(1);
}

GuestSignalSet Usr1() {
    GuestSignalSet set{{0, 0, 0, 0}};
    set.bits[(Sigusr1 - 1) / 32] = 1u << ((Sigusr1 - 1) % 32);
    return set;
}

void TakeOne() {
    const GuestSignalSet set = Usr1();
    const KernelTimespec wait{5, 0};
    unsigned char info[80]{};
    Require(sigtimedwait_nid_postfix(&set, info, &wait) == Sigusr1, "sigtimedwait");
}

}

int main(int argc, char** argv) {
    const bool deleted = argc > 1 && std::strcmp(argv[1], "deleted") == 0;
    Require(argc > 1 && (deleted || std::strcmp(argv[1], "armed") == 0), "mode is armed or deleted");
    const GuestSignalSet set = Usr1();
    Require(sigprocmask_nid_postfix(SigBlock, &set, nullptr) == 0, "sigprocmask");
    const GuestSigevent event{SigevSignal, Sigusr1, 0, {}};
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &event, &id) == 0, "ktimer_create");
    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0, "ktimer_settime");
    TakeOne();
    TakeOne();
    if (deleted) {
        Require(ktimer_delete_nid_postfix(id) == 0, "ktimer_delete");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::printf("guest_kernel_sys_ktimer_exit %s returning\n", argv[1]);
    std::fflush(stdout);
    return 0;
}
