#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <string>

static constexpr int GuestEagain = 35;
static constexpr int GuestEinval = 22;
static constexpr int GuestEnametoolong = 63;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "first") == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "second") == GuestEagain);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    PthreadCond fresh = nullptr;
    Require(pthread_cond_init_nid_postfix(&fresh, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&fresh, nullptr) == GuestEinval);
    Require(pthread_cond_setname_np_nid_postfix(&fresh, std::string(32, 'a').c_str()) == GuestEnametoolong);
    Require(pthread_cond_setname_np_nid_postfix(&fresh, std::string(1024, 'a').c_str()) == GuestEnametoolong);
    Require(pthread_cond_setname_np_nid_postfix(&fresh, std::string(31, 'a').c_str()) == 0);
    Require(pthread_cond_destroy_nid_postfix(&fresh) == 0);
    Require(pthread_cond_setname_np_nid_postfix(nullptr, "cond") == GuestEinval);
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
