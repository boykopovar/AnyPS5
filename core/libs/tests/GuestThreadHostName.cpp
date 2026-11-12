#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdlib>
#include <pthread.h>
#include <string>
#include <thread>

static constexpr int SCE_OK = 0;

static void Require(bool condition) {
    if (!condition) std::abort();
}

static std::string HostName() {
    char name[32] = {};
    Require(pthread_getname_np(pthread_self(), name, sizeof name) == 0);
    return name;
}

static std::atomic<int> stage{0};
static std::string created;
static std::string renamedByOther;
static std::string renamedBySelf;

static void* APS5_VABI Worker(void*) {
    created = HostName();
    stage.store(1, std::memory_order_release);
    while (stage.load(std::memory_order_acquire) != 2) std::this_thread::yield();
    renamedByOther = HostName();
    Require(scePthreadRename(scePthreadSelf(), "audio mixer") == SCE_OK);
    renamedBySelf = HostName();
    return nullptr;
}

int main() {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, nullptr, "RenderThread job 7") == SCE_OK);
    while (stage.load(std::memory_order_acquire) != 1) std::this_thread::yield();
    Require(scePthreadRename(thread, "streaming worker") == SCE_OK);
    stage.store(2, std::memory_order_release);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(created == "RenderThreadjob");
    Require(renamedByOther == "streamingworker");
    Require(renamedBySelf == "audio mixer");
    return 0;
}
