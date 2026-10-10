#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

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

struct ChildState {
    Functions ajm{};
    std::uint32_t context = 0;
    std::uint32_t instance = 0;
    int reportFd = -1;
};

ChildState& Child() {
    static ChildState state;
    return state;
}

[[noreturn]] void ChildFail(const std::string& message) {
    const auto written = write(Child().reportFd, message.data(), message.size());
    static_cast<void>(written);
    std::_Exit(1);
}

template<typename TFunction>
TFunction Resolve(void* library, const char* name) {
    auto* symbol = dlsym(library, name);
    if (symbol == nullptr) ChildFail(std::string("missing symbol ") + name);
    return reinterpret_cast<TFunction>(symbol);
}

void UseAfterStaticTeardown() {
    const auto& child = Child();
    alignas(16) std::uint8_t buffer[0x100]{};
    AjmBatchInfo info{};
    SidebandResult sideband{-1, -1};
    if (child.ajm.batchInitialize(buffer, sizeof(buffer), &info) != 0 || child.ajm.batchJobClearContext(&info, child.instance, &sideband) != 0) {
        ChildFail("a batch cannot be built after static teardown");
    }
    std::uint32_t batch = 0;
    AjmBatchError error{};
    if (child.ajm.batchStart(child.context, &info, 0, &error, &batch) != 0) ChildFail("sceAjmBatchStart failed after static teardown");
    if (sideband.result != 0) ChildFail("an instance created before exit is unknown after static teardown");
    if (child.ajm.batchWait(child.context, batch, 0, &error) != 0) ChildFail("sceAjmBatchWait does not find a batch started after static teardown");
    if (child.ajm.instanceDestroy(child.context, child.instance) != 0) ChildFail("sceAjmInstanceDestroy does not find the instance after static teardown");
}

[[noreturn]] void RunChild(const std::string& libraryPath) {
    auto& child = Child();
    if (std::atexit(UseAfterStaticTeardown) != 0) ChildFail("atexit failed");
    auto* library = dlopen(libraryPath.c_str(), RTLD_NOW);
    if (library == nullptr) ChildFail(std::string("dlopen failed: ") + dlerror());
    child.ajm = {
        Resolve<Initialize>(library, "sceAjmInitialize"),
        Resolve<InstanceCreate>(library, "sceAjmInstanceCreate"),
        Resolve<InstanceDestroy>(library, "sceAjmInstanceDestroy"),
        Resolve<BatchInitialize>(library, "sceAjmBatchInitialize"),
        Resolve<BatchJobClearContext>(library, "sceAjmBatchJobClearContext"),
        Resolve<BatchStart>(library, "sceAjmBatchStart"),
        Resolve<BatchWait>(library, "sceAjmBatchWait"),
    };
    if (child.ajm.initialize(0, &child.context) != 0 || child.ajm.instanceCreate(child.context, 0, 0, &child.instance) != 0) {
        ChildFail("AJM setup failed");
    }
    std::exit(0);
}

const Case staticTeardown{"StaticTeardown_InstanceCreatedBeforeExit_StaysUsableFromAtexitHandler", [] {
    const std::string& libraryPath = Testing::RequireArgument(0, "library path");
    int report[2] = {-1, -1};
    Require(pipe(report) == 0, "create the report pipe");
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);
    const pid_t child = fork();
    if (child == 0) {
        close(report[0]);
        Child().reportFd = report[1];
        RunChild(libraryPath);
    }
    close(report[1]);
    if (child < 0) {
        close(report[0]);
        Testing::Fail("fork the teardown child");
    }
    std::string message;
    char buffer[256];
    for (ssize_t bytes = read(report[0], buffer, sizeof(buffer)); bytes > 0; bytes = read(report[0], buffer, sizeof(buffer))) {
        message.append(buffer, static_cast<std::size_t>(bytes));
    }
    close(report[0]);
    int status = 0;
    RequireEqual(waitpid(child, &status, 0), child, "wait for the teardown child");
    Require(WIFEXITED(status), "the teardown child exits normally");
    RequireEqual(WEXITSTATUS(status), 0, "teardown child exit status (" + message + ")");
}};

} // namespace
