#include "prx/libSceFios2/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

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

    std::string longPath(SCE_FIOS_PATH_MAX + 1, 'a');
    Require(sceFiosPathNormalize(buffer, longPath.c_str(), sizeof(buffer)) == static_cast<std::size_t>(SCE_FIOS_ERROR_PATH_TOO_LONG));
}

void TestOverlayAndResolutionBounds() {
    char result[128]{};
    Require(sceFiosOverlayResolveSync(0, nullptr, result, 0x400) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayResolveSync(0, "/app0", nullptr, 0x400) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayResolveSync(0, "/app0", result, 0x401) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0, "/app0", result, 0x1000) == SCE_FIOS_ERROR_BAD_SIZE);

    std::uint8_t dummy[0x410]{};
    Require(sceFiosResolveSync(nullptr, nullptr, dummy) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, dummy, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
}

}

int main() {
    TestPathNormalize();
    TestOverlayAndResolutionBounds();
    return 0;
}
