#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackAddress, std::size_t* stackSize);
Pthread APS5_VABI scePthreadSelf();
}

static constexpr int SCE_OK = 0;
static constexpr std::size_t FRAME_SIZE = 4096;
static constexpr std::size_t FRAME_MARGIN = 2 * FRAME_SIZE;
static constexpr std::size_t STACK_SIZES[] = {16384, 65536, 1u << 20, 16u << 20};

thread_local unsigned char threadLocalBlock[512 * 1024];

static void Require(bool value) { if (!value) std::abort(); }

static std::uintptr_t Descend(std::uintptr_t floor);
static std::uintptr_t (*volatile descend)(std::uintptr_t) = Descend;

static std::uintptr_t Descend(std::uintptr_t floor) {
    volatile unsigned char frame[FRAME_SIZE];
    frame[0] = 0x5a;
    frame[FRAME_SIZE - 1] = 0xa5;
    const auto here = reinterpret_cast<std::uintptr_t>(&frame[0]);
    const auto deepest = here < floor + FRAME_MARGIN ? here : descend(floor);
    Require(frame[0] == 0x5a && frame[FRAME_SIZE - 1] == 0xa5);
    return deepest;
}

static void* APS5_VABI Worker(void* arg) {
    const auto requested = *static_cast<const std::size_t*>(arg);
    threadLocalBlock[0] = 1;
    threadLocalBlock[sizeof(threadLocalBlock) - 1] = 2;

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(scePthreadSelf(), &attr) == SCE_OK);
    void* address = nullptr;
    std::size_t size = 0;
    Require(scePthreadAttrGetstack(&attr, &address, &size) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(size == requested);

    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto local = reinterpret_cast<std::uintptr_t>(&size);
    Require(local > begin && local < begin + size);
    Require(local - begin + FRAME_SIZE >= requested);
    const auto deepest = Descend(begin);
    Require(deepest >= begin && deepest < begin + FRAME_MARGIN);

    Require(threadLocalBlock[0] == 1 && threadLocalBlock[sizeof(threadLocalBlock) - 1] == 2);
    return arg;
}

int main() {
    for (auto stackSize : STACK_SIZES) {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, stackSize) == SCE_OK);
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, Worker, &stackSize, nullptr) == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
        void* result = nullptr;
        Require(scePthreadJoin(thread, &result) == SCE_OK);
        Require(result == &stackSize);
    }
}
