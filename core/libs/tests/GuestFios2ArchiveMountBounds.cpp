#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct PsarcDescriptorTest {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t workBuffer;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};

static_assert(sizeof(PsarcDescriptorTest) == 0x38);

void TestDecompressorThreadBounds() {
    Require(sceFiosArchiveGetDecompressorThreadCount() >= 1);

    Require(sceFiosArchiveSetDecompressorThreadCount(0) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);

    Require(sceFiosArchiveSetDecompressorThreadCount(1) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);

    Require(sceFiosArchiveSetDecompressorThreadCount(2) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    const int clampedHigh = sceFiosArchiveSetDecompressorThreadCount(255);
    Require(clampedHigh <= 3 && clampedHigh >= 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == clampedHigh);
}

void TestArchiveGetMountBufferSizeBounds() {
    SceFiosSize sizeOut = 0;
    const SceFiosOp op1 = sceFiosArchiveGetMountBufferSize(nullptr, nullptr, nullptr);
    Require(op1 == 0);

    const SceFiosOp op2 = sceFiosArchiveGetMountBufferSize(nullptr, "/app0/archive.psarc", &sizeOut);
    Require(op2 == 0);

    const std::string longPath(1025, 'a');
    const SceFiosOp op3 = sceFiosArchiveGetMountBufferSize(nullptr, longPath.c_str(), &sizeOut);
    Require(op3 == 0);

    const SceFiosOp op4 = sceFiosArchiveGetMountBufferSize(nullptr, "", &sizeOut);
    Require(op4 == 0);

    SceFiosSize canarySize = 0x12345678;
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, nullptr, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, nullptr, &canarySize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canarySize == 0x12345678);

    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, "/app0/game_data.psarc", &canarySize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canarySize == 0x12345678);

    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, "", &canarySize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canarySize == 0x12345678);

    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, longPath.c_str(), &canarySize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canarySize == 0x12345678);
}

