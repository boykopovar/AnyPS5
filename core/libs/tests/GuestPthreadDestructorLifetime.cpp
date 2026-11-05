#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <thread>

struct Context {
    PthreadKey key = -1;
    Pthread expected = nullptr;
    int expectedId = 0;
    unsigned calls = 0;
    unsigned failures = 0;
    bool exitExplicitly = false;
    bool pendingCancel = false;

    void Check(bool passed, const char* message) {
        if (passed) return;
        std::fprintf(stderr, "%s: %s\n", exitExplicitly ? "explicit exit" : "return", message);
        ++failures;
    }
};

static void APS5_VABI DestroyValue(void* value) {
    auto& context = *static_cast<Context*>(value);
    ++context.calls;
    context.Check(scePthreadGetspecific(context.key) == nullptr, "key value was not cleared before its destructor");
    if (context.expected) {
        const auto self = scePthreadSelf();
        context.Check(self == context.expected, "key destructor lost the exiting thread identity");
        context.Check(scePthreadGetthreadid() == context.expectedId, "key destructor lost the exiting thread ID");
        char name[32]{};
        context.Check(scePthreadGetname(self, name) == 0 && std::strcmp(name, "KeyDestructor") == 0,
                      "key destructor lost the exiting thread name");
        context.Check(sceKernelUsleep_nid_postfix(1000) == 0, "key destructor could not wait");
    }
    context.Check(scePthreadSetspecific(context.key, value) == 0, "key destructor could not restore its value");
}

static void* APS5_VABI Worker(void* value) {
    auto& context = *static_cast<Context*>(value);
    context.expected = scePthreadSelf();
    context.expectedId = scePthreadGetthreadid();
    context.Check(scePthreadSetspecific(context.key, value) == 0, "worker could not bind its key value");
    if (context.pendingCancel) context.Check(scePthreadCancel(context.expected) == 0, "worker could not request cancellation");
    if (context.exitExplicitly) scePthreadExit(value);
    return value;
}

static bool RunGuest(bool exitExplicitly, bool pendingCancel = false) {
    Context context;
    context.exitExplicitly = exitExplicitly;
    context.pendingCancel = pendingCancel;
    if (scePthreadKeyCreate(&context.key, reinterpret_cast<pthread_key_destructor_func_t>(DestroyValue)) != 0) return false;
    Pthread thread = nullptr;
    if (scePthreadCreate(&thread, nullptr, Worker, &context, "KeyDestructor") != 0) return false;
    void* result = nullptr;
    context.Check(scePthreadJoin(thread, &result) == 0 && result == &context, "thread did not join with its return value");
    context.Check(context.calls == 4, "key destructor did not stop after four iterations");
    context.Check(scePthreadKeyDelete(context.key) == 0, "key deletion failed");
    return context.failures == 0;
}

static bool RunHost() {
    Context context;
    if (scePthreadKeyCreate(&context.key, reinterpret_cast<pthread_key_destructor_func_t>(DestroyValue)) != 0) return false;
    std::thread worker([&] {
        context.Check(scePthreadSetspecific(context.key, &context) == 0, "host worker could not bind its key value");
    });
    worker.join();
    context.Check(context.calls == 4, "host thread did not run four key destructor iterations");
    context.Check(scePthreadKeyDelete(context.key) == 0, "host key deletion failed");
    return context.failures == 0;
}

int main() {
    const bool returned = RunGuest(false);
    const bool exited = RunGuest(true);
    const bool canceled = RunGuest(false, true);
    const bool host = RunHost();
    if (!returned || !exited || !canceled || !host) return 1;
    std::puts("PASS: pthread key destructors retain thread identity on return and explicit exit; pending cancellation, four iterations and host cleanup preserved");
}
