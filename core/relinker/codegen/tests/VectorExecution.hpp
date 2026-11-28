#ifndef CODEGEN_TESTS_VECTOREXECUTION_HPP
#define CODEGEN_TESTS_VECTOREXECUTION_HPP

#include "LoweringExecution.hpp"
#include <array>
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
};

class Runner {
public:
    explicit Runner(const Bytes& body) {
        Bytes code{0x48, 0x8B, 0x4F, 0x08, 0x48, 0x8B, 0x3F, 0x48, 0x89, 0xE2, 0x48, 0x89, 0xCC, 0x52};
        code.insert(code.end(), {0x48, 0x8D, 0x87});
        append32(code, 512);
        for (std::uint8_t reg = 0; reg < 16; ++reg) {
            code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, 0x6F, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
            append32(code, reg * 32);
        }
        for (int slot = 0; slot < 16; ++slot) {
            code.insert(code.end(), {0x48, 0x8B, 0x8F});
            append32(code, 544 + slot * 8);
            code.insert(code.end(), {0x48, 0x89, 0x8C, 0x24});
            append32(code, -128 + slot * 8);
        }
        code.insert(code.end(), body.begin(), body.end());
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
        code.insert(code.end(), {0x5C, 0xC5, 0xF8, 0x77, 0xC3});
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
    std::uint8_t* page = nullptr;
    std::size_t entry = 0;
    static void append32(Bytes& bytes, std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
};

inline Machine Initial() {
    Machine machine{};
    for (std::size_t reg = 0; reg < 16; ++reg)
        for (std::size_t q = 0; q < 4; ++q) machine.Ymm[reg][q] = 0x0123456789ABCDEFull * (reg + 1) ^ (0x1111111111111111ull * (q + 3)) ^ (q == 1 ? 0x8000800080008000ull : 0);
    for (std::size_t q = 0; q < 4; ++q) machine.Memory[q] = 0xFEDCBA9876543210ull ^ (0x0F0F0F0F0F0F0F0Full * q);
    for (auto& slot : machine.RedZone) slot = LoweringExecution::kSentinel;
    return machine;
}

}

#endif

#endif
