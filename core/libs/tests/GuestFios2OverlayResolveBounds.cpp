#include "prx/libSceFios2/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct OverlayRecordData {
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

static_assert(sizeof(OverlayRecordData) == 0x258);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCapacityBoundsUnregistered() {
    char result[1024]{};
    const char* root = "/app0";
    const char* path = "/app0/eboot.bin";

    Require(sceFiosOverlayResolveSync(0, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(1, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(2, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(64, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(256, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(512, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x3ff, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, root, path, result) == SCE_FIOS_ERROR_BAD_OVERLAY);

    Require(sceFiosOverlayResolveSync(0x401, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x402, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x800, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x1000, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x10000, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x7fffffff, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0xffffffff, root, path, result) == SCE_FIOS_ERROR_BAD_SIZE);
}

void TestPointerVariations() {
    char result[512]{};
    std::memset(result, 0x5a, sizeof(result));

    Require(sceFiosOverlayResolveSync(0x100, nullptr, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, "/app0", nullptr, nullptr) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x200, nullptr, "/app0/test", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, "/app0", "/app0/test", nullptr) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, "", "", result) == SCE_FIOS_ERROR_BAD_OVERLAY);

    Require(sceFiosOverlayResolveSync(0x401, nullptr, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x500, "/app0", nullptr, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x600, nullptr, "/app0/test", result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x700, "/app0", "/app0/test", nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x800, "", "", result) == SCE_FIOS_ERROR_BAD_SIZE);

    for (std::size_t i = 0; i < sizeof(result); ++i) {
        Require(static_cast<std::uint8_t>(result[i]) == 0x5a);
    }
}

void TestOverlayRegistrationBounds() {
    OverlayRecordData overlay{};
    overlay.type = 0;
    overlay.order = 1;
    const char dst[] = "/app0";
    const char src[] = "/hostapp";
    std::memcpy(overlay.destination, dst, sizeof(dst));
    std::memcpy(overlay.source, src, sizeof(src));

    std::int32_t overlayId = 0;
    Require(sceFiosOverlayAdd(&overlay, &overlayId) == SCE_FIOS_OK);
    Require(overlayId > 0);

    OverlayRecordData info{};
    Require(sceFiosOverlayGetInfo(overlayId, &info) == SCE_FIOS_OK);
    Require(info.id == overlayId);

    char result[512]{};
    Require(sceFiosOverlayResolveSync(0, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x200, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x1000, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_SIZE);

    const char modDst[] = "/app0/mod";
    std::memcpy(overlay.destination, modDst, sizeof(modDst));
    Require(sceFiosOverlayModify(overlayId, &overlay) == SCE_FIOS_OK);

    Require(sceFiosOverlayResolveSync(0x400, modDst, "/app0/mod/file", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, modDst, "/app0/mod/file", result) == SCE_FIOS_ERROR_BAD_SIZE);

    Require(sceFiosOverlayRemove(overlayId) == SCE_FIOS_OK);
    Require(sceFiosOverlayGetInfo(overlayId, &info) == SCE_FIOS_ERROR_BAD_OVERLAY);

    Require(sceFiosOverlayResolveSync(0x400, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, dst, "/app0/data.bin", result) == SCE_FIOS_ERROR_BAD_SIZE);
}

void TestInitializationLifecycle() {
    char result[256]{};
    Require(sceFiosOverlayResolveSync(0x400, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_SIZE);

    std::uint32_t params[16]{};
    params[0] = (sizeof(params) << 1) | 0u;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    Require(sceFiosOverlayResolveSync(0, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x400, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0x800, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_SIZE);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    Require(sceFiosOverlayResolveSync(0x400, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayResolveSync(0x401, "/app0", "/app0/file", result) == SCE_FIOS_ERROR_BAD_SIZE);
}

} // namespace

int main() {
    TestCapacityBoundsUnregistered();
    TestPointerVariations();
    TestOverlayRegistrationBounds();
    TestInitializationLifecycle();
    return 0;
}