void TestArchiveMountBounds() {
    SceFiosFH canaryFH = 0x76543210;
    char dummyBuf[512] = {};

    const SceFiosOp mOp1 = sceFiosArchiveMount(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    Require(mOp1 == 0);

    const SceFiosOp mOp2 = sceFiosArchiveMount(nullptr, &canaryFH, "/app0/archive.psarc", "/app0/mounted", dummyBuf, sizeof(dummyBuf), nullptr);
    Require(mOp2 == 0);
    Require(canaryFH == 0x76543210);

    const std::string longPath(1025, 'b');
    const SceFiosOp mOp3 = sceFiosArchiveMount(nullptr, &canaryFH, longPath.c_str(), "/app0/m", dummyBuf, sizeof(dummyBuf), nullptr);
    Require(mOp3 == 0);
    Require(canaryFH == 0x76543210);

    const SceFiosOp mOp4 = sceFiosArchiveMount(nullptr, &canaryFH, "/app0/archive.psarc", longPath.c_str(), dummyBuf, sizeof(dummyBuf), nullptr);
    Require(mOp4 == 0);
    Require(canaryFH == 0x76543210);

    canaryFH = 0x55AA55AA;
    Require(sceFiosArchiveMountSync(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, nullptr, nullptr, nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, "/app0/archive.psarc", "/app0/mounted", dummyBuf, sizeof(dummyBuf), nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, "", "", dummyBuf, sizeof(dummyBuf), nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, longPath.c_str(), "/app0/mounted", dummyBuf, sizeof(dummyBuf), nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, "/app0/archive.psarc", longPath.c_str(), dummyBuf, sizeof(dummyBuf), nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, "/app0/archive.psarc", "/app0/mounted", nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);

    Require(sceFiosArchiveMountSync(nullptr, &canaryFH, "/app0/archive.psarc", "/app0/mounted", dummyBuf, 0xFFFFFFFFFFFFFFFFULL, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(canaryFH == 0x55AA55AA);
}

void TestArchiveMountWithOrderBounds() {
    SceFiosFH canaryFH = 0x11223344;
    char dummyBuf[256] = {};
    const std::uint32_t testOrders[] = { 0, 1, 10, 0x7f, 0x80, 0xff, 0xffff, 0xffffffff };

    for (const std::uint32_t order : testOrders) {
        const SceFiosOp op = sceFiosArchiveMountWithOrder(nullptr, &canaryFH, "/app0/arc.psarc", "/app0/mnt", dummyBuf, sizeof(dummyBuf), nullptr, order);
        Require(op == 0);
        Require(canaryFH == 0x11223344);

        Require(sceFiosArchiveMountWithOrderSync(nullptr, &canaryFH, "/app0/arc.psarc", "/app0/mnt", dummyBuf, sizeof(dummyBuf), nullptr, order) == SCE_FIOS_ERROR_UNIMPLEMENTED);
        Require(canaryFH == 0x11223344);

        Require(sceFiosArchiveMountWithOrderSync(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, order) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    }
}

void TestArchiveUnmountBounds() {
    const SceFiosFH testHandles[] = { 0, 1, 2, -1, -100, 0x7fffffff, static_cast<SceFiosFH>(0x80000000u) };
    for (const SceFiosFH fh : testHandles) {
        const SceFiosOp uOp = sceFiosArchiveUnmount(nullptr, fh);
        Require(uOp == 0);
        Require(sceFiosArchiveUnmountSync(nullptr, fh) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    }
}

void TestPsarcDearchiverFilterBounds() {
    alignas(64) char workBuf[256] = {};
    PsarcDescriptorTest desc{};
    desc.size = sizeof(PsarcDescriptorTest);
    desc.workBufferSize = sizeof(workBuf);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(workBuf);
    desc.flags = 0;
    desc.reserved[0] = 0;
    desc.reserved[1] = 0;
    desc.reserved[2] = 0;

    Require(sceFiosIOFilterPsarcDearchiver(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    PsarcDescriptorTest badDesc = desc;
    badDesc.workBuffer = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    badDesc = desc;
    badDesc.size = 0x20;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    badDesc = desc;
    badDesc.size = 0x40;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    badDesc = desc;
    badDesc.workBufferSize = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    badDesc = desc;
    badDesc.workBuffer = reinterpret_cast<std::uint64_t>(workBuf) + 1;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_ALIGNMENT);

    badDesc = desc;
    badDesc.flags = 2;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);

    badDesc = desc;
    badDesc.reserved[0] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    badDesc = desc;
    badDesc.reserved[1] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    badDesc = desc;
    badDesc.reserved[2] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&badDesc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_OK);

    desc.flags = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_OK);
    desc.flags = 0;

    char descBuf[128] = {};
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, nullptr) == static_cast<std::int32_t>(0x8082001Du));
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, descBuf) == SCE_FIOS_OK);
    Require(std::strcmp(descBuf, "description not implemented") == 0);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, nullptr) == static_cast<std::int32_t>(0x8082001Du));
    char dummyData[16] = {};
    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, dummyData) == SCE_FIOS_OK);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 3, dummyData) == static_cast<std::int32_t>(0x8082001Du));
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 99, dummyData) == static_cast<std::int32_t>(0x8082001Du));

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 1, nullptr) == SCE_FIOS_OK);
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 1, nullptr) == SCE_FIOS_OK);

    PsarcDescriptorTest invalidDesc = desc;
    invalidDesc.size = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&invalidDesc, 1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, descBuf) == SCE_FIOS_ERROR_BAD_PTR);
}

void TestCustomParamInitialization() {
    std::uint32_t customParams[16] = {};
    customParams[0] = static_cast<std::uint32_t>(sizeof(customParams)) << 1;
    auto* bytes = reinterpret_cast<std::uint8_t*>(customParams);
    bytes[0x18] = 3;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(customParams)) == SCE_FIOS_OK);
    Require(sceFiosArchiveSetDecompressorThreadCount(3) == 3);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);
    Require(sceFiosArchiveSetDecompressorThreadCount(100) == 3);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);
}

} // namespace

int main() {
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);

    std::uint32_t defaultParams[16] = {};
    defaultParams[0] = static_cast<std::uint32_t>(sizeof(defaultParams)) << 1;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(defaultParams)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    TestDecompressorThreadBounds();
    TestArchiveGetMountBufferSizeBounds();
    TestArchiveMountBounds();
    TestArchiveMountWithOrderBounds();
    TestArchiveUnmountBounds();
    TestPsarcDearchiverFilterBounds();

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    TestCustomParamInitialization();

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    return 0;
}
