#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    Require(!_ZN7WebCore22EmptyFrameLoaderClient26shouldUseCredentialStorageEPNS_14DocumentLoaderEm(nullptr, nullptr, 0));
    Require(!_ZN7WebCore22EmptyFrameLoaderClient26shouldUseCredentialStorageEPNS_14DocumentLoaderEm(nullptr, nullptr, UINT64_MAX));
    std::puts("EmptyFrameLoaderClient credential storage tests passed");
    return 0;
}
