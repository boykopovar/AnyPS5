#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
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
constexpr int plain = 0x01;

int main() {
    void* condition = nullptr;
    Require(_Cnd_init_with_default_name_override_nid_postfix(&condition, "GameCond") == success);
    Require(condition != nullptr);
    Require(_Cnd_signal_nid_postfix(&condition) == success);
    Require(_Cnd_broadcast_nid_postfix(&condition) == success);

    void* mutex = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    bool ready = false;
    std::thread notifier([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == success);
        ready = true;
        Require(_Cnd_signal_nid_postfix(&condition) == success);
        Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    });
    while (!ready) Require(_Cnd_wait_nid_postfix(&condition, &mutex) == success);
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    notifier.join();
    _Mtx_destroy_nid_postfix(&mutex);

    _Cnd_destroy_nid_postfix(&condition);
    Require(condition == nullptr);
    _Cnd_destroy_nid_postfix(&condition);
    Require(condition == nullptr);

    void* unnamed = nullptr;
    Require(_Cnd_init_with_default_name_override_nid_postfix(&unnamed, nullptr) == success);
    Require(unnamed != nullptr);
    _Cnd_destroy_nid_postfix(&unnamed);
    Require(unnamed == nullptr);

    void* empty = nullptr;
    RequireThrows<std::invalid_argument>([] {
        _Cnd_init_with_default_name_override_nid_postfix(nullptr, "NullHandle");
    });
    RequireThrows<std::invalid_argument>([] { _Cnd_destroy_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _Cnd_signal_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([&] { _Cnd_signal_nid_postfix(&empty); });
    RequireThrows<std::invalid_argument>([] { _Cnd_broadcast_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([&] { _Cnd_broadcast_nid_postfix(&empty); });
    return 0;
}
