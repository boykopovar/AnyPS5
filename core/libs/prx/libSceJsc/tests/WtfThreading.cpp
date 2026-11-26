#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <thread>

int main() {
    _ZN3WTF19initializeThreadingEv();
    _ZN3WTF19initializeThreadingEv();
    std::thread other([] { _ZN3WTF19initializeThreadingEv(); });
    other.join();
    std::puts("WTF initializeThreading tests passed");
    return 0;
}
