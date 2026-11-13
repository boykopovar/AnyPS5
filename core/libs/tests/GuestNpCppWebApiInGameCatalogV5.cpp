#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>

static void APS5_VABI DummyDelete(void*) {}

int main() {
    try {
        alignas(16) std::array<std::byte, 0x480> storage;
        storage.fill(std::byte{0xa5});
        for (const std::size_t offset : {0u, 0x240u}) {
            auto* container = storage.data() + offset;
            const auto before = storage;
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container8getLabelEv(container) != container + 0x58) throw std::runtime_error("container label getter did not return the +0x58 member");
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDisplayNameEv(container) != container + 0x88) throw std::runtime_error("container displayName getter did not return the +0x88 member");
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getReleaseDateEv(container) != container + 0xa8) throw std::runtime_error("container releaseDate getter did not return the +0xa8 member");
            if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv(container)) throw std::runtime_error("container releaseDateIsSet did not read the +0xc8 flag byte");
            if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv(container)) throw std::runtime_error("container skusIsSet did not read the +0x220 flag byte");
            if (storage != before) throw std::runtime_error("container getter mutated its owner");
            storage[offset + 0xc8] = std::byte{0};
            storage[offset + 0x220] = std::byte{0};
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv(container)) throw std::runtime_error("container releaseDateIsSet ignored a cleared +0xc8 flag");
            if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv(container)) throw std::runtime_error("container skusIsSet ignored a cleared +0x220 flag");
            storage[offset + 0xc8] = std::byte{1};
            storage[offset + 0x220] = std::byte{1};
            if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv(container)) throw std::runtime_error("container releaseDateIsSet ignored a set +0xc8 flag");
            if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv(container)) throw std::runtime_error("container skusIsSet ignored a set +0x220 flag");
            storage[offset + 0xc8] = std::byte{0};
            storage[offset + 0x220] = std::byte{0};

            alignas(8) std::array<std::byte, 0x40> vectorObject;
            vectorObject.fill(std::byte{0x5a});
            std::int32_t referenceCount = 7;
            std::memcpy(vectorObject.data() + 0x30, &referenceCount, sizeof(referenceCount));
            int context = 0;
            const IntrusivePtr member{vectorObject.data(), DummyDelete, &context};
            std::memcpy(container + 0x208, &member, sizeof(member));
            IntrusivePtr out;
            std::memset(&out, 0xa5, sizeof(out));
            if (_ZN3sce2Np9CppWebApi13InGameCatalog2V59Container7getSkusEv(&out, container) != &out) throw std::runtime_error("container getSkus did not return the out pointer");
            if (out.object != vectorObject.data() || out.deleter != DummyDelete || out.libContext != &context) throw std::runtime_error("container getSkus did not copy the +0x208 intrusive pointer");
            std::memcpy(&referenceCount, vectorObject.data() + 0x30, sizeof(referenceCount));
            if (referenceCount != 8) throw std::runtime_error("container getSkus did not retain the vector object");
            if (std::memcmp(container + 0x208, &member, sizeof(member)) != 0) throw std::runtime_error("container getSkus mutated the member pointer");
        }
        alignas(16) std::array<std::byte, 256> skuStorage;
        skuStorage.fill(std::byte{0xa5});
        auto* sku = skuStorage.data();
        const auto skuBefore = skuStorage;
        if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10getEndDateEv(sku) != sku + 0x88) throw std::runtime_error("sku endDate getter did not return the +0x88 member");
        if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv(sku)) throw std::runtime_error("sku endDateIsSet did not read the +0xa8 flag byte");
        if (skuStorage != skuBefore) throw std::runtime_error("sku getter mutated its owner");
        skuStorage[0xa8] = std::byte{0};
        if (_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv(sku)) throw std::runtime_error("sku endDateIsSet ignored a cleared +0xa8 flag");
        skuStorage[0xa8] = std::byte{1};
        if (!_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv(sku)) throw std::runtime_error("sku endDateIsSet ignored a set +0xa8 flag");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
