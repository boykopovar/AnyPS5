#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static constexpr int GuestEinval = 22;
static constexpr int CancelEnable = 0;
static constexpr int CancelDisable = 1;

static void* APS5_VABI Entry(void* argument) {
    return argument;
}

int main() {
    PthreadAttr attr{};
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    for (const int state : {1, 0}) {
        int read = -1;
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, state) == 0);
        Require(pthread_attr_getdetachstate_nid_postfix(&attr, &read) == 0 && read == state);
    }
    for (const int invalid : {2, -1, 100}) {
        int read = -1;
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, invalid) == GuestEinval);
        Require(pthread_attr_getdetachstate_nid_postfix(&attr, &read) == 0 && read == 0);
    }
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, nullptr) == GuestEinval);

    for (const std::size_t size : {std::size_t{0}, std::size_t{4096}, std::size_t{65536}}) {
        std::size_t read = 1;
        Require(pthread_attr_setguardsize_nid_postfix(&attr, size) == 0);
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &read) == 0 && read == size);
    }
    Require(pthread_attr_getguardsize_nid_postfix(&attr, nullptr) == GuestEinval);

    {
        std::size_t read = 0;
        Require(pthread_attr_setstacksize_nid_postfix(&attr, std::size_t{1} << 20) == 0);
        Require(pthread_attr_getstacksize_nid_postfix(&attr, &read) == 0 && read == (std::size_t{1} << 20));
        Require(pthread_attr_getstacksize_nid_postfix(&attr, nullptr) == GuestEinval);
    }

    for (const int priority : {256, 500, 767}) {
        KernelSchedParam written{};
        KernelSchedParam read{};
        written.sched_priority = priority;
        Require(pthread_attr_setschedparam_nid_postfix(&attr, &written) == 0);
        Require(pthread_attr_getschedparam_nid_postfix(&attr, &read) == 0 && read.sched_priority == priority);
    }
    Require(pthread_attr_getschedparam_nid_postfix(&attr, nullptr) == GuestEinval);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    int afterDestroy = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &afterDestroy) == GuestEinval);
    PthreadAttr unset = nullptr;
    std::size_t guard = 7;
    std::size_t stack = 13;
    KernelSchedParam sched{.sched_priority = 13};
    afterDestroy = -1;
    for (PthreadAttr* invalid : {static_cast<PthreadAttr*>(nullptr), &unset, &attr}) {
        Require(pthread_attr_getdetachstate_nid_postfix(invalid, &afterDestroy) == GuestEinval && afterDestroy == -1);
        Require(pthread_attr_setdetachstate_nid_postfix(invalid, 0) == GuestEinval);
        Require(pthread_attr_getguardsize_nid_postfix(invalid, &guard) == GuestEinval && guard == 7);
        Require(pthread_attr_setguardsize_nid_postfix(invalid, 4096) == GuestEinval);
        Require(pthread_attr_getstacksize_nid_postfix(invalid, &stack) == GuestEinval && stack == 13);
        Require(pthread_attr_setstacksize_nid_postfix(invalid, std::size_t{1} << 20) == GuestEinval);
        Require(pthread_attr_getschedparam_nid_postfix(invalid, &sched) == GuestEinval && sched.sched_priority == 13);
        Require(pthread_attr_setschedparam_nid_postfix(invalid, &sched) == GuestEinval);
        Require(pthread_attr_destroy_nid_postfix(invalid) == GuestEinval);
    }
    Require(pthread_attr_init_nid_postfix(nullptr) == GuestEinval);
    Require(pthread_attr_destroy_nid_postfix(nullptr) == GuestEinval);

    const Pthread self = pthread_self_nid_postfix();
    Require(self != nullptr);
    Require(pthread_equal_nid_postfix(self, self) != 0);

    int marker = 0;
    Pthread created = nullptr;
    Require(pthread_create_name_np_nid_postfix(&created, nullptr, Entry, &marker, "aps5-named") == 0);
    Require(created != nullptr);
    Require(pthread_equal_nid_postfix(created, self) == 0);
    Require(pthread_equal_nid_postfix(created, created) != 0);
    void* returned = nullptr;
    Require(pthread_join_nid_postfix(created, &returned) == 0);
    Require(returned == &marker);

    Require(sched_yield_nid_postfix() == 0);
    pthread_yield_nid_postfix();

    int old = -1;
    Require(pthread_setcancelstate_nid_postfix(CancelDisable, &old) == 0 && old == CancelEnable);
    Require(pthread_setcancelstate_nid_postfix(CancelEnable, &old) == 0 && old == CancelDisable);
    Require(pthread_setcancelstate_nid_postfix(CancelEnable, &old) == 0 && old == CancelEnable);
    for (const int invalid : {2, -1}) Require(pthread_setcancelstate_nid_postfix(invalid, &old) == GuestEinval);
    Require(pthread_setcancelstate_nid_postfix(CancelDisable, &old) == 0 && old == CancelEnable);
    Require(pthread_setcancelstate_nid_postfix(3, nullptr) == GuestEinval);
    Require(pthread_setcancelstate_nid_postfix(CancelEnable, &old) == 0 && old == CancelDisable);
    return 0;
}
