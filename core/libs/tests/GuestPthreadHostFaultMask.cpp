#include "SceTypes.hpp"
#include <csignal>
#include <cstdlib>
#include <pthread.h>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
}

static constexpr int FaultSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP};
static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI CheckMask(void* arg) {
    sigset_t mask;
    Require(pthread_sigmask(SIG_SETMASK, nullptr, &mask) == 0);
    for (const int signal : FaultSignals) Require(sigismember(&mask, signal) == 0);
    *static_cast<bool*>(arg) = true;
    return nullptr;
}

int main() {
    sigset_t oldMask;
    sigset_t blocked;
    sigemptyset(&blocked);
    for (const int signal : FaultSignals) sigaddset(&blocked, signal);
    Require(pthread_sigmask(SIG_BLOCK, &blocked, &oldMask) == 0);

    bool ran = false;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, CheckMask, &ran, "host-fault-mask") == 0);

    sigset_t creatorMask;
    Require(pthread_sigmask(SIG_SETMASK, nullptr, &creatorMask) == 0);
    for (const int signal : FaultSignals) Require(sigismember(&creatorMask, signal) == 1);
    Require(pthread_sigmask(SIG_SETMASK, &oldMask, nullptr) == 0);

    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(ran);
}
