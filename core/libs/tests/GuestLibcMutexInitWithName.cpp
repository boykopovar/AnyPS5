#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

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

int main() {
    void* mutex = nullptr;
    Require(_Mtx_init_with_name_nid_postfix(&mutex, plain, "GameMutex") == success);
    Require(mutex != nullptr);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == busy);
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    _Mtx_destroy_nid_postfix(&mutex);

    Require(mutex == nullptr);

    RequireThrows<std::invalid_argument>([] {
        _Mtx_init_with_name_nid_postfix(nullptr, plain, "NullHandle");

    });
}
