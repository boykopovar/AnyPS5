#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int success = 0;
constexpr int permission = 1;
constexpr int deadlock = 11;
constexpr int busy = 16;
constexpr int invalid = 22;
constexpr int timedOut = 60;
constexpr std::int64_t second = 1000000000;

static Xtime FromNow(std::int64_t nanoseconds) {
    const auto since = std::chrono::system_clock::now().time_since_epoch() + std::chrono::nanoseconds(nanoseconds);
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since);
    return {seconds.count(), std::chrono::duration_cast<std::chrono::nanoseconds>(since - seconds).count()};
}

static int Microseconds(const Xtime& target) {
    return _ZNSt8__sce_v228_Timespec64_diff_to_usecondsEPK8timespec_nid_postfix(&target);
}

static bool Near(int microseconds, std::int64_t expected) {
    return microseconds <= expected + 1 && microseconds > expected - 500000;
}

static void DiffToMicroseconds() {
    Require(Microseconds(FromNow(-second)) == 0);
    Require(Microseconds({0, 0}) == 0);
    Require(Microseconds({-5, -3 * second}) == 0);
    Require(Near(Microseconds(FromNow(2 * second)), 2000000));
    const Xtime now = FromNow(0);
    Require(Near(Microseconds({now.sec + 4, now.nsec - 2 * second}), 2000000));
    Require(Near(Microseconds({now.sec, now.nsec + 2 * second}), 2000000));
    Require(Near(Microseconds({now.sec + 2, now.nsec + 1}), 2000001));
    const auto wrapped = static_cast<std::uint32_t>(Microseconds(FromNow(3000 * second)));
    Require(wrapped <= 3000000001u && wrapped > 3000000000u - 500000u);
    RequireThrows<std::invalid_argument>([] { _ZNSt8__sce_v228_Timespec64_diff_to_usecondsEPK8timespec_nid_postfix(nullptr); });
}

static void ExclusiveOwnership() {
    void* smtx = nullptr;
    Require(_Smtx_init_nid_postfix(&smtx) == success && smtx != nullptr);
    Require(_Smtx_lock_exclusive_nid_postfix(&smtx) == success);
    Require(_Smtx_lock_exclusive_nid_postfix(&smtx) == deadlock);
    Require(_Smtx_try_lock_exclusive_nid_postfix(&smtx) == busy);
    Require(_Smtx_try_lock_shared_nid_postfix(&smtx) == busy);
    Require(scePthreadRwlockTrywrlock(reinterpret_cast<PthreadRwlock*>(&smtx)) == static_cast<int>(0x80020010));
    std::thread([&] {
        Require(_Smtx_try_lock_shared_nid_postfix(&smtx) == busy);
        Require(_Smtx_try_lock_exclusive_nid_postfix(&smtx) == busy);
        const Xtime soon = FromNow(second / 50);
        Require(_Smtx_timedlock_shared_nid_postfix(&smtx, &soon) == timedOut);
        Require(_Smtx_timedlock_exclusive_nid_postfix(&smtx, &soon) == timedOut);
    }).join();
    Require(_Smtx_unlock_exclusive_nid_postfix(&smtx) == success);
    Require(_Smtx_unlock_exclusive_nid_postfix(&smtx) == permission);
    const Xtime later = FromNow(second);
    Require(_Smtx_timedlock_exclusive_nid_postfix(&smtx, &later) == success);
    Require(_Smtx_unlock_exclusive_nid_postfix(&smtx) == success);
    Require(_Smtx_destroy_nid_postfix(&smtx) == success);
    Require(_Smtx_lock_exclusive_nid_postfix(&smtx) == invalid);
    Require(_Smtx_destroy_nid_postfix(&smtx) == invalid);
}

static void SharedOwnership() {
    void* smtx = nullptr;
    Require(_Smtx_init_with_name_nid_postfix(&smtx, "GameSmtx") == success && smtx != nullptr);
    Require(_Smtx_lock_shared_nid_postfix(&smtx) == success);
    Require(_Smtx_try_lock_shared_nid_postfix(&smtx) == success);
    std::thread([&] {
        Require(_Smtx_try_lock_shared_nid_postfix(&smtx) == success);
        Require(_Smtx_unlock_shared_nid_postfix(&smtx) == success);
        Require(_Smtx_try_lock_exclusive_nid_postfix(&smtx) == busy);
        const Xtime past = FromNow(-second);
        Require(_Smtx_timedlock_exclusive_nid_postfix(&smtx, &past) == timedOut);
        const Xtime later = FromNow(second);
        Require(_Smtx_timedlock_shared_nid_postfix(&smtx, &later) == success);
        Require(_Smtx_unlock_shared_nid_postfix(&smtx) == success);
    }).join();
    Require(_Smtx_unlock_shared_nid_postfix(&smtx) == success);
    Require(_Smtx_unlock_shared_nid_postfix(&smtx) == success);
    Require(_Smtx_unlock_shared_nid_postfix(&smtx) == permission);
    Require(_Smtx_destroy_nid_postfix(&smtx) == success);

    void* unnamed = nullptr;
    Require(_Smtx_init_with_name_nid_postfix(&unnamed, nullptr) == success && unnamed != nullptr);
    Require(_Smtx_destroy_nid_postfix(&unnamed) == success);

    void* lazy = nullptr;
    Require(_Smtx_lock_shared_nid_postfix(&lazy) == success && lazy != nullptr);
    Require(_Smtx_unlock_shared_nid_postfix(&lazy) == success);
    Require(_Smtx_destroy_nid_postfix(&lazy) == success);
}

