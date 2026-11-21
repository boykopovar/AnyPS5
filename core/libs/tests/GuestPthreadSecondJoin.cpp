#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

extern "C" void APS5_Test_SetFinalThreadReleaseHook(Pthread thread, void (*hook)(Pthread));

static constexpr int SCE_OK = 0;
static constexpr int GUEST_ESRCH = 3;
static constexpr std::size_t REUSED_SLOT_ROUNDS = 64;
static constexpr std::size_t RETAINED_JOINED_THREADS = 100;

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI Finish(void*) {
    return reinterpret_cast<void*>(std::uintptr_t{0x1234});
}

static void* APS5_VABI WaitForRelease(void* arg) {
    auto* release = static_cast<std::atomic<bool>*>(arg);
    while (!release->load()) std::this_thread::yield();
    return nullptr;
}

static std::atomic<bool> oldestReleased{false};

static void MarkOldestReleased(Pthread) {
    oldestReleased.store(true);
}

static void CreateAndJoin(Pthread& handle) {
    Require(scePthreadCreate(&handle, nullptr, Finish, nullptr, "retained") == SCE_OK);
    Require(scePthreadJoin(handle, nullptr) == SCE_OK);
}

static void RequireJoinedThreadsAreBounded() {
    std::vector<Pthread> handles(RETAINED_JOINED_THREADS + 1);
    CreateAndJoin(handles[0]);
    APS5_Test_SetFinalThreadReleaseHook(handles[0], MarkOldestReleased);
    for (std::size_t index = 1; index < RETAINED_JOINED_THREADS; ++index) CreateAndJoin(handles[index]);
    Require(!oldestReleased.load());
    Require(scePthreadJoin(handles[0], nullptr) == SCE_KERNEL_ERROR_ESRCH);
    CreateAndJoin(handles[RETAINED_JOINED_THREADS]);
    Require(oldestReleased.load());
    char name[32] = {'x'};
    int clockId = 0x55;
    Require(scePthreadJoin(handles[0], nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadDetach(handles[0]) == SCE_KERNEL_ERROR_ESRCH);
    Require(pthread_getname_np_nid_postfix(handles[0], name) == GUEST_ESRCH && name[0] == 'x');
    Require(pthread_getcpuclockid_nid_postfix(handles[0], &clockId) == GUEST_ESRCH && clockId == 0x55);
    for (std::size_t index = 1; index <= RETAINED_JOINED_THREADS; ++index)
        Require(scePthreadJoin(handles[index], nullptr) == SCE_KERNEL_ERROR_ESRCH);
}

int main() {
    RequireJoinedThreadsAreBounded();

    Pthread thread = nullptr;
    void* result = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Finish, nullptr, "second-join") == SCE_OK);
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x1234}));

    result = reinterpret_cast<void*>(std::uintptr_t{0x55});
    Require(scePthreadJoin(thread, &result) == SCE_KERNEL_ERROR_ESRCH);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x55}));
    Require(scePthreadDetach(thread) == SCE_KERNEL_ERROR_ESRCH);
    Require(pthread_join_nid_postfix(thread, &result) == GUEST_ESRCH);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x55}));

    for (std::size_t round = 0; round < REUSED_SLOT_ROUNDS; ++round) {
        thread = nullptr;
        result = nullptr;
        Require(scePthreadCreate(&thread, nullptr, Finish, nullptr, "reused-slot") == SCE_OK);
        Require(scePthreadJoin(thread, &result) == SCE_OK);
        Require(result == reinterpret_cast<void*>(std::uintptr_t{0x1234}));
        Require(scePthreadJoin(thread, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    }

    static std::atomic<bool> release{false};
    Pthread detached = nullptr;
    Require(scePthreadCreate(&detached, nullptr, WaitForRelease, &release, "detached") == SCE_OK);
    Require(scePthreadDetach(detached) == SCE_OK);
    result = reinterpret_cast<void*>(std::uintptr_t{0x55});
    Require(scePthreadJoin(detached, &result) == SCE_KERNEL_ERROR_EINVAL);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x55}));
    Require(scePthreadDetach(detached) == SCE_KERNEL_ERROR_EINVAL);
    release = true;

    Pthread named = nullptr;
    char name[64] = {};
    Require(scePthreadCreate(&named, nullptr, Finish, nullptr, "0123456789abcdefghijklmnopqrstu") == SCE_OK);
    Require(scePthreadGetname(named, name) == SCE_OK && std::strcmp(name, "0123456789abcdefghijklmnopqrstu") == 0);
    Require(scePthreadJoin(named, nullptr) == SCE_OK);
    for (const char* tooLong : {"0123456789abcdefghijklmnopqrstuv", "0123456789abcdefghijklmnopqrstuvw"}) {
        Pthread cleared = reinterpret_cast<Pthread>(std::uintptr_t{0x77});
        Require(scePthreadCreate(&cleared, nullptr, Finish, nullptr, tooLong) == SCE_KERNEL_ERROR_ENAMETOOLONG);
        Require(cleared == nullptr);
    }
    Pthread unnamed = nullptr;
    std::memset(name, 'x', sizeof(name));
    Require(scePthreadCreate(&unnamed, nullptr, Finish, nullptr, nullptr) == SCE_OK);
    Require(scePthreadGetname(unnamed, name) == SCE_OK && name[0] == 0);
    Require(scePthreadJoin(unnamed, nullptr) == SCE_OK);
}
