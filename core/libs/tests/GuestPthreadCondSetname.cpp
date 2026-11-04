#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <stdexcept>

static constexpr int GuestEinval = 22;

static void Require(bool value) { if (!value) std::abort(); }

template <class TCall>
static bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "first") == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "second") == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(ThrowsInvalidArgument([&] { return pthread_cond_setname_np_nid_postfix(&cond, nullptr); }));
    Require(ThrowsInvalidArgument([] { return pthread_cond_setname_np_nid_postfix(nullptr, "cond"); }));
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "destroyed") == GuestEinval);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "reinitialized") == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);

    PthreadCond lazy = nullptr;
    Require(pthread_cond_setname_np_nid_postfix(&lazy, "lazy") == 0 && lazy != nullptr);
    Require(pthread_cond_signal_nid_postfix(&lazy) == 0);
    Require(pthread_cond_destroy_nid_postfix(&lazy) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&lazy, "lazy") == GuestEinval);
}
