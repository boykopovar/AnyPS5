#include "prx/libSceFios2/include/Api.hpp"
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

void TestPathNormalize() {
    char buffer[256]{};
    Require(sceFiosPathNormalize(nullptr, "/app0/test", 32) == static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR));
    Require(sceFiosPathNormalize(buffer, nullptr, sizeof(buffer)) == static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR));

    Require(sceFiosPathNormalize(buffer, "/app0/eboot.bin", sizeof(buffer)) == 16);
    Require(std::strcmp(buffer, "/app0/eboot.bin") == 0);

    Require(sceFiosPathNormalize(buffer, "\\app0\\data\\game.pkg", sizeof(buffer)) == 20);
    Require(std::strcmp(buffer, "/app0/data/game.pkg") == 0);

    Require(sceFiosPathNormalize(buffer, "///app0///data///sub///", sizeof(buffer)) == 15);
    Require(std::strcmp(buffer, "/app0/data/sub") == 0);

    Require(sceFiosPathNormalize(buffer, "/app0/./data/./file.dat", sizeof(buffer)) == 20);
    Require(std::strcmp(buffer, "/app0/data/file.dat") == 0);

    Require(sceFiosPathNormalize(buffer, "/app0/data/../extra/file.dat", sizeof(buffer)) == 21);
    Require(std::strcmp(buffer, "/app0/extra/file.dat") == 0);

    Require(sceFiosPathNormalize(buffer, "app0/sub/../../app0", sizeof(buffer)) == 5);
    Require(std::strcmp(buffer, "app0") == 0);

    Require(sceFiosPathNormalize(buffer, "/", sizeof(buffer)) == 2);
    Require(std::strcmp(buffer, "/") == 0);

    Require(sceFiosPathNormalize(buffer, "////", sizeof(buffer)) == 2);
    Require(std::strcmp(buffer, "/") == 0);

    Require(sceFiosPathNormalize(buffer, "", sizeof(buffer)) == 2);
    Require(std::strcmp(buffer, ".") == 0);

    Require(sceFiosPathNormalize(buffer, "/app0/file", 0) == 1);

    char smallBuf[5]{};
    Require(sceFiosPathNormalize(smallBuf, "/app0/file", sizeof(smallBuf)) == 6);
    Require(smallBuf[0] == '\0');

    char exactBuf[11]{};
    Require(sceFiosPathNormalize(exactBuf, "/app0/file", sizeof(exactBuf)) == 11);
    Require(std::strcmp(exactBuf, "/app0/file") == 0);

    std::string longPath(SCE_FIOS_PATH_MAX + 1, 'a');
    Require(sceFiosPathNormalize(buffer, longPath.c_str(), sizeof(buffer)) == static_cast<std::size_t>(SCE_FIOS_ERROR_PATH_TOO_LONG));
}

