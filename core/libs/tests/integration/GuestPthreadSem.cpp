#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name);
int APS5_VABI scePthreadSemDestroy(PthreadSem* sem);
int APS5_VABI scePthreadSemPost(PthreadSem* sem);
int APS5_VABI scePthreadSemWait(PthreadSem* sem);
int APS5_VABI scePthreadSemTrywait(PthreadSem* sem);
int APS5_VABI scePthreadSemTimedwait(PthreadSem* sem, unsigned int usec);
int APS5_VABI scePthreadSemGetvalue(PthreadSem* sem, int* value);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int SCE_OK = 0;
constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

class Semaphore {
public:
    explicit Semaphore(unsigned int initial) {
        RequireEqual(scePthreadSemInit(&sem, 0, initial, nullptr), SCE_OK, "sem init");
        initialized = true;
    }
    ~Semaphore() {
        if (initialized) scePthreadSemDestroy(&sem);
    }
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    int Value() {
        int value = -1;
        RequireEqual(scePthreadSemGetvalue(&sem, &value), SCE_OK, "sem getvalue");
        return value;
    }

    int Destroy() {
        initialized = false;
        return scePthreadSemDestroy(&sem);
    }

    PthreadSem sem = nullptr;

private:
    bool initialized = false;
};

struct PosterRun {
    PthreadSem* sem;
    std::atomic<int> firstPost{-1};
    std::atomic<int> secondPost{-1};
};

void* APS5_VABI Poster(void* arg) {
    auto& run = *static_cast<PosterRun*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    run.firstPost.store(scePthreadSemPost(run.sem));
    run.secondPost.store(scePthreadSemPost(run.sem));
    return nullptr;
}

const Case initialValue{"SemInit_InitialCount_IsReportedByGetvalue", [] {
    Semaphore semaphore(2);
    RequireEqual(semaphore.Value(), 2, "value");
}};

const Case exhausted{"SemTrywait_CountExhausted_FailsWithEbusy", [] {
    Semaphore semaphore(2);
    RequireEqual(scePthreadSemTrywait(&semaphore.sem), SCE_OK, "first trywait");
    RequireEqual(scePthreadSemTrywait(&semaphore.sem), SCE_OK, "second trywait");
    RequireEqual(scePthreadSemTrywait(&semaphore.sem), SCE_KERNEL_ERROR_EBUSY, "third trywait");
    RequireEqual(semaphore.Value(), 0, "value");
}};

const Case timeout{"SemTimedwait_ZeroCount_TimesOut", [] {
    Semaphore semaphore(0);
    RequireEqual(scePthreadSemTimedwait(&semaphore.sem, 1000), SCE_KERNEL_ERROR_ETIMEDOUT, "timed wait");
}};

const Case post{"SemPost_ZeroCount_IncrementsValue", [] {
    Semaphore semaphore(0);
    RequireEqual(scePthreadSemPost(&semaphore.sem), SCE_OK, "post");
    RequireEqual(semaphore.Value(), 1, "value");
    RequireEqual(semaphore.Destroy(), SCE_OK, "destroy");
}};

const Case crossThread{"SemWait_PostedFromGuestThread_WakesWaiter", [] {
    Semaphore semaphore(0);
    PosterRun run{&semaphore.sem};
    Pthread thread = nullptr;
    RequireEqual(scePthreadCreate(&thread, nullptr, Poster, &run, nullptr), SCE_OK, "create poster");
    const int timedWait = scePthreadSemTimedwait(&semaphore.sem, 5000000);
    const int wait = timedWait == SCE_OK ? scePthreadSemWait(&semaphore.sem) : timedWait;
    RequireEqual(scePthreadJoin(thread, nullptr), SCE_OK, "join poster");
    RequireEqual(timedWait, SCE_OK, "timed wait");
    RequireEqual(wait, SCE_OK, "wait");
    RequireEqual(run.firstPost.load(), SCE_OK, "first post");
    RequireEqual(run.secondPost.load(), SCE_OK, "second post");
    RequireEqual(semaphore.Destroy(), SCE_OK, "destroy");
}};

const Case destroyed{"SemWait_DestroyedSemaphore_Throws", [] {
    Semaphore semaphore(0);
    RequireEqual(semaphore.Destroy(), SCE_OK, "destroy");
    RequireThrows<std::runtime_error>([&semaphore] { scePthreadSemWait(&semaphore.sem); }, "wait on destroyed semaphore");
}};

} // namespace
