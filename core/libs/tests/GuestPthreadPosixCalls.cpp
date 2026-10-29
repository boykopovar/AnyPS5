#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static constexpr int GuestEinval = 22;
static constexpr int CancelEnable = 0;
static constexpr int CancelDisable = 1;

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "pthread posix check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static void* APS5_VABI Entry(void* argument) {
    return argument;
}

int main() {
    PthreadAttr attr{};
    Check(pthread_attr_init_nid_postfix(&attr) == 0);
    Check(attr != nullptr);

    for (const int state : {1, 0}) {
        int read = -1;
        Check(pthread_attr_setdetachstate_nid_postfix(&attr, state) == 0);
        Check(pthread_attr_getdetachstate_nid_postfix(&attr, &read) == 0 && read == state);
    }
    for (const int invalid : {2, -1, 100}) {
        int read = -1;
        Check(pthread_attr_setdetachstate_nid_postfix(&attr, invalid) == GuestEinval);
        Check(pthread_attr_getdetachstate_nid_postfix(&attr, &read) == 0 && read == 0);
    }
    Check(pthread_attr_getdetachstate_nid_postfix(&attr, nullptr) == GuestEinval);

    for (const std::size_t size : {std::size_t{0}, std::size_t{4096}, std::size_t{65536}}) {
        std::size_t read = 1;
        Check(pthread_attr_setguardsize_nid_postfix(&attr, size) == 0);
        Check(pthread_attr_getguardsize_nid_postfix(&attr, &read) == 0 && read == size);
    }
    Check(pthread_attr_getguardsize_nid_postfix(&attr, nullptr) == GuestEinval);

    {
        std::size_t read = 0;
        Check(pthread_attr_setstacksize_nid_postfix(&attr, std::size_t{1} << 20) == 0);
        Check(pthread_attr_getstacksize_nid_postfix(&attr, &read) == 0 && read == (std::size_t{1} << 20));
        Check(pthread_attr_getstacksize_nid_postfix(&attr, nullptr) == GuestEinval);
    }

    for (const int priority : {256, 500, 767}) {
        KernelSchedParam written{};
        KernelSchedParam read{};
        written.sched_priority = priority;
        Check(pthread_attr_setschedparam_nid_postfix(&attr, &written) == 0);
        Check(pthread_attr_getschedparam_nid_postfix(&attr, &read) == 0 && read.sched_priority == priority);
    }
    Check(pthread_attr_getschedparam_nid_postfix(&attr, nullptr) == GuestEinval);

    Check(pthread_attr_destroy_nid_postfix(&attr) == 0);
    int afterDestroy = -1;
    Check(pthread_attr_getdetachstate_nid_postfix(&attr, &afterDestroy) == GuestEinval);
    Check(pthread_attr_init_nid_postfix(nullptr) == GuestEinval);
    Check(pthread_attr_destroy_nid_postfix(nullptr) == GuestEinval);

    const Pthread self = pthread_self_nid_postfix();
    Check(self != nullptr);
    Check(pthread_equal_nid_postfix(self, self) != 0);

    int marker = 0;
    Pthread created = nullptr;
    Check(pthread_create_name_np_nid_postfix(&created, nullptr, Entry, &marker, "aps5-named") == 0);
    Check(created != nullptr);
    Check(pthread_equal_nid_postfix(created, self) == 0);
    Check(pthread_equal_nid_postfix(created, created) != 0);
    void* returned = nullptr;
    Check(pthread_join_nid_postfix(created, &returned) == 0);
    Check(returned == &marker);

    Check(sched_yield_nid_postfix() == 0);
    pthread_yield_nid_postfix();

    int old = -1;
    Check(pthread_setcancelstate_nid_postfix(CancelDisable, &old) == 0 && old == CancelEnable);
    Check(pthread_setcancelstate_nid_postfix(CancelEnable, &old) == 0 && old == CancelDisable);
    Check(pthread_setcancelstate_nid_postfix(CancelEnable, &old) == 0 && old == CancelEnable);
    return 0;
}
