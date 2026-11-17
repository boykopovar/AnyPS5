#include "prx/libSceFios2/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::int32_t FiosOk = 0;
constexpr std::int32_t FiosErrorBadPtr = static_cast<std::int32_t>(0x80820006u);
constexpr std::int32_t FiosErrorBadIndex = static_cast<std::int32_t>(0x8082001Cu);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int APS5_VABI TestFilterCallback(void*, std::int32_t, void*) {
    return 0;
}

void TestUninitializedApiRejection() {
    Require(!sceFiosIsInitialized(nullptr));

    std::array<std::uint32_t, 16> dummyOut{};
    auto* outParams = reinterpret_cast<SceFiosParams*>(dummyOut.data());
    Require(!sceFiosIsInitialized(outParams));
    Require(dummyOut[0] == 0);

    std::uint8_t req[0x410]{};
    std::uint8_t res[0x410]{};
    Require(sceFiosResolve(nullptr, req, res) == 0);

    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());

    std::array<std::uint32_t, 16> updateBuffer{};
    updateBuffer[0] = static_cast<std::uint32_t>(sizeof(updateBuffer)) << 1;
    updateBuffer[3] = 0xbeefcafe;
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(updateBuffer.data()));
    Require(!sceFiosIsInitialized(nullptr));
}

void TestParameterValidation() {
    Require(sceFiosInitialize(nullptr) == FiosOk);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    const auto* badParams = reinterpret_cast<const SceFiosParams*>(1);
    Require(sceFiosInitialize(badParams) == FiosErrorBadPtr);
    Require(!sceFiosIsInitialized(nullptr));

    std::array<std::uint8_t, 0x50> paramsBuffer{};
    const std::uint32_t sizeHeader = static_cast<std::uint32_t>(paramsBuffer.size()) << 1;
    std::memcpy(paramsBuffer.data(), &sizeHeader, sizeof(sizeHeader));
    paramsBuffer[0x18] = 3;
    paramsBuffer[0x20] = 0x77;

    const auto* customParams = reinterpret_cast<const SceFiosParams*>(paramsBuffer.data());
    Require(sceFiosInitialize(customParams) == FiosOk);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);

    std::array<std::uint8_t, 0x50> readBack{};
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(readBack.data())));
    Require(readBack[0x18] == 3);
    Require(readBack[0x20] == 0x77);
}

void TestRepeatedInitialization() {
    Require(sceFiosIsInitialized(nullptr));

    Require(sceFiosInitialize(nullptr) == FiosOk);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    std::array<std::uint8_t, 0x40> newParams{};
    const std::uint32_t sizeHeader = static_cast<std::uint32_t>(newParams.size()) << 1;
    std::memcpy(newParams.data(), &sizeHeader, sizeof(sizeHeader));
    newParams[0x18] = 1;
    newParams[0x1C] = 0x42;

    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(newParams.data())) == FiosOk);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);

    std::array<std::uint8_t, 0x40> readCopy{};
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(readCopy.data())));
    Require(readCopy[0x18] == 1);
    Require(readCopy[0x1C] == 0x42);

    const auto* badParams = reinterpret_cast<const SceFiosParams*>(1);
    Require(sceFiosInitialize(badParams) == FiosErrorBadPtr);
}

void TestUpdateParametersLifecycle() {
    Require(sceFiosIsInitialized(nullptr));

    std::array<std::uint8_t, 0x40> updatedParams{};
    const std::uint32_t sizeHeader = static_cast<std::uint32_t>(updatedParams.size()) << 1;
    std::memcpy(updatedParams.data(), &sizeHeader, sizeof(sizeHeader));
    updatedParams[0x18] = 2;
    updatedParams[0x1C] = 0x99;

    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(updatedParams.data()));

    std::array<std::uint8_t, 0x40> readCopy{};
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(readCopy.data())));
    Require(readCopy[0x1C] == 0x99);

    const auto* badParams = reinterpret_cast<const SceFiosParams*>(1);
    sceFiosUpdateParameters(badParams);
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(readCopy.data())));
    Require(readCopy[0x1C] == 0x99);
}

void TestSuspendResumeLifecycle() {
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

    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());

    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());
}

void TestPostTerminationCleanup() {
    Require(sceFiosIOFilterAdd(0, TestFilterCallback, reinterpret_cast<void*>(0x1234)) == FiosOk);
    void* filterCb = nullptr;
    void* filterCtx = nullptr;
    Require(sceFiosIOFilterGetInfo(0, &filterCb, &filterCtx) == FiosOk);
    Require(filterCb == reinterpret_cast<void*>(TestFilterCallback));
    Require(filterCtx == reinterpret_cast<void*>(0x1234));

    sceFiosSuspend();
    Require(sceFiosIsSuspended());

    sceFiosTerminate();

    Require(!sceFiosIsInitialized(nullptr));
    std::array<std::uint8_t, 16> dummyOut{};
    Require(!sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(dummyOut.data())));

    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
    Require(sceFiosGetSuspendCount() == 0);
    Require(!sceFiosIsSuspended());

    std::uint8_t req[0x410]{};
    std::uint8_t res[0x410]{};
    Require(sceFiosResolve(nullptr, req, res) == 0);

    filterCb = nullptr;
    filterCtx = nullptr;
    Require(sceFiosIOFilterGetInfo(0, &filterCb, &filterCtx) == FiosErrorBadIndex);

    Require(sceFiosIOFilterAdd(0, TestFilterCallback, reinterpret_cast<void*>(0x5678)) == FiosOk);
    Require(sceFiosIOFilterRemove(0) == FiosOk);

    std::array<std::uint8_t, 16> updateBuffer{};
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(updateBuffer.data()));
    Require(!sceFiosIsInitialized(nullptr));
}

void TestReinitializationLifecycle() {
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosInitialize(nullptr) == FiosOk);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
}

} // namespace

int main() {
    TestUninitializedApiRejection();
    TestParameterValidation();
    TestRepeatedInitialization();
    TestUpdateParametersLifecycle();
    TestSuspendResumeLifecycle();
    TestPostTerminationCleanup();
    TestReinitializationLifecycle();
    return 0;
}
