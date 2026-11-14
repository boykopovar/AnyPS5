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
constexpr int busy = 3;
constexpr int plain = 0x01;
constexpr int recursive = 0x100;

int main() {
    void* mutex = nullptr;
    Require(_Mtx_init_with_name_nid_postfix(&mutex, plain, "GameMutex") == success);
    Require(mutex != nullptr);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == busy);
    Require(_Mtx_trylock_nid_postfix(&mutex) == busy);
    RequireThrows<std::logic_error>([&] { _Mtx_destroy_nid_postfix(&mutex); });
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    _Mtx_destroy_nid_postfix(&mutex);
    Require(mutex == nullptr);

    _Mtx_destroy_nid_postfix(&mutex);
    Require(mutex == nullptr);

    void* unnamed = nullptr;
    Require(_Mtx_init_with_name_nid_postfix(&unnamed, plain, nullptr) == success);
    Require(unnamed != nullptr);
    Require(_Mtx_trylock_nid_postfix(&unnamed) == success);
    Require(_Mtx_unlock_nid_postfix(&unnamed) == success);
    _Mtx_destroy_nid_postfix(&unnamed);
    Require(unnamed == nullptr);

    void* overridden = nullptr;
    Require(_Mtx_init_with_default_name_override_nid_postfix(&overridden, plain, "OverrideMutex") == success);
    Require(overridden != nullptr);
    _Mtx_destroy_nid_postfix(&overridden);
    Require(overridden == nullptr);

    void* recMutex = nullptr;
    Require(_Mtx_init_with_name_nid_postfix(&recMutex, plain | recursive, "RecursiveMutex") == success);
    Require(recMutex != nullptr);
    Require(_Mtx_lock_nid_postfix(&recMutex) == success);
    Require(_Mtx_lock_nid_postfix(&recMutex) == success);
    Require(_Mtx_trylock_nid_postfix(&recMutex) == success);

    int contenderFirstTry = -1;
    std::thread contender([&] {
        contenderFirstTry = _Mtx_trylock_nid_postfix(&recMutex);
        Require(_Mtx_lock_nid_postfix(&recMutex) == success);
        Require(_Mtx_unlock_nid_postfix(&recMutex) == success);
    });
    while (contenderFirstTry == -1) std::this_thread::yield();
    Require(contenderFirstTry == busy);

    Require(_Mtx_unlock_nid_postfix(&recMutex) == success);
    Require(_Mtx_unlock_nid_postfix(&recMutex) == success);
    Require(_Mtx_unlock_nid_postfix(&recMutex) == success);
    contender.join();
    _Mtx_destroy_nid_postfix(&recMutex);
    Require(recMutex == nullptr);

    RequireThrows<std::invalid_argument>([] {
        _Mtx_init_with_name_nid_postfix(nullptr, plain, "NullHandle");
    });
    RequireThrows<std::invalid_argument>([] {
        _Mtx_destroy_nid_postfix(nullptr);
    });
    return 0;
}
