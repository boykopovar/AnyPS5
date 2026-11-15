#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location loc = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", loc.line());
        std::fflush(stderr);
        std::abort();
    }
}

struct OverlayRecordTest {
    std::uint8_t type;
    std::uint8_t order;
    std::uint16_t destinationLength;
    std::uint16_t sourceLength;
    std::uint16_t reserved;
    std::int32_t processId;
    std::int32_t id;
    char destination[292];
    char source[292];
};

static_assert(sizeof(OverlayRecordTest) == 0x258);

void TestArchiveProbedExports() {
    Require(sceFiosArchiveGetMountBufferSize(nullptr, "/app0/test.psarc", nullptr) == 0);
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, "/app0/test.psarc", nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    Require(sceFiosArchiveMount(nullptr, nullptr, "/app0/test.psarc", "/mount", nullptr, 0, nullptr) == 0);
    Require(sceFiosArchiveMountSync(nullptr, nullptr, "/app0/test.psarc", "/mount", nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    Require(sceFiosArchiveMountWithOrder(nullptr, nullptr, "/app0/test.psarc", "/mount", nullptr, 0, nullptr, 10) == 0);
    Require(sceFiosArchiveMountWithOrderSync(nullptr, nullptr, "/app0/test.psarc", "/mount", nullptr, 0, nullptr, 10) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    const SceFiosFH testHandles[] = { 0, 1, 100, -1, 999999 };
    for (const SceFiosFH handle : testHandles) {
        Require(sceFiosArchiveUnmount(nullptr, handle) == 0);
        Require(sceFiosArchiveUnmountSync(nullptr, handle) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    }
}

void TestOverlayPriorityAndOrderingBounds() {
    Require(sceFiosOverlayAdd(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayGetInfo(0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayGetList(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    std::uint64_t dummyCount = 0;
    Require(sceFiosOverlayGetList(nullptr, 10, &dummyCount) == SCE_FIOS_ERROR_BAD_PTR);

    OverlayRecordTest badRec{};
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    badRec.type = 4;
    std::strcpy(badRec.destination, "/app0/dest");
    std::strcpy(badRec.source, "/app0/src");
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);

    badRec.type = 0;
    Require(sceFiosOverlayModify(999999, &badRec) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayModify(999999, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayRemove(999999) == SCE_FIOS_ERROR_BAD_OVERLAY);

    OverlayRecordTest rec1{};
    rec1.type = 0;
    rec1.order = 10;
    std::strcpy(rec1.destination, "/app0/data");
    std::strcpy(rec1.source, "/hostapp/data");
    std::int32_t id1 = 0;
    Require(sceFiosOverlayAdd(&rec1, &id1) == SCE_FIOS_OK);
    Require(id1 > 0);

    OverlayRecordTest rec2{};
    rec2.type = 1;
    rec2.order = 20;
    std::strcpy(rec2.destination, "/app0/patch");
    std::strcpy(rec2.source, "/hostapp/patch");
    std::int32_t id2 = 0;
    Require(sceFiosOverlayAdd(&rec2, &id2) == SCE_FIOS_OK);
    Require(id2 > id1);

    std::uint64_t count = 0;
    Require(sceFiosOverlayGetList(nullptr, 0, &count) == SCE_FIOS_OK);
    Require(count >= 2);

    std::int32_t listedIds[16] = {};
    Require(sceFiosOverlayGetList(listedIds, 16, &count) == SCE_FIOS_OK);
    Require(count >= 2);
    bool foundId1 = false;
    bool foundId2 = false;
    for (std::uint64_t i = 0; i < count && i < 16; ++i) {
        if (listedIds[i] == id1) foundId1 = true;
        if (listedIds[i] == id2) foundId2 = true;
    }
    Require(foundId1 && foundId2);

    OverlayRecordTest info{};
    Require(sceFiosOverlayGetInfo(id1, &info) == SCE_FIOS_OK);
    Require(info.id == id1);
    Require(info.type == 0);
    Require(info.order == 10);
    Require(std::strcmp(info.destination, "/app0/data") == 0);
    Require(std::strcmp(info.source, "/hostapp/data") == 0);
    Require(info.destinationLength == std::strlen("/app0/data"));
    Require(info.sourceLength == std::strlen("/hostapp/data"));

    rec1.order = 15;
    Require(sceFiosOverlayModify(id1, &rec1) == SCE_FIOS_OK);
    std::memset(&info, 0, sizeof(info));
    Require(sceFiosOverlayGetInfo(id1, &info) == SCE_FIOS_OK);
    Require(info.order == 15);

    Require(sceFiosOverlayRemove(id1) == SCE_FIOS_OK);
    Require(sceFiosOverlayGetInfo(id1, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(id1) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(id2) == SCE_FIOS_OK);

    char resolveResult[1024] = {};
    Require(sceFiosOverlayResolveSync(0x100, "/app0/file.bin", resolveResult, 0x400) == SCE_FIOS_ERROR_BAD_RESOLVE_FLAG);
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, 0x400) == SCE_FIOS_OK);
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, 0x401) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, 0x800) == SCE_FIOS_ERROR_BAD_SIZE);
}

void TestDecompressorThreadLimits() {
    std::uint32_t params[16] = {};
    params[0] = static_cast<std::uint32_t>(sizeof(params) << 1);
    params[6] = 3;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);

    Require(sceFiosArchiveSetDecompressorThreadCount(1) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);

    Require(sceFiosArchiveSetDecompressorThreadCount(0) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);

    Require(sceFiosArchiveSetDecompressorThreadCount(2) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    Require(sceFiosArchiveSetDecompressorThreadCount(5) == 3);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);

    sceFiosTerminate();
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
}

} // namespace

int main() {
    TestArchiveProbedExports();
    TestOverlayPriorityAndOrderingBounds();
    TestDecompressorThreadLimits();
    return 0;
}