struct RwlockLog {
    void** handle = nullptr;
    const void* attr = &second;
    const char* name = nullptr;
    unsigned int timeout = 0;
    int result = 0;
};

static RwlockLog rwlockLog;

static int APS5_VABI FakeRwlockInit(void** handle, void* const* attr, const char* name) {
    rwlockLog.handle = handle;
    rwlockLog.attr = attr;
    rwlockLog.name = name;
    return rwlockLog.result;
}

static int APS5_VABI FakeRwlock(void** handle) {
    rwlockLog.handle = handle;
    return rwlockLog.result;
}

static int APS5_VABI FakeTimedRwlock(void** handle, unsigned int timeout) {
    rwlockLog.handle = handle;
    rwlockLog.timeout = timeout;
    return rwlockLog.result;
}

static void ForwardedResults() {
    PthreadSyncForwards_nid_no_patch fakes{};
    fakes.rwlockInit = FakeRwlockInit;
    fakes.rwlockDestroy = FakeRwlock;
    fakes.rwlockRdlock = FakeRwlock;
    fakes.rwlockWrlock = FakeRwlock;
    fakes.rwlockTryrdlock = FakeRwlock;
    fakes.rwlockTrywrlock = FakeRwlock;
    fakes.rwlockTimedrdlock = FakeTimedRwlock;
    fakes.rwlockTimedwrlock = FakeTimedRwlock;
    fakes.rwlockUnlock = FakeRwlock;
    RegisterPthreadSyncForwards_nid_no_patch(&fakes);

    void* smtx = nullptr;
    Require(_Smtx_init_nid_postfix(&smtx) == success && rwlockLog.handle == &smtx && rwlockLog.attr == nullptr);
    Require(rwlockLog.name != nullptr && std::strcmp(rwlockLog.name, "SceLibc_V2Smtx") == 0);
    const char* name = "Named";
    Require(_Smtx_init_with_name_nid_postfix(&smtx, name) == success && rwlockLog.name == name);
    Require(_Smtx_init_with_name_nid_postfix(&smtx, "") == success && rwlockLog.name == nullptr);
    rwlockLog.name = name;
    Require(_Smtx_init_with_name_nid_postfix(&smtx, nullptr) == success && rwlockLog.name == nullptr);

    const Xtime past = FromNow(-second);
    rwlockLog.timeout = 1;
    Require(_Smtx_timedlock_exclusive_nid_postfix(&smtx, &past) == success && rwlockLog.timeout == 0);
    const Xtime later = FromNow(2 * second);
    Require(_Smtx_timedlock_shared_nid_postfix(&smtx, &later) == success && Near(static_cast<int>(rwlockLog.timeout), 2000000));

    using Call = int (APS5_VABI*)(void**);
    const Call calls[] = {_Smtx_init_nid_postfix, _Smtx_destroy_nid_postfix, _Smtx_lock_exclusive_nid_postfix,
        _Smtx_lock_shared_nid_postfix, _Smtx_try_lock_exclusive_nid_postfix, _Smtx_try_lock_shared_nid_postfix,
        _Smtx_unlock_exclusive_nid_postfix, _Smtx_unlock_shared_nid_postfix};
    for (const auto call : calls) {
        rwlockLog.result = static_cast<int>(0x80020023);
        Require(call(&smtx) == 0x23);
        rwlockLog.result = static_cast<int>(0x8002ffff);
        Require(call(&smtx) == 0xffff);
        rwlockLog.result = static_cast<int>(0x80550001);
        Require(call(&smtx) == 1);
        rwlockLog.result = -1;
        Require(call(&smtx) == 1);
        rwlockLog.result = 7;
        Require(call(&smtx) == 1);
    }
    rwlockLog.result = static_cast<int>(0x8002003c);
    Require(_Smtx_timedlock_exclusive_nid_postfix(&smtx, &later) == timedOut);
    Require(_Smtx_timedlock_shared_nid_postfix(&smtx, &later) == timedOut);

    RegisterPthreadSyncForwards_nid_no_patch(nullptr);
    RequireThrows<std::runtime_error>([&] { _Smtx_lock_exclusive_nid_postfix(&smtx); });
}

int main() {
    DiffToMicroseconds();
    ExclusiveOwnership();
    SharedOwnership();
    ForwardedResults();
}
