#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

static constexpr int SIGUSR1 = 30;
static constexpr int SCE_OK = 0;
static constexpr int Rounds = 100;
static constexpr std::size_t Threads = 32;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> handled{0};
static std::atomic<int> waiting{0};
static HANDLE release = nullptr;

static void APS5_VABI Handler(int signum, void*) {
    Require(signum == SIGUSR1);
    handled.fetch_add(1);
}

static void* APS5_VABI Exiting(void*) {
    waiting.fetch_add(1);
    Require(WaitForSingleObject(release, INFINITE) == WAIT_OBJECT_0);
    return nullptr;
}

int main() {
    release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(release != nullptr);
    Require(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(&Handler)) == 0);
    int raised = 0;
    for (int round = 0; round < Rounds; ++round) {
        Require(ResetEvent(release) != 0);
        waiting.store(0);
        std::array<Pthread, Threads> threads{};
        for (auto& thread : threads) Require(scePthreadCreate(&thread, nullptr, Exiting, nullptr, "exiting") == SCE_OK);
        while (waiting.load() != static_cast<int>(Threads)) std::this_thread::yield();
        for (const auto thread : threads) {
            Require(sceKernelRaiseException(thread, SIGUSR1) == SCE_OK);
            ++raised;
        }
        Require(SetEvent(release) != 0);
        for (const auto thread : threads) Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    }
    Require(handled.load() <= raised);
    Require(sceKernelRemoveExceptionHandler(SIGUSR1) == 0);
    Require(CloseHandle(release) != 0);
    std::printf("%d raises at threads that exit after their host wait, %d handled\n", raised, handled.load());
    return 0;
}
