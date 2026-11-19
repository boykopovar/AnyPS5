#ifndef CODEGEN_TESTS_GPRVECTOREXECUTION_HPP
#define CODEGEN_TESTS_GPRVECTOREXECUTION_HPP

#include "LoweringExecution.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#if LOWERING_EXECUTION_AVAILABLE

namespace GprVectorExecution {

using LoweringExecution::Bytes;

struct Machine {
    std::array<std::array<std::uint64_t, 4>, 16> Ymm;
    std::array<std::uint64_t, 16> Gpr;
    std::uint64_t Flags;
    std::array<std::uint64_t, 16> RedZone;
};

inline constexpr std::int32_t kGpr = static_cast<std::int32_t>(offsetof(Machine, Gpr));
inline constexpr std::int32_t kFlags = static_cast<std::int32_t>(offsetof(Machine, Flags));
inline constexpr std::int32_t kRedZone = static_cast<std::int32_t>(offsetof(Machine, RedZone));
inline constexpr std::array<std::uint8_t, 6> kCalleeSaved{3, 5, 12, 13, 14, 15};

class Runner {
public:
    explicit Runner(const Bytes& body) {
        for (const auto reg : kCalleeSaved) push(reg);
        code.insert(code.end(), {0x48, 0x8B, 0x4F, 0x08, 0x48, 0x8B, 0x3F, 0x48, 0x89, 0xE2, 0x48, 0x89, 0xCC, 0x52, 0x57});
        for (std::uint8_t reg = 0; reg < 16; ++reg) vector(0x6F, reg);
        code.insert(code.end(), {0xFF, 0xB7});
        append32(kFlags);
        code.push_back(0x9D);
        for (int slot = 0; slot < 16; ++slot) {
            gpr(0x8B, 1, kRedZone + slot * 8);
            stack(0x89, 1, -128 + slot * 8);
        }
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) gpr(0x8B, reg, kGpr + reg * 8);
        gpr(0x8B, 7, kGpr + 7 * 8);
        code.insert(code.end(), body.begin(), body.end());
        code.insert(code.end(), {0x48, 0x87, 0x3C, 0x24});
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) gpr(0x89, reg, kGpr + reg * 8);
        for (std::uint8_t reg = 0; reg < 16; ++reg) vector(0x7F, reg);
        for (int slot = 0; slot < 16; ++slot) {
            stack(0x8B, 1, -128 + slot * 8);
            gpr(0x89, 1, kRedZone + slot * 8);
        }
        code.insert(code.end(), {0x9C, 0x59});
        gpr(0x89, 1, kFlags);
        code.push_back(0x59);
        gpr(0x89, 1, kGpr + 7 * 8);
        code.insert(code.end(), {0x5C, 0xC5, 0xF8, 0x77});
        for (auto reg = kCalleeSaved.rbegin(); reg != kCalleeSaved.rend(); ++reg) pop(*reg);
        code.push_back(0xC3);
        entry = LoweringExecution::AppendHostEntry(code);
        if (code.size() > kPage) throw std::runtime_error("move mask execution code is too large");
        page = LoweringExecution::AllocatePages(kPage);
        if (!page) throw std::runtime_error("cannot allocate the move mask execution page");
        std::memcpy(page, code.data(), code.size());
        if (!LoweringExecution::ProtectPages(page, kPage, true, false)) {
            LoweringExecution::FreePages(page, kPage);
            throw std::runtime_error("cannot make the move mask execution page executable");
        }
    }
    ~Runner() { LoweringExecution::FreePages(page, kPage); }
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    void Run(Machine& machine) const {
        alignas(64) std::array<std::uint8_t, 8192> hostStack{};
        struct {
            Machine* Target;
            std::uint8_t* Top;
        } frame{&machine, hostStack.data() + hostStack.size()};
        LoweringExecution::Call(page, entry, &frame);
    }

private:
    static constexpr std::size_t kPage = 8192;
    Bytes code;
    std::uint8_t* page = nullptr;
    std::size_t entry = 0;
    void append32(std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) code.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
    void push(std::uint8_t reg) {
        if (reg >= 8) code.push_back(0x41);
        code.push_back(static_cast<std::uint8_t>(0x50 | (reg & 7)));
    }
    void pop(std::uint8_t reg) {
        if (reg >= 8) code.push_back(0x41);
        code.push_back(static_cast<std::uint8_t>(0x58 | (reg & 7)));
    }
    void vector(std::uint8_t opcode, std::uint8_t reg) {
        code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
        append32(reg * 32);
    }
    void gpr(std::uint8_t opcode, std::uint8_t reg, std::int32_t displacement) {
        code.insert(code.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
        append32(displacement);
    }
    void stack(std::uint8_t opcode, std::uint8_t reg, std::int32_t displacement) {
        code.insert(code.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
        append32(displacement);
    }
};

inline Machine Initial(std::uint64_t seed, std::uint64_t flags) {
    Machine machine{};
    auto next = [&seed] {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    };
    for (auto& reg : machine.Ymm)
        for (auto& quad : reg) quad = next();
    for (auto& reg : machine.Gpr) reg = next();
    machine.Flags = flags;
    for (auto& slot : machine.RedZone) slot = LoweringExecution::kSentinel;
    return machine;
}

}

#endif

#endif
