#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {

using ValueGetter = std::uint64_t (APS5_VABI*)(const void*);
using ReferenceGetter = const void* (APS5_VABI*)(const void*);

struct ValueProbe {
    const char* name;
    ValueGetter get;
};

struct ReferenceProbe {
    const char* name;
    ReferenceGetter get;
};

constexpr ValueProbe Values[]{
    {"offer-player", _ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead12getAccountIdEv},
    {"player", _ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead12getAccountIdEv},
    {"submitter", _ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter12getAccountIdEv},
};

constexpr ReferenceProbe References[]{
    {"activities", _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities14UserActivities12getAccountIdEv},
    {"storage-owner", _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V15Owner12getAccountIdEv},
};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckValue(const ValueProbe& probe) {
    alignas(8) std::array<std::uint8_t, 0x40> model{};
    constexpr std::uint64_t cases[]{0, 1, 0xffffffffULL, 0x100000000ULL, 0x8000000000000000ULL, 0x0123456789abcdefULL, 0xffffffffffffffffULL};
    for (const auto value : cases) {
        model.fill(0xa5);
        std::memcpy(model.data() + 0x10, &value, sizeof(value));
        const auto before = model;
        Require(probe.get(model.data()) == value, "account ID value mismatch");
        Require(model == before, "account ID getter modified the model");
    }
}

void CheckReference(const ReferenceProbe& probe) {
    alignas(8) std::array<std::uint8_t, 0x80> storage{};
    for (const std::size_t displacement : {0u, 8u, 16u}) {
        storage.fill(0xa5);
        const auto before = storage;
        const auto* model = storage.data() + displacement;
        const auto* field = static_cast<const std::uint8_t*>(probe.get(model));
        Require(field == model + 0x10, "account ID reference is not the original member");
        Require(storage == before, "account ID reference getter modified the model");
        storage[displacement + 0x10] = 0x3c;
        Require(field[0] == 0x3c, "account ID reference does not alias the model");
        const auto updated = storage;
        Require(probe.get(model) == field, "account ID reference changed after member update");
        Require(storage == updated, "account ID reference getter changed the member update");
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc > 2) return 2;
        const std::string_view selected = argc == 2 ? argv[1] : "";
        std::size_t checked = 0;
        for (const auto& probe : Values) {
            if (!selected.empty() && selected != probe.name) continue;
            CheckValue(probe);
            ++checked;
        }
        for (const auto& probe : References) {
            if (!selected.empty() && selected != probe.name) continue;
            CheckReference(probe);
            ++checked;
        }
        Require(checked != 0, "unknown account ID probe");
        std::printf("%zu account ID accessors checked\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "account ID probe failed: %s\n", error.what());
        return 1;
    }
}
