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

constexpr std::int32_t FilterNotHandled = static_cast<std::int32_t>(0x8082001Du);

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

struct PsarcDescriptorTest {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t workBuffer;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};
static_assert(sizeof(PsarcDescriptorTest) == 0x38);

void AuditFilesystemOverlayAndPathBounds() {
    Require(sceFiosOverlayAdd(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayGetInfo(1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayGetList(nullptr, 1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    std::uint64_t dummyCount = 0;
    Require(sceFiosOverlayGetList(nullptr, 10, &dummyCount) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayModify(1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    OverlayRecordTest badRec{};
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    std::strcpy(badRec.destination, "/app0");
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    badRec.destination[0] = '\0';
    std::strcpy(badRec.source, "/hostapp");
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    std::memset(badRec.destination, 'a', sizeof(badRec.destination));
    std::strcpy(badRec.source, "/hostapp");
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    std::strcpy(badRec.destination, "/app0");
    std::memset(badRec.source, 'b', sizeof(badRec.source));
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    std::strcpy(badRec.source, "/hostapp");
    badRec.type = 4;
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);
    badRec.type = 255;
    Require(sceFiosOverlayAdd(&badRec, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);

    OverlayRecordTest info{};
    Require(sceFiosOverlayGetInfo(0, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayGetInfo(-1, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayGetInfo(999999, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);
    badRec.type = 0;
    Require(sceFiosOverlayModify(999999, &badRec) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(999999) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(0) == SCE_FIOS_ERROR_BAD_OVERLAY);

    OverlayRecordTest rec1{};
    rec1.type = 0;
    rec1.order = 5;
    std::strcpy(rec1.destination, "/app0");
    std::strcpy(rec1.source, "/hostapp/app0");
    std::int32_t id1 = 0;
    Require(sceFiosOverlayAdd(&rec1, &id1) == SCE_FIOS_OK);
    Require(id1 > 0);

    OverlayRecordTest rec2{};
    rec2.type = 1;
    rec2.order = 10;
    std::strcpy(rec2.destination, "/patch");
    std::strcpy(rec2.source, "/hostapp/patch");
    std::int32_t id2 = 0;
    Require(sceFiosOverlayAdd(&rec2, &id2) == SCE_FIOS_OK);
    Require(id2 > id1);

    OverlayRecordTest rec3{};
    rec3.type = 2;
    rec3.order = 20;
    std::strcpy(rec3.destination, "/data");
    std::strcpy(rec3.source, "/hostapp/data");
    std::int32_t id3 = 0;
    Require(sceFiosOverlayAdd(&rec3, &id3) == SCE_FIOS_OK);
    Require(id3 > id2);

    OverlayRecordTest rec4{};
    rec4.type = 3;
    rec4.order = 30;
    std::strcpy(rec4.destination, "/savedata");
    std::strcpy(rec4.source, "/hostapp/savedata");
    std::int32_t id4 = 0;
    Require(sceFiosOverlayAdd(&rec4, &id4) == SCE_FIOS_OK);
    Require(id4 > id3);

    std::uint64_t count = 0;
    Require(sceFiosOverlayGetList(nullptr, 0, &count) == SCE_FIOS_OK);
    Require(count >= 4);

    std::int32_t ids[16] = {};
    Require(sceFiosOverlayGetList(ids, 16, &count) == SCE_FIOS_OK);
    Require(count >= 4);

    bool found1 = false;
    bool found2 = false;
    bool found3 = false;
    bool found4 = false;
    for (std::uint64_t i = 0; i < count && i < 16; ++i) {
        if (ids[i] == id1) found1 = true;
        if (ids[i] == id2) found2 = true;
        if (ids[i] == id3) found3 = true;
        if (ids[i] == id4) found4 = true;
    }
    Require(found1 && found2 && found3 && found4);

    std::memset(&info, 0, sizeof(info));
    Require(sceFiosOverlayGetInfo(id1, &info) == SCE_FIOS_OK);
    Require(info.id == id1);
    Require(info.type == 0);
    Require(info.order == 5);
    Require(info.destinationLength == std::strlen("/app0"));
    Require(info.sourceLength == std::strlen("/hostapp/app0"));
    Require(std::strcmp(info.destination, "/app0") == 0);
    Require(std::strcmp(info.source, "/hostapp/app0") == 0);

    rec2.type = 3;
    rec2.order = 15;
    Require(sceFiosOverlayModify(id2, &rec2) == SCE_FIOS_OK);
    std::memset(&info, 0, sizeof(info));
    Require(sceFiosOverlayGetInfo(id2, &info) == SCE_FIOS_OK);
    Require(info.type == 3);
    Require(info.order == 15);

    Require(sceFiosOverlayRemove(id2) == SCE_FIOS_OK);
    Require(sceFiosOverlayGetInfo(id2, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(id2) == SCE_FIOS_ERROR_BAD_OVERLAY);

    Require(sceFiosOverlayRemove(id1) == SCE_FIOS_OK);
    Require(sceFiosOverlayRemove(id3) == SCE_FIOS_OK);
    Require(sceFiosOverlayRemove(id4) == SCE_FIOS_OK);

    char resolveResult[64] = {};
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, sizeof(resolveResult)) == SCE_FIOS_OK);
    Require(sceFiosOverlayResolveSync(1, "/app0/file.bin", resolveResult, sizeof(resolveResult)) == SCE_FIOS_OK);
    Require(sceFiosOverlayResolveSync(0x100, "/app0/file.bin", resolveResult, sizeof(resolveResult)) == SCE_FIOS_ERROR_BAD_RESOLVE_FLAG);
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, 0x401) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0, "/app0/file.bin", resolveResult, 0x1000) == SCE_FIOS_ERROR_BAD_SIZE);

    char normBuffer[128] = {};
    Require(sceFiosPathNormalize(nullptr, "/app0", sizeof(normBuffer)) == static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR));
    Require(sceFiosPathNormalize(normBuffer, nullptr, sizeof(normBuffer)) == static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR));

    std::size_t normLen = sceFiosPathNormalize(normBuffer, "", sizeof(normBuffer));
    Require(normLen == 2);
    Require(std::strcmp(normBuffer, ".") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "\\app0\\dir\\file", sizeof(normBuffer));
    Require(normLen == std::strlen("/app0/dir/file") + 1);
    Require(std::strcmp(normBuffer, "/app0/dir/file") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "/app0///dir////file", sizeof(normBuffer));
    Require(std::strcmp(normBuffer, "/app0/dir/file") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "/app0/./dir/./file", sizeof(normBuffer));
    Require(std::strcmp(normBuffer, "/app0/dir/file") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "/app0/dir/../data/file", sizeof(normBuffer));
    Require(std::strcmp(normBuffer, "/app0/data/file") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "/app0/dir/", sizeof(normBuffer));
    Require(std::strcmp(normBuffer, "/app0/dir") == 0);

    normLen = sceFiosPathNormalize(normBuffer, "/", sizeof(normBuffer));
    Require(normLen == 2);
    Require(std::strcmp(normBuffer, "/") == 0);

    char tinyBuffer[4] = {};
    normLen = sceFiosPathNormalize(tinyBuffer, "/app0/toolong", sizeof(tinyBuffer));
    Require(normLen == sizeof(tinyBuffer) + 1);
    Require(tinyBuffer[0] == '\0');

    Require(sceFiosPathcmp(nullptr, "/app0") == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathcmp("/app0", nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathcmp("/app0/file", "/app0/file") == 0);
    Require(sceFiosPathcmp("/app0/a", "/app0/b") < 0);
    Require(sceFiosPathcmp("/app0/b", "/app0/a") > 0);

    Require(sceFiosPathncmp(nullptr, "/app0", 5) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathncmp("/app0", nullptr, 5) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathncmp("/app0/same", "/app0/diff", 6) == 0);
    Require(sceFiosPathncmp("/app0/a", "/app0/b", 0) == 0);
    Require(sceFiosPathncmp("/app0/a", "/app0/b", 7) < 0);
}

void AuditArchiveMountsAndHandles() {
    SceFiosSize mountSize = 0;
    Require(sceFiosArchiveGetMountBufferSize(nullptr, "/app0/archive.psarc", &mountSize) == 0);
    Require(sceFiosArchiveGetMountBufferSize(nullptr, nullptr, nullptr) == 0);

    SceFiosFH archive = 0;
    Require(sceFiosArchiveMount(nullptr, &archive, "/app0/archive.psarc", "/mount", nullptr, 0, nullptr) == 0);
    Require(sceFiosArchiveMount(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr) == 0);

    const std::uint32_t orders[] = {0, 1, 10, 100, 0xFFFFFFFFu};
    for (const std::uint32_t order : orders) {
        Require(sceFiosArchiveMountWithOrder(nullptr, &archive, "/app0/archive.psarc", "/mount", nullptr, 0, nullptr, order) == 0);
    }

    const SceFiosFH handles[] = {0, 1, 10, -1, 0x7FFFFFFF};
    for (const SceFiosFH handle : handles) {
        Require(sceFiosArchiveUnmount(nullptr, handle) == 0);
        Require(!sceFiosIsValidHandle(handle));
    }
}

void AuditDecompressionBufferAndThreadBoundaries() {
    sceFiosTerminate();
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);

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

    Require(sceFiosArchiveSetDecompressorThreadCount(10) == 3);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 3);

    sceFiosTerminate();
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);

    params[6] = 0;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    Require(sceFiosArchiveSetDecompressorThreadCount(5) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    sceFiosTerminate();
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);

    alignas(64) std::uint8_t workBuffer[4096] = {};
    PsarcDescriptorTest desc{};
    desc.size = sizeof(PsarcDescriptorTest);
    desc.workBufferSize = sizeof(workBuffer);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(workBuffer);
    desc.flags = 0;
    desc.reserved[0] = 0;
    desc.reserved[1] = 0;
    desc.reserved[2] = 0;

    Require(sceFiosIOFilterPsarcDearchiver(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    desc.workBuffer = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(workBuffer);

    desc.size = 0x30;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.size = sizeof(PsarcDescriptorTest);

    desc.workBufferSize = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.workBufferSize = sizeof(workBuffer);

    desc.workBuffer = reinterpret_cast<std::uint64_t>(workBuffer) + 8;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_ALIGNMENT);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(workBuffer);

    desc.flags = 2;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);
    desc.flags = 0;

    desc.reserved[0] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.reserved[0] = 0;

    desc.reserved[1] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.reserved[1] = 0;

    desc.reserved[2] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.reserved[2] = 0;

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_OK);

    char description[64] = {};
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, nullptr) == FilterNotHandled);
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, description) == SCE_FIOS_OK);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, nullptr) == FilterNotHandled);
    std::uint16_t opSlot = 42;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, &opSlot) == SCE_FIOS_OK);
    Require(opSlot == 42);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 1, nullptr) == SCE_FIOS_OK);

    Require(sceFiosOpSetBuffer(0, nullptr) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpSetBuffer(-1, nullptr) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpSetBuffer(999999, nullptr) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpGetBuffer(0) == nullptr);
    Require(sceFiosOpGetBuffer(-1) == nullptr);
    Require(sceFiosOpGetBuffer(999999) == nullptr);

    alignas(8) std::uint8_t req[0x410] = {};
    alignas(8) std::uint8_t res[0x410] = {};
    std::memset(req, 0x3c, sizeof(req));

    Require(sceFiosResolveSync(nullptr, req, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, nullptr, res) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, req, res) == SCE_FIOS_OK);
    Require(std::memcmp(req, res, sizeof(req)) == 0);

    Require(sceFiosResolve(nullptr, req, res) == 0);

    std::memset(res, 0, sizeof(res));
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    const SceFiosOp resolveOp = sceFiosResolve(nullptr, req, res);
    Require(resolveOp != 0);
    Require(sceFiosOpSyncWait(resolveOp) == SCE_FIOS_OK);
    Require(std::memcmp(req, res, sizeof(req)) == 0);
    sceFiosOpDelete(resolveOp);

    sceFiosTerminate();
}

} // namespace

int main() {
    AuditFilesystemOverlayAndPathBounds();
    AuditArchiveMountsAndHandles();
    AuditDecompressionBufferAndThreadBoundaries();
    return 0;
}
