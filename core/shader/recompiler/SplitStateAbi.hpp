#ifndef CORE_SHADER_RECOMPILER_SPLITSTATEABI_HPP
#define CORE_SHADER_RECOMPILER_SPLITSTATEABI_HPP

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler::SplitStateAbi {

inline constexpr std::uint32_t DescriptorSet = 3u;
inline constexpr std::uint32_t CapacityDword = 0u;
inline constexpr std::uint32_t GroupBaseDword = 1u;
inline constexpr std::uint32_t GroupCountDword = 2u;
inline constexpr std::uint32_t HeaderDwords = 8u;

enum class Action : std::uint32_t { ClearTarget, SaveTarget, Save, Load, SaveSelected, SaveState, LoadTarget, LoadState, StoreTarget };
enum class Area : std::uint32_t { Exit };

inline constexpr std::uint32_t VectorRegisters = 256u;
inline constexpr std::uint32_t ScalarBase = 0x100u;
inline constexpr std::uint32_t ScalarRegisters = 106u;
inline constexpr std::uint32_t ExecRegister = 0x200u;
inline constexpr std::uint32_t VccRegister = 0x201u;
inline constexpr std::uint32_t SccRegister = 0x202u;
inline constexpr std::uint32_t M0Register = 0x203u;

[[nodiscard]] constexpr bool ValidRegister(std::uint32_t reg) {
    return reg < VectorRegisters || (reg >= ScalarBase && reg < ScalarBase + ScalarRegisters) || (reg >= ExecRegister && reg <= M0Register);
}

[[nodiscard]] constexpr std::uint32_t RegisterSlots(std::uint32_t reg) {
    if (reg >= ScalarBase && reg < ScalarBase + ScalarRegisters) return 2u;
    if (reg == ExecRegister || reg == VccRegister) return 3u;
    return 1u;
}

[[nodiscard]] inline std::uint32_t SlotCount(std::span<const std::uint32_t> registers) {
    std::uint32_t slots = 0;
    for (const auto reg : registers) slots += RegisterSlots(reg);
    return slots;
}

struct Point {
    std::uint32_t programCounter = 0;
    bool afterInstruction = false;
    Action action = Action::Save;
    Area area = Area::Exit;
};

struct Plan {
    std::span<const Point> points;
    std::span<const std::uint32_t> registers;
    std::uint32_t vectorCount = 0;
    std::uint32_t targetRegister = 0;
    std::uint64_t selectedTarget = 0;
    std::span<const std::uint32_t> assumed;
};

[[nodiscard]] constexpr std::uint32_t AreaSlots(std::uint32_t vectorCount) {
    return vectorCount;
}

[[nodiscard]] constexpr std::uint32_t AreaBase(Area area, std::uint32_t vectorCount) {
    return static_cast<std::uint32_t>(area) * AreaSlots(vectorCount);
}

[[nodiscard]] constexpr std::uint32_t TargetSlot(std::uint32_t vectorCount) {
    return AreaSlots(vectorCount);
}

[[nodiscard]] constexpr std::uint32_t TotalSlots(std::uint32_t vectorCount) {
    return TargetSlot(vectorCount) + 2u;
}

inline constexpr std::uint32_t HeaderWords = 4u;
inline constexpr std::uint32_t PointWords = 4u;

[[nodiscard]] inline std::vector<std::uint32_t> Encode(const Plan& plan) {
    if (SlotCount(plan.registers) != plan.vectorCount) throw std::invalid_argument("split state plan names registers of " + std::to_string(SlotCount(plan.registers)) + " slots for " + std::to_string(plan.vectorCount) + " slots");
    std::vector<std::uint32_t> words{static_cast<std::uint32_t>(plan.registers.size()), plan.targetRegister, static_cast<std::uint32_t>(plan.selectedTarget), static_cast<std::uint32_t>(plan.selectedTarget >> 32u)};
    words.insert(words.end(), plan.registers.begin(), plan.registers.end());
    words.push_back(static_cast<std::uint32_t>(plan.assumed.size()));
    words.insert(words.end(), plan.assumed.begin(), plan.assumed.end());
    for (const auto& point : plan.points) words.insert(words.end(), {point.programCounter, point.afterInstruction ? 1u : 0u, static_cast<std::uint32_t>(point.action), static_cast<std::uint32_t>(point.area)});
    return words;
}

struct DecodedPlan {
    std::vector<Point> points;
    std::vector<std::uint32_t> registers;
    std::vector<std::uint32_t> assumed;
    Plan plan;
};

[[nodiscard]] inline DecodedPlan Decode(std::span<const std::uint32_t> words) {
    if (words.size() < HeaderWords + 1u || words.size() - HeaderWords - 1u < words[0]) throw std::invalid_argument("split state plan has " + std::to_string(words.size()) + " words");
    const std::size_t assumedAt = HeaderWords + words[0];
    const std::size_t pointsAt = assumedAt + 1u + words[assumedAt];
    if (pointsAt > words.size() || (words.size() - pointsAt) % PointWords != 0u) throw std::invalid_argument("split state plan has " + std::to_string(words.size()) + " words");
    DecodedPlan decoded;
    decoded.registers.assign(words.begin() + HeaderWords, words.begin() + assumedAt);
    for (const auto reg : decoded.registers) {
        if (!ValidRegister(reg)) throw std::invalid_argument("split state plan saves register " + std::to_string(reg));
    }
    decoded.assumed.assign(words.begin() + assumedAt + 1u, words.begin() + pointsAt);
    for (const auto reg : decoded.assumed) {
        if (reg < ScalarBase || reg >= ScalarBase + ScalarRegisters || std::find(decoded.registers.begin(), decoded.registers.end(), reg) == decoded.registers.end()) throw std::invalid_argument("split state plan assumes register " + std::to_string(reg));
    }
    for (std::size_t word = pointsAt; word < words.size(); word += PointWords) {
        if (words[word + 1u] > 1u || words[word + 2u] > static_cast<std::uint32_t>(Action::StoreTarget) || words[word + 3u] > static_cast<std::uint32_t>(Area::Exit)) throw std::invalid_argument("split state plan holds an unknown point");
        decoded.points.push_back({words[word], words[word + 1u] != 0u, static_cast<Action>(words[word + 2u]), static_cast<Area>(words[word + 3u])});
    }
    decoded.plan = {std::span<const Point>(), std::span<const std::uint32_t>(), SlotCount(decoded.registers), words[1], words[2] | (static_cast<std::uint64_t>(words[3]) << 32u)};
    return decoded;
}

}

#endif
