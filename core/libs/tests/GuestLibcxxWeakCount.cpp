#include "prx/libc/include/LibcxxWeakCount.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <thread>

namespace {

struct GuestControl {
    virtual ~GuestControl() = default;
    virtual void APS5_VABI OnZeroShared() { wrongSlot = true; }
    virtual const void* APS5_VABI GetDeleter(const void*) { wrongSlot = true; return nullptr; }
    virtual void APS5_VABI OnZeroSharedWeak() { ++cleanups; }

    std::uint64_t shared = 0;
    std::int64_t weak = 0;
    std::atomic<unsigned> cleanups{0};
    std::atomic<bool> wrongSlot{false};
};

LibcxxWeakCount* Guest(GuestControl& control) {
    return reinterpret_cast<LibcxxWeakCount*>(&control);
}

constexpr auto Lock = _ZNSt3__119__shared_weak_count4lockEv_nid_postfix;
constexpr auto Release = _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix;

bool CountsAndCallback() {
    GuestControl control;
    control.weak = 1;
    if (Lock(Guest(control)) != Guest(control) || control.shared != 1 || control.weak != 1) return false;
    constexpr auto wideCount = UINT64_C(1) << 32;
    control.shared = wideCount;
    if (Lock(Guest(control)) != Guest(control) || control.shared != wideCount + 1) return false;
    control.shared = UINT64_MAX;
    if (Lock(Guest(control)) || control.shared != UINT64_MAX) return false;
    Release(Guest(control));
    if (control.weak != 0 || control.cleanups != 0) return false;
    Release(Guest(control));
    return control.cleanups == 1 && !control.wrongSlot && control.weak == -1 && control.shared == UINT64_MAX;
}

template <std::size_t OwnerCount>
bool ConcurrentOwners() {
    GuestControl live;
    GuestControl expired;
    std::array<std::thread, OwnerCount> workers;
    expired.shared = UINT64_MAX;
    expired.weak = workers.size() - 1;
    std::atomic<unsigned> acquired{0};
    std::barrier releaseTogether(OwnerCount);
    for (auto& worker : workers) {
        worker = std::thread([&] {
            if (Lock(Guest(live)) == Guest(live)) ++acquired;
            releaseTogether.arrive_and_wait();
            Release(Guest(expired));
        });
    }
    for (auto& worker : workers) worker.join();
    return acquired == workers.size() && live.shared == workers.size() && expired.weak == -1
        && expired.cleanups == 1 && !expired.wrongSlot;
}

}

int main() {
    if (!CountsAndCallback()) {
        std::fputs("weak reference count or cleanup slot mismatch\n", stderr);
        return 1;
    }
    if (!ConcurrentOwners<2>() || !ConcurrentOwners<8>()) {
        std::fputs("concurrent promotions or final release mismatch\n", stderr);
        return 1;
    }
    return 0;
}
