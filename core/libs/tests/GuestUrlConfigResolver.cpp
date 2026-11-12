#include "prx/libSceUrlConfigResolver/libSceUrlConfigResolver.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidArgument() {
    return static_cast<int>(0x80BA8080u);
}

void CheckAppConfigWithoutKey(std::size_t capacity, std::uint32_t size) {
    std::vector<unsigned char> buffer(capacity, 0x5A);
    std::uint32_t length = 0xCAFEF00Du;
    Require(sceUrlConfigResolverGetAppConfig(nullptr, 7, buffer.data(), size, &length) == invalidArgument());
    for (std::size_t i = 0; i < buffer.size(); ++i) Require(buffer[i] == (i < size ? 0 : 0x5A));
    Require(length == 0);
}

}

int main() {
    for (int call = 0; call < 2; ++call) {
        Require(sceUrlConfigResolverUpdate(nullptr) == invalidArgument());
        errno = 71;
        bool threwUpdate = false;
        try {
            sceUrlConfigResolverUpdate(reinterpret_cast<const char*>(0x10));
        } catch (const std::invalid_argument&) {
            threwUpdate = true;
        }
        Require(threwUpdate);
        Require(errno == 71);
        Require(sceUrlConfigResolverGetJscHeapSizeSoftLimit(nullptr) == invalidArgument());
        errno = 71;
        bool threwLimit = false;
        try {
            sceUrlConfigResolverGetJscHeapSizeSoftLimit(reinterpret_cast<std::uint32_t*>(0x10));
        } catch (const std::invalid_argument&) {
            threwLimit = true;
        }
        Require(threwLimit);
        Require(errno == 71);
        Require(!sceUrlConfigResolverIsUpdated());
        CheckAppConfigWithoutKey(64, 48);
        CheckAppConfigWithoutKey(16, 0);
        CheckAppConfigWithoutKey(9000, 8195);
        Require(sceUrlConfigResolverGetAppConfig(nullptr, 0, nullptr, 48, nullptr) == invalidArgument());
    }
    return 0;
}
