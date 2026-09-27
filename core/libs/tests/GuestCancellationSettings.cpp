#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI pthread_setcanceltype_nid_postfix(int, int*);
int APS5_VABI pthread_setcancelstate_nid_postfix(int, int*);
int APS5_VABI scePthreadSetcanceltype(int, int*);
int APS5_VABI scePthreadSetcancelstate(int, int*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int oldType = -1;
    int oldState = -1;
    Require(pthread_setcanceltype_nid_postfix(2, &oldType) == 0 && oldType == 0);
    Require(pthread_setcancelstate_nid_postfix(1, &oldState) == 0 && oldState == 0);
    std::atomic<bool> independent{false};
    std::thread worker([&] {
        int workerType = -1;
        int workerState = -1;
        independent = pthread_setcanceltype_nid_postfix(2, &workerType) == 0 && workerType == 0 &&
                      pthread_setcancelstate_nid_postfix(1, &workerState) == 0 && workerState == 0;
    });
    worker.join();
    Require(independent);
    Require(scePthreadSetcanceltype(0, &oldType) == 0 && oldType == 2);
    Require(scePthreadSetcancelstate(0, &oldState) == 0 && oldState == 1);
    Require(pthread_setcanceltype_nid_postfix(2, nullptr) == 0);
    Require(pthread_setcancelstate_nid_postfix(1, nullptr) == 0);
    oldType = 123;
    oldState = 456;
    Require(pthread_setcanceltype_nid_postfix(1, &oldType) == 22 && oldType == 123);
    Require(pthread_setcancelstate_nid_postfix(2, &oldState) == 22 && oldState == 456);
    Require(pthread_setcanceltype_nid_postfix(0, &oldType) == 0 && oldType == 2);
    Require(pthread_setcancelstate_nid_postfix(0, &oldState) == 0 && oldState == 1);
}
