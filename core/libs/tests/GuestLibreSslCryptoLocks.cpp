#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TAction>
static void RequireThrows(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    std::abort();
}

static void APS5_VABI Locking(int, int, const char*, int) {}
static int APS5_VABI AddLock(int*, int, int, const char*, int) { return 0; }
static void APS5_VABI ThreadId(CryptoThreadId*) {}
static void APS5_VABI OtherThreadId(CryptoThreadId*) {}

static void CheckThreadIdCallback() {
    Require(CRYPTO_THREADID_set_callback_nid_postfix(nullptr) == 1);
    Require(CRYPTO_THREADID_set_callback_nid_postfix(nullptr) == 1);
    Require(CRYPTO_THREADID_set_callback_nid_postfix(reinterpret_cast<void*>(&ThreadId)) == 1);
    Require(CRYPTO_THREADID_set_callback_nid_postfix(reinterpret_cast<void*>(&OtherThreadId)) == 0);
    Require(CRYPTO_THREADID_set_callback_nid_postfix(reinterpret_cast<void*>(&ThreadId)) == 0);
    Require(CRYPTO_THREADID_set_callback_nid_postfix(nullptr) == 0);
}

static void CheckThreadIdNumeric() {
    CryptoThreadId id{};
    std::memset(&id, 0xaa, sizeof(id));
    CRYPTO_THREADID_set_numeric_nid_postfix(&id, 0x1234567890abcdefull);
    Require(id.ptr == nullptr && id.val == 0x1234567890abcdefull);
    std::memset(&id, 0x55, sizeof(id));
    CRYPTO_THREADID_set_numeric_nid_postfix(&id, 0);
    unsigned char zero[sizeof(id)]{};
    Require(std::memcmp(&id, zero, sizeof(id)) == 0);
    RequireThrows([] { CRYPTO_THREADID_set_numeric_nid_postfix(nullptr, 1); });
}

int main() {
    Require(CRYPTO_num_locks_nid_postfix() == 41);
    CRYPTO_set_locking_callback_nid_postfix(reinterpret_cast<void*>(&Locking));
    CRYPTO_set_locking_callback_nid_postfix(nullptr);
    CRYPTO_set_add_lock_callback_nid_postfix(reinterpret_cast<void*>(&AddLock));
    CRYPTO_set_add_lock_callback_nid_postfix(nullptr);
    CheckThreadIdCallback();
    CheckThreadIdNumeric();
    return 0;
}
