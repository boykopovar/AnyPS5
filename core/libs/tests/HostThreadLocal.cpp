#include <cstdlib>
#include <array>
#include <chrono>
#include <future>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" void TouchHostThreadLocal();
extern "C" unsigned DestroyedHostThreadLocals();

int main() {
    for (unsigned i = 0; i < 64; ++i) {
        std::thread worker([] { TouchHostThreadLocal(); TouchHostThreadLocal(); });
        worker.join();
        if (DestroyedHostThreadLocals() != (i + 1) * 2) std::abort();
    }
#ifdef _WIN32
    for (unsigned i = 0; i < 64; ++i) {
        const auto worker = CreateThread(nullptr, 0, +[](void*) -> DWORD { TouchHostThreadLocal(); return 0; }, nullptr, 0, nullptr);
        if (!worker || WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) std::abort();
        CloseHandle(worker);
        if (DestroyedHostThreadLocals() != 128 + (i + 1) * 2) std::abort();
    }
#endif
    for (unsigned i = 0; i < 16; ++i) {
        const auto before = DestroyedHostThreadLocals();
        std::array<std::future<void>, 4> workers;
        for (auto& worker : workers) {
            worker = std::async(std::launch::async, [] { TouchHostThreadLocal(); TouchHostThreadLocal(); });
        }
        for (auto& worker : workers) worker.get();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (DestroyedHostThreadLocals() < before + 8 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (DestroyedHostThreadLocals() != before + 8) std::abort();
    }

}
