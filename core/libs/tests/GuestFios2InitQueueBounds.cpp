#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestInitializationLifecycleSequencing() {
    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosIsIdle());
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);

    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(static_cast<std::uintptr_t>(0x10))) == SCE_FIOS_ERROR_BAD_PTR);
    Require(!sceFiosIsInitialized(nullptr));

    Require(sceFiosInitialize(nullptr) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    std::uint32_t params1[16] = {};
    params1[0] = static_cast<std::uint32_t>(sizeof(params1) << 1);
    params1[6] = 3;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params1)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);

    std::uint32_t outParams[16] = {};
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(outParams)));
    Require(outParams[0] == params1[0]);
    Require(outParams[6] == 3);

    std::uint32_t params2[16] = {};
    params2[0] = static_cast<std::uint32_t>(sizeof(params2) << 1);
    params2[6] = 1;
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(params2));
    std::memset(outParams, 0, sizeof(outParams));
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(outParams)));
    Require(outParams[6] == 1);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
    std::memset(outParams, 0xaa, sizeof(outParams));
    Require(!sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(outParams)));
    Require(outParams[0] == 0xaaaaaaaa);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
}

void TestSuspensionAndQueueDepth() {
    std::uint32_t params[16] = {};
    params[0] = static_cast<std::uint32_t>(sizeof(params) << 1);
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);

    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());

    sceFiosSuspend();
    Require(sceFiosGetSuspendCount() == 1);
    Require(sceFiosIsSuspended());

    sceFiosSuspend();
    Require(sceFiosGetSuspendCount() == 2);
    Require(sceFiosIsSuspended());

    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 1);
    Require(sceFiosIsSuspended());

    constexpr std::size_t kQueueDepth = 16;
    SceFiosOp ops[kQueueDepth] = {};
    for (std::size_t i = 0; i < kQueueDepth; ++i) {
        ops[i] = sceFiosFileGetSize(nullptr, "nonexistent_fios_queue_probe.bin");
        Require(ops[i] > 0);
        Require(sceFiosIsValidHandle(ops[i]));
        Require(!sceFiosOpIsDone(ops[i]));
        Require(!sceFiosOpIsCancelled(ops[i]));
        Require(sceFiosOpGetError(ops[i]) == 1);
    }
    Require(!sceFiosIsIdle());

    const std::uint32_t totalCount = sceFiosGetAllOps(nullptr, 0);
    Require(totalCount >= kQueueDepth);
    std::vector<SceFiosOp> listedOps(totalCount);
    const std::uint32_t copiedCount = sceFiosGetAllOps(listedOps.data(), totalCount);
    Require(copiedCount == totalCount);
    for (std::size_t i = 0; i < kQueueDepth; ++i) {
        bool found = false;
        for (std::uint32_t j = 0; j < copiedCount; ++j) {
            if (listedOps[j] == ops[i]) {
                found = true;
                break;
            }
        }
        Require(found);
    }

    for (std::size_t i = 0; i < kQueueDepth / 2; ++i) {
        sceFiosOpCancel(ops[i]);
        Require(sceFiosOpIsCancelled(ops[i]));
        Require(sceFiosOpIsDone(ops[i]));
        Require(sceFiosOpGetError(ops[i]) == SCE_FIOS_ERROR_CANCELLED);
    }
    for (std::size_t i = kQueueDepth / 2; i < kQueueDepth; ++i) {
        Require(!sceFiosOpIsCancelled(ops[i]));
        Require(!sceFiosOpIsDone(ops[i]));
        Require(sceFiosOpGetError(ops[i]) == 1);
    }

    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());

    for (std::size_t i = 0; i < kQueueDepth / 2; ++i) {
        Require(sceFiosOpWait(ops[i]) == SCE_FIOS_ERROR_CANCELLED);
        Require(sceFiosOpIsDone(ops[i]));
        sceFiosOpDelete(ops[i]);
        Require(!sceFiosIsValidHandle(ops[i]));
    }
    for (std::size_t i = kQueueDepth / 2; i < kQueueDepth; ++i) {
        Require(sceFiosOpWait(ops[i]) == SCE_FIOS_ERROR_BAD_PATH);
        Require(sceFiosOpIsDone(ops[i]));
        sceFiosOpDelete(ops[i]);
        Require(!sceFiosIsValidHandle(ops[i]));
    }
    Require(sceFiosIsIdle());

    sceFiosTerminate();
}

void TestCancelAllAndShutdownOps() {
    Require(sceFiosInitialize(nullptr) == SCE_FIOS_OK);

    sceFiosSuspend();
    const SceFiosOp qOp1 = sceFiosFileGetSize(nullptr, "cancel_all_probe1.bin");
    const SceFiosOp qOp2 = sceFiosFileGetSize(nullptr, "cancel_all_probe2.bin");
    Require(qOp1 > 0 && qOp2 > 0);
    Require(!sceFiosIsIdle());

    sceFiosCancelAllOps();
    Require(sceFiosOpIsCancelled(qOp1));
    Require(sceFiosOpIsCancelled(qOp2));
    Require(sceFiosOpWait(qOp1) == SCE_FIOS_ERROR_CANCELLED);
    Require(sceFiosOpWait(qOp2) == SCE_FIOS_ERROR_CANCELLED);

    sceFiosOpDelete(qOp1);
    sceFiosOpDelete(qOp2);
    sceFiosResume();
    Require(sceFiosIsIdle());

    sceFiosSuspend();
    const SceFiosOp sOp = sceFiosFileGetSize(nullptr, "shutdown_probe.bin");
    Require(sOp > 0);
    sceFiosShutdownAndCancelOps();
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosGetSuspendCount() == 0);
    Require(sceFiosIsIdle());
}

void TestGlobalAndThreadOpAttrSequencing() {
    Require(sceFiosInitialize(nullptr) == SCE_FIOS_OK);

    SceFiosOpAttr attr{};
    attr.priority = 10;
    attr.userTag = 0xbeef;
    sceFiosSetGlobalDefaultOpAttr(&attr);

    SceFiosOpAttr retrieved{};
    Require(sceFiosGetGlobalDefaultOpAttr(&retrieved));
    Require(retrieved.priority == 10);
    Require(retrieved.userTag == 0xbeef);

    sceFiosSetGlobalDefaultOpAttr(nullptr);
    Require(!sceFiosGetGlobalDefaultOpAttr(&retrieved));

    attr.priority = 20;
    attr.userTag = 0xfeed;
    sceFiosSetThreadDefaultOpAttr(&attr);

    std::memset(&retrieved, 0, sizeof(retrieved));
    Require(sceFiosGetThreadDefaultOpAttr(&retrieved));
    Require(retrieved.priority == 20);
    Require(retrieved.userTag == 0xfeed);

    sceFiosSetThreadDefaultOpAttr(nullptr);
    Require(!sceFiosGetThreadDefaultOpAttr(&retrieved));

    attr.priority = 15;
    attr.userTag = 0xcafe;
    sceFiosSetGlobalDefaultOpAttr(&attr);

    std::memset(&retrieved, 0, sizeof(retrieved));
    sceFiosGetDefaultOpAttr(&retrieved);
    Require(retrieved.priority == 15);
    Require(retrieved.userTag == 0xcafe);

    sceFiosSetGlobalDefaultOpAttr(nullptr);
    sceFiosTerminate();
}

} // namespace

int main() {
    TestInitializationLifecycleSequencing();
    TestSuspensionAndQueueDepth();
    TestCancelAllAndShutdownOps();
    TestGlobalAndThreadOpAttrSequencing();
    return 0;
}
