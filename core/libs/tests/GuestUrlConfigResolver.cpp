#include "prx/libSceUrlConfigResolver/libSceUrlConfigResolver.hpp"

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

template<typename TCall>
void RequireInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    std::abort();
}

void CheckAppConfigWithoutKey(std::size_t capacity, std::uint32_t size, std::uint32_t* lengthOut) {
    std::vector<unsigned char> buffer(capacity, 0x5A);
    Require(sceUrlConfigResolverGetAppConfig(nullptr, 7, buffer.data(), size, lengthOut) == invalidArgument());
    for (std::size_t i = 0; i < buffer.size(); ++i) Require(buffer[i] == (i < size ? 0 : 0x5A));
}

}

int main() {
    for (int call = 0; call < 2; ++call) {
        Require(sceUrlConfigResolverUpdate(nullptr) == invalidArgument());
        Require(sceUrlConfigResolverGetJscHeapSizeSoftLimit(nullptr) == invalidArgument());
        Require(!sceUrlConfigResolverIsUpdated());

        for (const auto [capacity, size] : {std::pair<std::size_t, std::uint32_t>{64, 48},
                                            std::pair<std::size_t, std::uint32_t>{16, 0},
                                            std::pair<std::size_t, std::uint32_t>{4100, 4096},
                                            std::pair<std::size_t, std::uint32_t>{9000, 8195}}) {
            std::uint32_t length = 0xCAFEF00Du;
            CheckAppConfigWithoutKey(capacity, size, &length);
            Require(length == 0);
            CheckAppConfigWithoutKey(capacity, size, nullptr);
        }

        std::uint32_t length = 0xCAFEF00Du;
        Require(sceUrlConfigResolverGetAppConfig(nullptr, 0, nullptr, 48, &length) == invalidArgument());
        Require(length == 0);
        Require(sceUrlConfigResolverGetAppConfig(nullptr, 0, nullptr, 48, nullptr) == invalidArgument());

        length = 0xCAFEF00Du;
        Require(sceUrlConfigResolverGetAppConfig(nullptr, 0, reinterpret_cast<void*>(0x10), 0, &length) == invalidArgument());
        Require(length == 0);

        length = 0xCAFEF00Du;
        RequireInvalidArgument([&] {
            sceUrlConfigResolverGetAppConfig(nullptr, 0, reinterpret_cast<void*>(0x10), 4, &length);
        });
        Require(length == 0xCAFEF00Du);

        std::vector<unsigned char> buffer(16, 0x5A);
        RequireInvalidArgument([&] {
            sceUrlConfigResolverGetAppConfig(nullptr, 0, buffer.data(), 4, reinterpret_cast<std::uint32_t*>(0x10));
        });
        for (std::size_t i = 0; i < buffer.size(); ++i) Require(buffer[i] == (i < 4 ? 0 : 0x5A));
    }
    return 0;
}
