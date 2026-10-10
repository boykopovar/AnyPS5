#include <Testing/Test.hpp>

#include <array>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" void TouchHostThreadLocal();
extern "C" unsigned DestroyedHostThreadLocals();
extern "C" unsigned HostThreadLocalViolations();

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

void TouchTwice() {
    TouchHostThreadLocal();
    TouchHostThreadLocal();
}

void RequireNoViolations() {
    RequireEqual(HostThreadLocalViolations(), 0u, "distinct, stable and intact thread-local values");
}

const Case stdThreads{"HostThreadLocal_StdThreadExits_DestroysBothValues", [] {
    for (unsigned i = 0; i < 64; ++i) {
        const auto before = DestroyedHostThreadLocals();
        std::thread worker(TouchTwice);
        worker.join();
        RequireEqual(DestroyedHostThreadLocals(), before + 2, "destroyed values after thread " + std::to_string(i));
    }
    RequireNoViolations();
}};

#ifdef _WIN32
const Case nativeThreads{"HostThreadLocal_NativeThreadExits_DestroysBothValues", [] {
    for (unsigned i = 0; i < 64; ++i) {
        const auto before = DestroyedHostThreadLocals();
        const auto worker = CreateThread(nullptr, 0, +[](void*) -> DWORD {
            TouchHostThreadLocal();
            return 0;
        }, nullptr, 0, nullptr);
        Require(worker != nullptr, "create native thread " + std::to_string(i));
        const auto waited = WaitForSingleObject(worker, 5000);
        CloseHandle(worker);
        RequireEqual(waited, static_cast<DWORD>(WAIT_OBJECT_0), "native thread " + std::to_string(i) + " finishes");
        RequireEqual(DestroyedHostThreadLocals(), before + 2, "destroyed values after native thread " + std::to_string(i));
    }
    RequireNoViolations();
}};
#endif

const Case asyncWorkers{"HostThreadLocal_ConcurrentAsyncWorkers_DestroyEveryValue", [] {
    for (unsigned i = 0; i < 16; ++i) {
        const auto before = DestroyedHostThreadLocals();
        std::array<std::future<void>, 4> workers;
        for (auto& worker : workers) worker = std::async(std::launch::async, TouchTwice);
        for (auto& worker : workers) worker.get();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (DestroyedHostThreadLocals() < before + 8 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        RequireEqual(DestroyedHostThreadLocals(), before + 8, "destroyed values after round " + std::to_string(i));
    }
    RequireNoViolations();
}};

} // namespace
