#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_STACKWALK_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_STACKWALK_HPP

#include "prx/libc/include/exceptions/Unwind.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

class DwarfFrames {
public:
    using Byte = LibcUnwind::Byte;
    using Word = LibcUnwind::Word;
    using Registers = std::array<Word, 17>;
    enum class Step { Stepped, NoRecord, Failed };

    DwarfFrames(Word base, Word end, const Byte* frames, const Byte* framesEnd) : base(base), end(end) {
        for (const Byte* p = frames; p != nullptr && framesEnd - p >= 8;) {
            const Byte* record = p;
            const auto length = LibcUnwind::Read<std::uint32_t>(p);
            if (length == 0) continue;
            if (length == 0xffffffff || length < 4 || static_cast<Word>(framesEnd - p) < length) break;
            const Byte* next = p + length;
            if (LibcUnwind::Read<std::uint32_t>(p) != 0) {
                LibcUnwind::Frame frame{};
                const Byte* cursor = nullptr;
                const char* augmentation = nullptr;
                Byte lsdaEncoding = 0;
                if (LibcUnwind::DecodeRange(frame, LibcUnwind::Lookup{0, record}, cursor, augmentation, lsdaEncoding)) index.emplace_back(frame.start, record);
            }
            p = next;
        }
        std::sort(index.begin(), index.end());
    }

    Word Base() const {
        return base;
    }

    bool Contains(Word pc) const {
        return pc >= base && pc < end;
    }

    bool Covers(Word pc) const {
        LibcUnwind::Frame frame{};
        return find(pc, frame) == Step::Stepped;
    }

    template<typename TRead>
    Step Unwind(Registers& registers, bool leaf, TRead&& read) const {
        const Word pc = registers[16] - (leaf ? 0 : 1);
        LibcUnwind::Frame frame{};
        if (const auto found = find(pc, frame); found != Step::Stepped) return found;
        LibcUnwind::Rules initial;
        if (!LibcUnwind::Instructions(frame.cieBegin, frame.cieEnd, frame, ~Word(0), initial, {})) return Step::Failed;
        if constexpr (LibcUnwind::RuleCount > 32) {
            if (frame.returnRegister == 32) {
                initial.registers[16] = initial.registers[32];
                initial.registers[32] = {};
                frame.returnRegister = 16;
            }
        }
        LibcUnwind::Rules rules = initial;
        if (!LibcUnwind::Instructions(frame.begin, frame.end, frame, pc, rules, initial)) return Step::Failed;
        if (rules.cfaExpression != nullptr || rules.cfaRegister >= 16 || frame.returnRegister >= 17) return Step::Failed;
        const Word cfa = registers[rules.cfaRegister] + rules.cfaOffset;
        if (cfa <= registers[7]) return Step::Failed;
        Registers next = registers;
        for (unsigned column = 0; column < next.size(); ++column) {
            const auto& rule = rules.registers[column];
            if (rule.kind == 0) continue;
            if (rule.kind == 1) {
                if (!read(cfa + rule.value, next[column])) return Step::Failed;
            } else if (rule.kind == 2 && rule.value >= 0 && rule.value < 17) {
                next[column] = registers[rule.value];
            } else if (rule.kind == 4) {
                next[column] = cfa + rule.value;
            } else if (rule.kind == 5) {
                next[column] = 0;
            } else {
                return Step::Failed;
            }
        }
        next[7] = cfa;
        next[16] = next[frame.returnRegister];
        if (next[16] == 0 || (next[7] == registers[7] && next[16] == registers[16])) return Step::Failed;
        registers = next;
        return Step::Stepped;
    }

private:
    Step find(Word pc, LibcUnwind::Frame& frame) const {
        const auto found = std::upper_bound(index.begin(), index.end(), pc, [](Word value, const auto& entry) { return value < entry.first; });
        if (found == index.begin()) return Step::NoRecord;
        const Byte* p = nullptr;
        const char* augmentation = nullptr;
        Byte lsdaEncoding = 0;
        if (!LibcUnwind::DecodeRange(frame, LibcUnwind::Lookup{pc, std::prev(found)->second}, p, augmentation, lsdaEncoding)) return Step::Failed;
        if (pc < frame.start || pc - frame.start >= frame.length) return Step::NoRecord;
        if (*augmentation == 'z') {
            const auto size = LibcUnwind::Uleb(p);
            p += size;
        }
        frame.begin = p;
        return Step::Stepped;
    }

    Word base;
    Word end;
    std::vector<std::pair<Word, const Byte*>> index;
};

class CollapsedStacks {
public:
    void Add(std::span<const std::uint64_t> leafFirst, bool truncated) {
        std::vector<std::uint64_t> stack(leafFirst.begin(), leafFirst.end());
        if (truncated) {
            stack.push_back(0);
            ++truncatedStacks;
        }
        ++stacks[std::move(stack)];
    }

    std::uint64_t Truncated() const {
        return truncatedStacks;
    }

    template<typename TName>
    std::string Format(TName&& name) const {
        std::string output;
        for (const auto& [stack, count] : stacks) {
            for (auto frame = stack.rbegin(); frame != stack.rend(); ++frame) {
                if (frame != stack.rbegin()) output += ';';
                output += *frame == 0 ? std::string("[truncated]") : name(*frame);
            }
            output += ' ' + std::to_string(count) + '\n';
        }
        return output;
    }

private:
    std::map<std::vector<std::uint64_t>, std::uint64_t> stacks;
    std::uint64_t truncatedStacks = 0;
};

}

#endif
