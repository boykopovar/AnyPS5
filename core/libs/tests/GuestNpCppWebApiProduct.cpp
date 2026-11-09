#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        alignas(16) std::array<std::byte, 384> storage;
        storage.fill(std::byte{0xa5});
        for (const std::size_t offset : {0u, 128u, 256u}) {
            auto* product = storage.data() + offset;
            auto* label = product + 0x30;
            const auto before = storage;
            const auto* result = _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product8getLabelEv(product);
            if (result != label || storage != before) throw std::runtime_error("product label getter changed its owner or returned a different member");
            const std::array<std::byte, 16> changed{std::byte{0x31}, std::byte{0x42}, std::byte{0x53}, std::byte{0x64}};
            std::copy(changed.begin(), changed.end(), label);
            if (std::memcmp(result, changed.data(), changed.size()) != 0) throw std::runtime_error("product label getter returned an owned copy instead of a member reference");
            const auto updated = storage;
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product8getLabelEv(product) != result || storage != updated) throw std::runtime_error("product label member identity changed after an owner update");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
