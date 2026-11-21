#ifndef CODEGEN_TESTS_VECTOREXECUTION_HPP
#define CODEGEN_TESTS_VECTOREXECUTION_HPP

#include "LoweringExecution.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#if LOWERING_EXECUTION_AVAILABLE

namespace VectorExecution {

using LoweringExecution::Bytes;

struct Machine {
    std::array<std::array<std::uint64_t, 4>, 16> Ymm;
    std::array<std::uint64_t, 4> Memory;
    std::array<std::uint64_t, 16> RedZone;
    std::array<std::uint64_t, 16> Gpr;
    std::uint64_t Flags;
    std::uint32_t Mxcsr = 0x1F80;
};

static_assert(offsetof(Machine, Gpr) == 672 && offsetof(Machine, Flags) == 800 && offsetof(Machine, Mxcsr) == 808);

inline constexpr std::uint64_t kStatusFlags = 0x8D5u;

class Runner {
public:
    explicit Runner(const Bytes& body) {
        Bytes code{0x53, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x64, 0x24, 0xF8, 0x0F, 0xAE, 0x1C, 0x24};
        code.insert(code.end(), {0x48, 0x8B, 0x4F, 0x08, 0x48, 0x8B, 0x3F, 0x48, 0x89, 0xE2, 0x48, 0x89, 0xCC, 0x52, 0x57});
        code.insert(code.end(), {0x48, 0x8D, 0x87});
        append32(code, 512);
        for (std::uint8_t reg = 0; reg < 16; ++reg) {
            code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, 0x6F, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
            append32(code, reg * 32);
        }
        code.insert(code.end(), {0x0F, 0xAE, 0x97});
        append32(code, kMxcsr);
        code.insert(code.end(), {0xFF, 0xB7});
        append32(code, kFlags);
        code.push_back(0x9D);
        for (int slot = 0; slot < 16; ++slot) {
            code.insert(code.end(), {0x48, 0x8B, 0x8F});
            append32(code, 544 + slot * 8);
            code.insert(code.end(), {0x48, 0x89, 0x8C, 0x24});
            append32(code, -128 + slot * 8);
        }
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 0 && reg != 4 && reg != 7) gpr(code, 0x8B, reg);
        gpr(code, 0x8B, 7);
        code.insert(code.end(), body.begin(), body.end());
        code.insert(code.end(), {0x48, 0x87, 0x3C, 0x24, 0x0F, 0xAE, 0x9F});
        append32(code, kMxcsr);
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) gpr(code, 0x89, reg);
        for (std::uint8_t reg = 0; reg < 16; ++reg) {
            code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, 0x7F, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
            append32(code, reg * 32);
        }
        for (int slot = 0; slot < 16; ++slot) {
            code.insert(code.end(), {0x48, 0x8B, 0x8C, 0x24});
            append32(code, -128 + slot * 8);
            code.insert(code.end(), {0x48, 0x89, 0x8F});
            append32(code, 544 + slot * 8);
        }
        code.insert(code.end(), {0x9C, 0x8F, 0x87});
        append32(code, kFlags);
        code.insert(code.end(), {0x48, 0x8B, 0x04, 0x24, 0x48, 0x89, 0x87});
        append32(code, kGpr + 7 * 8);
        code.insert(code.end(), {0x58, 0x5C, 0x0F, 0xAE, 0x14, 0x24, 0x48, 0x8D, 0x64, 0x24, 0x08});
        code.insert(code.end(), {0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5D, 0x5B, 0xC5, 0xF8, 0x77, 0xC3});
        entry = LoweringExecution::AppendHostEntry(code);
        if (code.size() > kPage) throw std::runtime_error("Vector execution code is too large");
        page = LoweringExecution::AllocatePages(kPage);
        if (!page) throw std::runtime_error("Cannot allocate the vector execution page");
        std::memcpy(page, code.data(), code.size());
        if (!LoweringExecution::ProtectPages(page, kPage, true, false)) {
            LoweringExecution::FreePages(page, kPage);
            throw std::runtime_error("Cannot make the vector execution page executable");
        }
    }
    ~Runner() { LoweringExecution::FreePages(page, kPage); }
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    void Run(Machine& machine) const {
        alignas(64) std::array<std::uint8_t, 8192> stack{};
        struct {
            Machine* Target;
            std::uint8_t* Top;
        } frame{&machine, stack.data() + stack.size()};
        LoweringExecution::Call(page, entry, &frame);
    }

private:
    static constexpr std::size_t kPage = 8192;
    static constexpr std::int32_t kGpr = 672;
    static constexpr std::int32_t kFlags = 800;
    static constexpr std::int32_t kMxcsr = 808;
    std::uint8_t* page = nullptr;
    std::size_t entry = 0;
    static void append32(Bytes& bytes, std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
    static void gpr(Bytes& bytes, std::uint8_t opcode, std::uint8_t reg) {
        bytes.insert(bytes.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
        append32(bytes, kGpr + reg * 8);
    }
};

inline Machine Initial() {
    Machine machine{};
    for (std::size_t reg = 0; reg < 16; ++reg)
        for (std::size_t q = 0; q < 4; ++q) machine.Ymm[reg][q] = 0x0123456789ABCDEFull * (reg + 1) ^ (0x1111111111111111ull * (q + 3)) ^ (q == 1 ? 0x8000800080008000ull : 0);
    for (std::size_t q = 0; q < 4; ++q) machine.Memory[q] = 0xFEDCBA9876543210ull ^ (0x0F0F0F0F0F0F0F0Full * q);
    for (auto& slot : machine.RedZone) slot = LoweringExecution::kSentinel;
    for (std::size_t reg = 0; reg < 16; ++reg) machine.Gpr[reg] = 0xA5A5000000000000ull ^ (0x0101010101010101ull * (reg + 1));
    machine.Flags = 0x202u | kStatusFlags;
    return machine;
}

}

#endif

#endif
