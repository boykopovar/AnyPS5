#include "SceTypes.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <thread>

extern "C" {
int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t, std::uint32_t);
int APS5_VABI sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadJoin(void*, std::int32_t*);
}

static constexpr int Ok = 0;
static constexpr int Null = -2139029503;
static constexpr int State = -2139029498;
static constexpr int Busy = -2139029497;

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ULT thread check failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(value) Require((value), __LINE__)

static std::atomic<bool> gStarted{false};
static std::atomic<bool> gRelease{false};

static std::int32_t APS5_VABI Parked(std::uint64_t arg) {
    gStarted.store(true, std::memory_order_release);
    while (!gRelease.load(std::memory_order_acquire)) std::this_thread::yield();
    return static_cast<std::int32_t>(arg);
}

static std::int32_t APS5_VABI Immediate(std::uint64_t arg) {
    return static_cast<std::int32_t>(arg);
}

struct alignas(8) Runtime { std::uint8_t bytes[4096]{}; };
struct alignas(8) Thread { std::uint8_t bytes[512]{}; };

static void Outstanding() {
    Runtime runtime;
    Thread thread;
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(&thread, "parked", Parked, 0x5A, nullptr, 0, &runtime, nullptr, 0) == Ok);
    while (!gStarted.load(std::memory_order_acquire)) std::this_thread::yield();

    const int outstanding = sceUltFinalize();
    gRelease.store(true, std::memory_order_release);
    std::int32_t status = 0;
    const int joined = sceUltUlthreadJoin(&thread, &status);

    CHECK(outstanding == Busy);
    CHECK(joined == Ok && status == 0x5A);
    CHECK(sceUltUlthreadJoin(&thread, &status) == State);
    CHECK(sceUltFinalize() == Ok);
}

static void Validation() {
    Runtime runtime;
    Thread thread;
    gStarted.store(false, std::memory_order_release);
    gRelease.store(false, std::memory_order_release);
    CHECK(sceUltUlthreadRuntimeCreate(nullptr, "runtime", 1, 1, nullptr, nullptr, 0) == Null);
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(nullptr, "thread", Immediate, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    CHECK(sceUltUlthreadCreate(&thread, "thread", nullptr, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0, nullptr, 0, nullptr, nullptr, 0) == Null);
    CHECK(sceUltUlthreadJoin(nullptr, nullptr) == Null);
    CHECK(sceUltUlthreadJoin(&thread, nullptr) == State);

    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == State);
    CHECK(sceUltFinalize() == Busy);
    std::int32_t status = 0;
    CHECK(sceUltUlthreadJoin(&thread, &status) == Ok && status == 0x27);
    CHECK(sceUltFinalize() == Ok);
    CHECK(sceUltUlthreadRuntimeGetWorkAreaSize(1, 1) == 256u + 16u * 1024u);
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(sceUltInitialize() == Ok);
    if (std::strcmp(argv[1], "outstanding") == 0) Outstanding();
    else if (std::strcmp(argv[1], "validation") == 0) Validation();
    else CHECK(false);
}
