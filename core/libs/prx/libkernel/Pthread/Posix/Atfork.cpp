#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include "../include/Atfork.hpp"
#include <mutex>
#include <new>

namespace {
constexpr int GuestNoMemory = 12;
std::mutex atforkLock;
std::vector<PthreadAtforkHandlers> atforkHandlers;
}

extern "C" int APS5_VABI pthread_atfork_nid_postfix(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    std::lock_guard lock(atforkLock);
    try {
        atforkHandlers.push_back({prepare, parent, child});
    } catch (const std::bad_alloc&) {
        return GuestNoMemory;
    }
    return 0;
}

std::vector<PthreadAtforkHandlers> PthreadAtforkHandlers_nid_no_patch() {
    std::lock_guard lock(atforkLock);
    return atforkHandlers;
}
