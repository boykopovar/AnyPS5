#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

namespace {

using Initialize = int (APS5_VABI*)(std::int64_t, std::uint32_t*);
using InstanceCreate = int (APS5_VABI*)(std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t*);
using InstanceDestroy = int (APS5_VABI*)(std::uint32_t, std::uint32_t);
using BatchInitialize = int (APS5_VABI*)(void*, std::size_t, AjmBatchInfo*);
using BatchJobClearContext = int (APS5_VABI*)(AjmBatchInfo*, std::uint32_t, void*);
using BatchStart = int (APS5_VABI*)(std::uint32_t, const AjmBatchInfo*, int, AjmBatchError*, std::uint32_t*);
using BatchWait = int (APS5_VABI*)(std::uint32_t, std::uint32_t, std::uint32_t, AjmBatchError*);

struct Functions {
    Initialize initialize;
    InstanceCreate instanceCreate;
    InstanceDestroy instanceDestroy;
    BatchInitialize batchInitialize;
    BatchJobClearContext batchJobClearContext;
    BatchStart batchStart;
    BatchWait batchWait;
};

struct SidebandResult {
    std::int32_t result;
    std::int32_t internalResult;
};

Functions ajm{};
std::uint32_t context = 0;
std::uint32_t instance = 0;

[[noreturn]] void Fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

template <typename T>
T Resolve(void* library, const char* name) {
    auto* symbol = dlsym(library, name);
    if (symbol == nullptr) Fail(name);
    return reinterpret_cast<T>(symbol);
}

void UseAfterStaticTeardown() {
    alignas(16) std::uint8_t buffer[0x100]{};
    AjmBatchInfo info{};
    SidebandResult sideband{-1, -1};
    if (ajm.batchInitialize(buffer, sizeof(buffer), &info) != 0 || ajm.batchJobClearContext(&info, instance, &sideband) != 0) Fail("a batch cannot be built after static teardown");
    std::uint32_t batch = 0;
    AjmBatchError error{};
    if (ajm.batchStart(context, &info, 0, &error, &batch) != 0) Fail("sceAjmBatchStart failed after static teardown");
    if (sideband.result != 0) Fail("an instance created before exit is unknown after static teardown");
    if (ajm.batchWait(context, batch, 0, &error) != 0) Fail("sceAjmBatchWait does not find a batch started after static teardown");
    if (ajm.instanceDestroy(context, instance) != 0) Fail("sceAjmInstanceDestroy does not find the instance after static teardown");
}

}

int main(int argc, char** argv) {
    if (argc != 2) Fail("usage: guest_ajm_teardown_tests <libSceAjm.native>");
    if (std::atexit(UseAfterStaticTeardown) != 0) Fail("atexit failed");
    auto* library = dlopen(argv[1], RTLD_NOW);
    if (library == nullptr) Fail(dlerror());
    ajm = {
        Resolve<Initialize>(library, "sceAjmInitialize"),
        Resolve<InstanceCreate>(library, "sceAjmInstanceCreate"),
        Resolve<InstanceDestroy>(library, "sceAjmInstanceDestroy"),
        Resolve<BatchInitialize>(library, "sceAjmBatchInitialize"),
        Resolve<BatchJobClearContext>(library, "sceAjmBatchJobClearContext"),
        Resolve<BatchStart>(library, "sceAjmBatchStart"),
        Resolve<BatchWait>(library, "sceAjmBatchWait"),
    };
    if (ajm.initialize(0, &context) != 0 || ajm.instanceCreate(context, 0, 0, &instance) != 0) Fail("AJM setup failed");
    return 0;
}
