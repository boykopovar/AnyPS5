#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_COMPACTUNWIND_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_COMPACTUNWIND_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "prx/libc/include/exceptions/Unwind.hpp"

namespace LibcUnwind {

enum class CompactParse { Miss, Found, Failed };

enum class CompactPersonality { Unknown, Gxx, Gcc };

struct CompactEntry {
    std::uint32_t encoding {};
    std::uint32_t functionOffset {};
    std::uint32_t functionEndOffset {};
    bool hasPersonality {};
    std::int32_t personalityDelta {};
    bool hasLsda {};
    std::uint32_t lsdaOffset {};
    bool dwarf {};
    std::uint32_t dwarfOffset {};
};

using CompactDwarfDecode = bool (*)(std::uint32_t ehOffset, void* user);

inline std::uint32_t ReadU32At(std::span<const unsigned char> data, std::size_t offset, bool& ok) {
    std::uint32_t value = 0;
    if (!ok || offset > data.size() || data.size() - offset < 4) { ok = false; return 0; }
    std::memcpy(&value, data.data() + offset, 4);
    return value;
}

inline CompactPersonality MatchCompactPersonality(const char* name) {
    if (name == nullptr) return CompactPersonality::Unknown;
    if (std::strcmp(name, "__gxx_personality_v0") == 0 || std::strcmp(name, "___gxx_personality_v0") == 0) return CompactPersonality::Gxx;
    if (std::strcmp(name, "__gcc_personality_v0") == 0 || std::strcmp(name, "___gcc_personality_v0") == 0) return CompactPersonality::Gcc;
    return CompactPersonality::Unknown;
}

inline bool CompactUnwindRules(std::uint32_t encoding, std::span<const unsigned char> function, Rules& rules) {
    const auto mode = encoding & 0x0f000000u;
    rules = {};
    auto save = [&](unsigned reg, std::intptr_t offset) {
        if (reg < RuleCount) rules.registers[reg] = {1, offset};
    };
    save(16, -8);
    if (mode == 0x01000000u) {
        rules.cfaRegister = 6;
        rules.cfaOffset = 16;
        save(6, -16);
        auto locations = encoding & 0x7fffu;
        const auto slot = static_cast<std::intptr_t>((encoding >> 16) & 0xffu);
        for (int index = 0; index < 5; ++index) {
            unsigned reg = 0;
            switch (locations & 7u) {
            case 1: reg = 3; break;
            case 2: reg = 12; break;
            case 3: reg = 13; break;
            case 4: reg = 14; break;
            case 5: reg = 15; break;
            case 0: break;
            default: return false;
            }
            if (reg) save(reg, -16 - 8 * slot + 8 * index);
            locations >>= 3;
        }
        return true;
    }
    if (mode == 0) return false;
    if (mode != 0x02000000u && mode != 0x03000000u) return false;
    const auto stackEncoded = (encoding >> 16) & 0xffu;
    const auto stackAdjust = (encoding >> 13) & 7u;
    const auto regCount = (encoding >> 10) & 7u;
    auto permutation = encoding & 0x3ffu;
    if (regCount > 6) return false;
    std::uint32_t stackSize = stackEncoded * 8u;
    if (mode == 0x03000000u) {
        if (stackEncoded > function.size() || function.size() - stackEncoded < 4) return false;
        std::uint32_t immediate = 0;
        std::memcpy(&immediate, function.data() + stackEncoded, 4);
        stackSize = immediate + 8u * stackAdjust;
    }
    if (stackSize > 0x10000000u) return false;
    rules.cfaRegister = 7;
    rules.cfaOffset = static_cast<std::intptr_t>(stackSize);
    unsigned permuted[6] = {};
    switch (regCount) {
    case 6:
        permuted[0] = permutation / 120; permutation -= permuted[0] * 120;
        permuted[1] = permutation / 24; permutation -= permuted[1] * 24;
        permuted[2] = permutation / 6; permutation -= permuted[2] * 6;
        permuted[3] = permutation / 2; permutation -= permuted[3] * 2;
        permuted[4] = permutation; permuted[5] = 0;
        break;
    case 5:
        permuted[0] = permutation / 120; permutation -= permuted[0] * 120;
        permuted[1] = permutation / 24; permutation -= permuted[1] * 24;
        permuted[2] = permutation / 6; permutation -= permuted[2] * 6;
        permuted[3] = permutation / 2; permutation -= permuted[3] * 2;
        permuted[4] = permutation;
        break;
    case 4:
        permuted[0] = permutation / 60; permutation -= permuted[0] * 60;
        permuted[1] = permutation / 12; permutation -= permuted[1] * 12;
        permuted[2] = permutation / 3; permutation -= permuted[2] * 3;
        permuted[3] = permutation;
        break;
    case 3:
        permuted[0] = permutation / 20; permutation -= permuted[0] * 20;
        permuted[1] = permutation / 4; permutation -= permuted[1] * 4;
        permuted[2] = permutation;
        break;
    case 2:
        permuted[0] = permutation / 5; permutation -= permuted[0] * 5;
        permuted[1] = permutation;
        break;
    case 1: permuted[0] = permutation; break;
    case 0: break;
    default: return false;
    }
    bool used[7] = {};
    static const unsigned dwarf[7] = {0, 3, 12, 13, 14, 15, 6};
    for (unsigned index = 0; index < regCount; ++index) {
        unsigned renumbered = 0;
        unsigned reg = 0;
        for (unsigned candidate = 1; candidate < 7; ++candidate) {
            if (used[candidate]) continue;
            if (renumbered == permuted[index]) { reg = dwarf[candidate]; used[candidate] = true; break; }
            ++renumbered;
        }
        if (reg == 0) return false;
        save(reg, -8 * static_cast<std::intptr_t>(regCount - index + 1));
    }
    return true;
}

inline CompactParse ParseCompactUnwind(std::span<const unsigned char> info, std::span<const unsigned char> eh, std::uint32_t target, CompactDwarfDecode decode, void* user, CompactEntry& entry) {
    entry = {};
    if (info.size() < 28) return CompactParse::Miss;
    bool ok = true;
    auto read = [&](std::size_t offset) { return ReadU32At(info, offset, ok); };
    if (read(0) != 1 || !ok) return CompactParse::Miss;
    const auto commonOffset = read(4);
    const auto commonCount = read(8);
    const auto personalityOffset = read(12);
    const auto personalityCount = read(16);
    const auto indexOffset = read(20);
    const auto indexCount = read(24);
    if (!ok || indexCount < 2) return CompactParse::Miss;
    std::uint32_t low = 0, high = indexCount;
    const auto last = indexCount - 1;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto start = read(indexOffset + static_cast<std::size_t>(mid) * 12u);
        if (!ok) return CompactParse::Miss;
        if (start <= target) {
            const auto follows = mid == last ? target : read(indexOffset + static_cast<std::size_t>(mid + 1) * 12u);
            if (!ok) return CompactParse::Miss;
            if (mid == last || follows > target) { low = mid; break; }
            low = mid + 1;
        } else high = mid;
    }
    if (!ok || low >= last) return CompactParse::Miss;
    const auto pageFunction = read(indexOffset + static_cast<std::size_t>(low) * 12u);
    const auto nextFunction = read(indexOffset + static_cast<std::size_t>(low + 1) * 12u);
    const auto pageOffset = read(indexOffset + static_cast<std::size_t>(low) * 12u + 4u);
    const auto lsdaOffset = read(indexOffset + static_cast<std::size_t>(low) * 12u + 8u);
    const auto lsdaEnd = read(indexOffset + static_cast<std::size_t>(low + 1) * 12u + 8u);
    if (!ok || pageOffset == 0) return CompactParse::Miss;
    const auto pageKind = read(pageOffset);
    const auto pagePacked = read(pageOffset + 4u);
    if (!ok) return CompactParse::Miss;
    const auto entryOffset = pagePacked & 0xffffu;
    const auto entryCount = pagePacked >> 16;
    if (entryCount == 0) return CompactParse::Miss;
    std::uint32_t encoding = 0;
    std::uint32_t functionOffset = 0;
    std::uint32_t functionEndOffset = 0;
    if (pageKind == 2) {
        low = 0;
        high = entryCount;
        while (low < high) {
            const auto mid = low + (high - low) / 2;
            const auto start = read(pageOffset + entryOffset + static_cast<std::size_t>(mid) * 8u);
            if (!ok) return CompactParse::Miss;
            if (start <= target) {
                if (mid + 1 == entryCount) { low = mid; break; }
                const auto follows = read(pageOffset + entryOffset + static_cast<std::size_t>(mid + 1) * 8u);
                if (!ok) return CompactParse::Miss;
                if (follows > target) { low = mid; break; }
                low = mid + 1;
            } else high = mid;
        }
        if (low >= entryCount) return CompactParse::Miss;
        functionOffset = read(pageOffset + entryOffset + static_cast<std::size_t>(low) * 8u);
        encoding = read(pageOffset + entryOffset + static_cast<std::size_t>(low) * 8u + 4u);
        if (!ok) return CompactParse::Miss;
        functionEndOffset = low + 1 < entryCount ? read(pageOffset + entryOffset + static_cast<std::size_t>(low + 1) * 8u) : nextFunction;
    } else if (pageKind == 3) {
        const auto encodingPacked = read(pageOffset + 8u);
        if (!ok) return CompactParse::Miss;
        const auto encodingsOffset = encodingPacked & 0xffffu;
        const auto pageTarget = target - pageFunction;
        const auto pageLast = entryCount - 1;
        low = 0;
        high = entryCount;
        while (low < high) {
            const auto mid = low + (high - low) / 2;
            const auto packed = read(pageOffset + entryOffset + static_cast<std::size_t>(mid) * 4u);
            if (!ok) return CompactParse::Miss;
            const auto start = packed & 0x00ffffffu;
            if (start <= pageTarget) {
                if (mid == pageLast) { low = mid; break; }
                const auto follows = read(pageOffset + entryOffset + static_cast<std::size_t>(mid + 1) * 4u) & 0x00ffffffu;
                if (!ok) return CompactParse::Miss;
                if (follows > pageTarget) { low = mid; break; }
                low = mid + 1;
            } else high = mid;
        }
        if (low >= entryCount) return CompactParse::Miss;
        const auto packed = read(pageOffset + entryOffset + static_cast<std::size_t>(low) * 4u);
        if (!ok) return CompactParse::Miss;
        functionOffset = pageFunction + (packed & 0x00ffffffu);
        if (low < pageLast) functionEndOffset = pageFunction + (read(pageOffset + entryOffset + static_cast<std::size_t>(low + 1) * 4u) & 0x00ffffffu);
        else functionEndOffset = nextFunction;
        const auto encodingIndex = packed >> 24;
        if (encodingIndex < commonCount) encoding = read(commonOffset + static_cast<std::size_t>(encodingIndex) * 4u);
        else encoding = read(pageOffset + encodingsOffset + static_cast<std::size_t>(encodingIndex - commonCount) * 4u);
    } else return CompactParse::Miss;
    if (!ok || target < functionOffset || target > functionEndOffset || functionEndOffset < functionOffset) return CompactParse::Miss;
    const auto mode = encoding & 0x0f000000u;
    if (mode == 0) return CompactParse::Failed;
    if (mode == 0x04000000u) {
        const auto ehOffset = encoding & 0x00ffffffu;
        if (eh.empty() || static_cast<std::size_t>(ehOffset) >= eh.size()) return CompactParse::Failed;
        if (decode == nullptr || !decode(ehOffset, user)) return CompactParse::Failed;
        entry.encoding = encoding;
        entry.functionOffset = functionOffset;
        entry.functionEndOffset = functionEndOffset;
        entry.dwarf = true;
        entry.dwarfOffset = ehOffset;
        return CompactParse::Found;
    }
    bool hasLsda = false;
    std::uint32_t lsda = 0;
    if (encoding & 0x40000000u) {
        if (lsdaEnd < lsdaOffset) return CompactParse::Failed;
        const auto count = (lsdaEnd - lsdaOffset) / 8u;
        low = 0;
        high = count;
        while (low < high) {
            const auto mid = low + (high - low) / 2;
            const auto start = read(lsdaOffset + static_cast<std::size_t>(mid) * 8u);
            if (!ok) return CompactParse::Failed;
            if (start == functionOffset) {
                lsda = read(lsdaOffset + static_cast<std::size_t>(mid) * 8u + 4u);
                hasLsda = true;
                break;
            }
            if (start < functionOffset) low = mid + 1;
            else high = mid;
        }
        if (!ok || !hasLsda) return CompactParse::Failed;
    }
    bool hasPersonality = false;
    std::int32_t personalityDelta = 0;
    const auto personalityIndex = (encoding >> 28) & 3u;
    if (personalityIndex != 0) {
        if (personalityIndex - 1 >= personalityCount) return CompactParse::Failed;
        personalityDelta = static_cast<std::int32_t>(read(personalityOffset + static_cast<std::size_t>(personalityIndex - 1) * 4u));
        if (!ok) return CompactParse::Failed;
        hasPersonality = true;
    }
    entry.encoding = encoding;
    entry.functionOffset = functionOffset;
    entry.functionEndOffset = functionEndOffset;
    entry.hasPersonality = hasPersonality;
    entry.personalityDelta = personalityDelta;
    entry.hasLsda = hasLsda;
    entry.lsdaOffset = lsda;
    return CompactParse::Found;
}

}
#endif
