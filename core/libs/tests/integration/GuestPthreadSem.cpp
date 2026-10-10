#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

static void Require(bool value) { if (!value) std::abort(); }

struct Context {
    PthreadSem sem = nullptr;
};

static void* APS5_VABI Poster(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    return nullptr;
}

int main() {
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 2, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 2);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 0);
    Require(scePthreadSemTimedwait(&sem, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 1);

    Context context;
    Require(scePthreadSemInit(&context.sem, 0, 0, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Poster, &context, nullptr) == SCE_OK);
    Require(scePthreadSemTimedwait(&context.sem, 5000000) == SCE_OK);
    Require(scePthreadSemWait(&context.sem) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadSemDestroy(&context.sem) == SCE_OK);

    bool rejected = false;
    try { scePthreadSemWait(&context.sem); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}
