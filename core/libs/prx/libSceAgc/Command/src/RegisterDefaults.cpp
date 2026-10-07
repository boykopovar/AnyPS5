#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <mutex>
#include <vector>

namespace Agc::Command {

namespace {

struct RegisterDefaults {
    ShaderRegister** tables[4];
    std::uint32_t registerCounts[4];
    std::uint32_t* types;
    std::uint32_t count;
};

static_assert(offsetof(RegisterDefaults, count) == 0x38);

struct CompactBank {
    ShaderRegister* registers;
    std::uint32_t registerCount;
    const std::uint16_t* offsets;
    std::uint32_t pointerCount;
};

struct CompactRegisterDefaults {
    CompactBank banks[4];
    std::uint32_t* types;
    std::uint32_t count;
};

#include "prx/libSceAgc/Command/include/RegisterDefaultsData.hpp"

struct Storage {
    RegisterDefaults defaults{};
    std::array<std::vector<ShaderRegister*>, 4> pointers;
    bool initialized = false;
};

std::mutex defaultsMutex;
std::array<std::array<Storage, 14>, 2> storage;

}

void* GetRegisterDefaults(std::uint32_t version, bool internal, const char* function) {
    Require(version < 14, function, "unsupported register defaults version");
    const auto* source = internal ? agcInternalRegDefaultsByVersion[version] : agcPublicRegDefaultsByVersion[version];
    std::lock_guard lock(defaultsMutex);
    auto& destination = storage[internal ? 1 : 0][version];
    if (!destination.initialized) {
        std::array<std::vector<ShaderRegister*>, 4> pointers;
        RegisterDefaults defaults{};
        for (std::size_t bank = 0; bank < pointers.size(); ++bank) {
            const auto& input = source->banks[bank];
            Require(input.pointerCount == 0 || (input.registers != nullptr && input.offsets != nullptr), function,
                    "invalid register defaults bank");
            auto& table = pointers[bank];
            table.reserve(input.pointerCount);
            for (std::uint32_t index = 0; index < input.pointerCount; ++index) {
                Require(input.offsets[index] < input.registerCount, function, "register defaults pointer exceeds bank");
                table.push_back(input.registers + input.offsets[index]);
            }
            defaults.tables[bank] = table.empty() ? nullptr : table.data();
            defaults.registerCounts[bank] = input.registerCount;
        }
        Require(source->count == 0 || source->types != nullptr, function, "missing register defaults types");
        defaults.types = source->types;
        defaults.count = source->count;
        destination.pointers = std::move(pointers);
        destination.defaults = defaults;
        destination.initialized = true;
    }
    return &destination.defaults;
}

}