void TestPathComparison() {
    Require(sceFiosPathcmp(nullptr, "/app0") == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathcmp("/app0", nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathcmp(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    Require(sceFiosPathcmp("/app0/data", "/app0/data") == 0);
    Require(sceFiosPathcmp("/app0/a", "/app0/b") < 0);
    Require(sceFiosPathcmp("/app0/b", "/app0/a") > 0);
    Require(sceFiosPathcmp("/app0", "/app0/sub") < 0);
    Require(sceFiosPathcmp("/app0/sub", "/app0") > 0);

    std::string longPath(SCE_FIOS_PATH_MAX + 1, 'x');
    Require(sceFiosPathcmp(longPath.c_str(), "/app0") == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(sceFiosPathcmp("/app0", longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    Require(sceFiosPathncmp(nullptr, "/app0", 4) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathncmp("/app0", nullptr, 4) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosPathncmp(nullptr, nullptr, 4) == SCE_FIOS_ERROR_BAD_PTR);

    Require(sceFiosPathncmp("/app0/a", "/app0/b", 0) == 0);
    Require(sceFiosPathncmp("/app0/a", "/app0/b", 6) == 0);
    Require(sceFiosPathncmp("/app0/a", "/app0/b", 7) < 0);
    Require(sceFiosPathncmp("/app0/b", "/app0/a", 7) > 0);
    Require(sceFiosPathncmp("/app0", "/app0", 100) == 0);
    Require(sceFiosPathncmp("/app0/a", "/app0/b", 100) < 0);
}

void TestOverlayAndResolutionBounds() {
    char result[128]{};
    Require(sceFiosOverlayResolveSync(0, "/app0", "/app0/eboot.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(64, "/app0", "/app0/eboot.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, "/app0", "/app0/eboot.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, "/app0", "/app0/eboot.bin", result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x1000, "/app0", "/app0/eboot.bin", result) == SCE_FIOS_ERROR_BAD_SIZE);

    std::uint8_t dummy[0x410]{};
    Require(sceFiosResolveSync(nullptr, nullptr, dummy) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, dummy, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    std::uint8_t req[0x410]{};
    std::uint8_t res[0x410]{};
    req[0] = 0x42;
    req[0x40f] = 0x99;
    Require(sceFiosResolveSync(nullptr, req, res) == SCE_FIOS_OK);
    Require(res[0] == 0x42 && res[0x40f] == 0x99);
    Require(std::memcmp(req, res, 0x410) == 0);

    Require(sceFiosResolve(nullptr, req, res) == 0);
}

void TestArchiveMountParameterValidation() {
    SceFiosSize mountSize = -1;
    Require(sceFiosArchiveGetMountBufferSize(nullptr, "/app0/archive.psarc", &mountSize) == 0);
    Require(mountSize == -1);
    Require(sceFiosArchiveGetMountBufferSize(nullptr, nullptr, nullptr) == 0);

    mountSize = -1;
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, "/app0/archive.psarc", &mountSize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, nullptr, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    SceFiosFH archiveHandle = -1;
    Require(sceFiosArchiveMount(nullptr, &archiveHandle, "/app0/archive.psarc", "/app0", nullptr, 0, nullptr) == 0);
    Require(archiveHandle == -1);
    Require(sceFiosArchiveMount(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr) == 0);

    Require(sceFiosArchiveMountSync(nullptr, &archiveHandle, "/app0/archive.psarc", "/app0", nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveMountSync(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    Require(sceFiosArchiveMountWithOrder(nullptr, &archiveHandle, "/app0/archive.psarc", "/app0", nullptr, 0, nullptr, 1) == 0);
    Require(sceFiosArchiveMountWithOrder(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0) == 0);

    Require(sceFiosArchiveMountWithOrderSync(nullptr, &archiveHandle, "/app0/archive.psarc", "/app0", nullptr, 0, nullptr, 1) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveMountWithOrderSync(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0) == SCE_FIOS_ERROR_UNIMPLEMENTED);

    Require(sceFiosArchiveUnmount(nullptr, 0) == 0);
    Require(sceFiosArchiveUnmount(nullptr, 1) == 0);
    Require(sceFiosArchiveUnmount(nullptr, -1) == 0);

    Require(sceFiosArchiveUnmountSync(nullptr, 0) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveUnmountSync(nullptr, 1) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveUnmountSync(nullptr, -1) == SCE_FIOS_ERROR_UNIMPLEMENTED);
}

void TestArchiveDecompressorThreadBounds() {
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
    Require(sceFiosArchiveSetDecompressorThreadCount(0) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);
    Require(sceFiosArchiveSetDecompressorThreadCount(5) == 0);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
}

void TestInitializedState() {
    std::uint32_t params[16]{};
    params[0] = (sizeof(params) << 1) | 0u;
    params[3] = 0xfeedbeef;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    Require(sceFiosArchiveSetDecompressorThreadCount(0) == 1);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 1);
    Require(sceFiosArchiveSetDecompressorThreadCount(2) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    Require(sceFiosArchiveSetDecompressorThreadCount(10) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);

    std::uint8_t req[0x410]{};
    std::uint8_t res[0x410]{};
    req[0] = 0x77;
    req[0x400] = 0x88;
    const SceFiosOp op = sceFiosResolve(nullptr, req, res);
    Require(op > 0);
    Require(sceFiosOpSyncWait(op) == SCE_FIOS_OK);
    Require(res[0] == 0x77 && res[0x400] == 0x88);
    sceFiosOpDelete(op);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
}

} // namespace

int main() {
    TestPathNormalize();
    TestPathComparison();
    TestOverlayAndResolutionBounds();
    TestArchiveMountParameterValidation();
    TestArchiveDecompressorThreadBounds();
    TestInitializedState();
    return 0;
}
