#include "prx/libSceFios2/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>

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

void TestOpAttributeSnapshots() {
    Require(sceFiosInitialize(nullptr) == SCE_FIOS_OK);

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = "fios2_attr_snapshot_" + std::to_string(stamp);
    Require(std::filesystem::create_directory(directory));
    const std::string file = (directory / "attr_probe.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out.write("snapshot-probe-data", 19);
    }

    SceFiosOpAttr globalA{};
    globalA.userTag = 0xAAAAu;
    globalA.priority = 1;

    SceFiosOpAttr threadB{};
    threadB.userTag = 0xBBBBu;
    threadB.priority = 2;

    SceFiosOpAttr explicitC{};
    explicitC.userTag = 0xCCCCu;
    explicitC.priority = 3;

    SceFiosOpAttr updatedDefault{};
    updatedDefault.userTag = 0xDDDDu;
    updatedDefault.priority = 4;

    SceFiosOpAttr readBack{};

    sceFiosSetGlobalDefaultOpAttr(&globalA);
    Require(sceFiosGetGlobalDefaultOpAttr(&readBack));
    Require(readBack.userTag == 0xAAAAu);
    Require(readBack.priority == 1);

    sceFiosSetThreadDefaultOpAttr(&threadB);
    Require(sceFiosGetThreadDefaultOpAttr(&readBack));
    Require(readBack.userTag == 0xBBBBu);
    Require(readBack.priority == 2);

    bool exists = false;
    const SceFiosOp opC = sceFiosFileExists(&explicitC, file.c_str(), &exists);
    Require(opC > 0);
    Require(sceFiosOpWait(opC) == SCE_FIOS_OK);
    Require(exists);

    const SceFiosOp opB = sceFiosFileExists(nullptr, file.c_str(), &exists);
    Require(opB > 0);
    Require(sceFiosOpWait(opB) == SCE_FIOS_OK);
    Require(exists);

    sceFiosSetThreadDefaultOpAttr(nullptr);
    const SceFiosOp opA = sceFiosFileExists(nullptr, file.c_str(), &exists);
    Require(opA > 0);
    Require(sceFiosOpWait(opA) == SCE_FIOS_OK);
    Require(exists);

    const SceFiosOpAttr* attrC = sceFiosOpGetAttr(opC);
    Require(attrC != nullptr);
    Require(attrC->userTag == 0xCCCCu);
    Require(attrC->priority == 3);

    const SceFiosOpAttr* attrB = sceFiosOpGetAttr(opB);
    Require(attrB != nullptr);
    Require(attrB->userTag == 0xBBBBu);
    Require(attrB->priority == 2);

    const SceFiosOpAttr* attrA = sceFiosOpGetAttr(opA);
    Require(attrA != nullptr);
    Require(attrA->userTag == 0xAAAAu);
    Require(attrA->priority == 1);

    explicitC.userTag = 0x1111u;
    explicitC.priority = 9;
    threadB.userTag = 0x2222u;
    threadB.priority = 9;
    globalA.userTag = 0x3333u;
    globalA.priority = 9;

    Require(sceFiosOpGetAttr(opC)->userTag == 0xCCCCu);
    Require(sceFiosOpGetAttr(opC)->priority == 3);
    Require(sceFiosOpGetAttr(opB)->userTag == 0xBBBBu);
    Require(sceFiosOpGetAttr(opB)->priority == 2);
    Require(sceFiosOpGetAttr(opA)->userTag == 0xAAAAu);
    Require(sceFiosOpGetAttr(opA)->priority == 1);

    sceFiosSetGlobalDefaultOpAttr(&updatedDefault);
    sceFiosSetThreadDefaultOpAttr(&updatedDefault);

    Require(sceFiosOpGetAttr(opC)->userTag == 0xCCCCu);
    Require(sceFiosOpGetAttr(opC)->priority == 3);
    Require(sceFiosOpGetAttr(opB)->userTag == 0xBBBBu);
    Require(sceFiosOpGetAttr(opB)->priority == 2);
    Require(sceFiosOpGetAttr(opA)->userTag == 0xAAAAu);
    Require(sceFiosOpGetAttr(opA)->priority == 1);

    const SceFiosOp opFresh = sceFiosFileExists(nullptr, file.c_str(), &exists);
    Require(opFresh > 0);
    Require(sceFiosOpWait(opFresh) == SCE_FIOS_OK);
    Require(exists);

    const SceFiosOpAttr* attrFresh = sceFiosOpGetAttr(opFresh);
    Require(attrFresh != nullptr);
    Require(attrFresh->userTag == 0xDDDDu);
    Require(attrFresh->priority == 4);

    sceFiosOpDelete(opC);
    sceFiosOpDelete(opB);
    sceFiosOpDelete(opA);
    sceFiosOpDelete(opFresh);

    sceFiosSetGlobalDefaultOpAttr(nullptr);
    sceFiosSetThreadDefaultOpAttr(nullptr);

    Require(!sceFiosGetGlobalDefaultOpAttr(&readBack));
    Require(!sceFiosGetThreadDefaultOpAttr(&readBack));

    sceFiosTerminate();
    Require(std::filesystem::remove(file));
    Require(std::filesystem::remove(directory));
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
    TestOpAttributeSnapshots();
    return 0;
}
