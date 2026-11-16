#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <limits>
#include <vector>
#include <utility>

namespace Agc::Command {

namespace {

struct TypeIndex {
    std::uint32_t key;
    std::uint32_t bankIndex;
};

static_assert(sizeof(TypeIndex) == 8);

struct RegisterDefaults {
    ShaderRegister** tables[4];
    std::uint32_t registerCounts[4];
    void* types;
    std::uint32_t count;
};

static_assert(offsetof(RegisterDefaults, types) == 0x30);
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
    std::vector<TypeIndex> types;
    bool initialized = false;
};

std::mutex defaultsMutex;
std::array<std::array<std::array<Storage, 14>, 2>, 2> storage;

struct DefaultContext {
    std::array<ShaderRegister, 1024> registers{};
    std::uint32_t count = 0;
    std::uint32_t version = 0;
    bool external = false;
    std::vector<std::uint32_t> resetCommands;
};

std::mutex defaultStateMutex;
std::array<DefaultContext, 2> defaultContexts;
std::uint32_t activeDefaultInstance = 0;

}

void* GetRegisterDefaults(std::uint32_t version, bool internal, const char* function,
                          RegisterDefaultsTypeLayout typeLayout) {
    Require(version < 14, function, "unsupported register defaults version");
    const auto* source = internal ? agcInternalRegDefaultsByVersion[version] : agcPublicRegDefaultsByVersion[version];
    std::lock_guard lock(defaultsMutex);
    const auto layoutIndex = typeLayout == RegisterDefaultsTypeLayout::KeyBankPairs ? 1 : 0;
    auto& destination = storage[internal ? 1 : 0][layoutIndex][version];
    if (!destination.initialized) {
        std::array<std::vector<ShaderRegister*>, 4> pointers;
        RegisterDefaults defaults{};
        for (std::size_t bank = 0; bank < pointers.size(); ++bank) {
            const auto& input = source->banks[bank];
            Require(input.pointerCount == 0 || (input.registers != nullptr && input.offsets != nullptr), function, "invalid register defaults bank");
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
        std::vector<TypeIndex> types;
        if (typeLayout == RegisterDefaultsTypeLayout::KeyBankPairs) {
            types.reserve(source->count);
            for (std::uint32_t entry = 0; entry < source->count; ++entry) {
                const std::uint32_t bankIndex = source->types[entry * 3 + 1] & 0x3ffu;
                Require((bankIndex >> 2) < source->banks[bankIndex & 3u].pointerCount, function, "register defaults type exceeds its bank");
                types.push_back({source->types[entry * 3], bankIndex});
            }
        }
        if (typeLayout == RegisterDefaultsTypeLayout::KeyBankPairs) {
            defaults.types = types.empty() ? nullptr : types.data();
        } else {
            defaults.types = source->types;
        }
        defaults.count = source->count;
        destination.pointers = std::move(pointers);
        destination.types = std::move(types);
        destination.defaults = defaults;
        destination.initialized = true;
    }
    return &destination.defaults;
}

int InitializeDefaultCxState(std::uint32_t version) {
    Require(version < 14, __func__, "unsupported context defaults version");
    std::lock_guard lock(defaultStateMutex);
    for (std::uint32_t index = 0; index < defaultContexts.size(); ++index) {
        if (defaultContexts[index].version == version) {
            defaultContexts[index].external = true;
            activeDefaultInstance = index;
            return 0;
        }
    }
    auto slot = std::find_if(defaultContexts.begin(), defaultContexts.end(), [](const auto& context) { return context.version == 0; });
    if (slot == defaultContexts.end()) return static_cast<int>(0x8a6c0004u);
    DefaultContext next;
    next.registers.fill({0xffffffffu, 0});
    for (const auto* source : {static_cast<const RegisterDefaults*>(GetRegisterDefaults(version, false, __func__)), static_cast<const RegisterDefaults*>(GetRegisterDefaults(version, true, __func__))}) {
        if (source->tables[0] == nullptr || source->registerCounts[0] == 0) continue;
        Require(source->tables[0][0] != nullptr, __func__, "missing context register defaults");
        for (std::uint32_t index = 0; index < source->registerCounts[0]; ++index) {
            const auto& entry = source->tables[0][0][index];
            Require(entry.offset < next.registers.size(), __func__, "context register default exceeds hardware range");
            next.registers[entry.offset] = entry;
        }
    }
    std::sort(next.registers.begin(), next.registers.end(), [](const auto& left, const auto& right) { return left.offset < right.offset; });
    const auto end = std::find_if(next.registers.begin(), next.registers.end(), [](const auto& entry) { return entry.offset == 0xffffffffu; });
    next.count = static_cast<std::uint32_t>(end - next.registers.begin());
    if (sceKernelIsTrinityMode() && (version < 11 || version == 12)) {
        const auto found = std::find_if(next.registers.begin(), end, [](const auto& entry) { return entry.offset == 0x212u; });
        Require(found != end, __func__, "missing Trinity context register default");
        found->value = 0x2400u;
    }
    std::vector<ShaderRegister> compute;
    for (const auto* source : {static_cast<const RegisterDefaults*>(GetRegisterDefaults(version, false, __func__)), static_cast<const RegisterDefaults*>(GetRegisterDefaults(version, true, __func__))}) {
        if (source->tables[1] == nullptr || source->registerCounts[1] == 0) continue;
        Require(source->tables[1][0] != nullptr, __func__, "missing shader register defaults");
        for (std::uint32_t index = 0; index < source->registerCounts[1]; ++index) {
            const auto& entry = source->tables[1][0][index];
            if (entry.offset >= 0x201u) compute.push_back(entry);
        }
    }
    std::sort(compute.begin(), compute.end(), [](const auto& left, const auto& right) { return left.offset < right.offset; });
    for (std::size_t begin = 0; begin < compute.size();) {
        auto end = begin + 1;
        while (end < compute.size() && compute[end].offset == compute[end - 1].offset + 1) ++end;
        next.resetCommands.push_back(Header(0x76u, static_cast<std::uint32_t>(end - begin) + 2));
        next.resetCommands.push_back(compute[begin].offset);
        for (auto index = begin; index < end; ++index) next.resetCommands.push_back(compute[index].value);
        begin = end;
    }
    if (next.resetCommands.size() * sizeof(std::uint32_t) >= 0x180u) return static_cast<int>(0x8a6c0001u);
    next.version = version;
    next.external = true;
    activeDefaultInstance = static_cast<std::uint32_t>(slot - defaultContexts.begin());
    *slot = std::move(next);
    return 0;
}

QueueResetState GetQueueResetState(std::uint32_t instance, bool active) {
    std::lock_guard lock(defaultStateMutex);
    if (active) instance = activeDefaultInstance;
    Require(instance < defaultContexts.size(), __func__, "invalid context defaults instance");
    const auto& context = defaultContexts[instance];
    return {instance, context.version, context.external, context.resetCommands};
}

int CopyDefaultCxStateRange(void* destination, std::uint32_t first, std::uint32_t last, std::span<const ShaderRegister> registers) {
    if (first >= last || registers.empty()) return 0;
    constexpr int InvalidArgument = static_cast<int>(0x80890001u);
    if (destination == nullptr) return InvalidArgument;
    auto next = std::lower_bound(registers.begin(), registers.end(), first, [](const auto& entry, auto offset) { return entry.offset < offset; });
    auto output = reinterpret_cast<std::uintptr_t>(destination);
    for (auto offset = first; offset < last; ++offset) {
        std::uint32_t value = 0;
        const bool finished = next == registers.end();
        if (!finished && next->offset == offset) value = (next++)->value;
        if (output > std::numeric_limits<std::uintptr_t>::max() - sizeof(value) || !GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(output), &value, sizeof(value))) return InvalidArgument;
        if (finished) break;
        output += sizeof(value);
    }
    return 0;
}

int GetDefaultCxStateFlat(void* destination, std::uint32_t first, std::uint32_t last, std::uint32_t instance) {
    if (first >= last) return 0;
    std::lock_guard lock(defaultStateMutex);
    Require(instance < defaultContexts.size(), __func__, "invalid context defaults instance");
    const auto& context = defaultContexts[instance];
    return CopyDefaultCxStateRange(destination, first, last, {context.registers.data(), context.count});
}

}
