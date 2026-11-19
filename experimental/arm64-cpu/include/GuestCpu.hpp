#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace anyps5::experimental {
class GuestCpu {
public:
    explicit GuestCpu(std::span<const std::uint8_t> program, std::size_t stackBytes = 65536);
    std::uint64_t run(std::size_t maxInstructions = 10000);
    // Configure a bounded data region separate from the guest stack.
    // Legacy single-region execution remains the default.
    void setMemoryRegions(std::size_t dataBegin, std::size_t dataEnd,
                          std::size_t stackBegin);
    void setRegister(std::size_t index, std::uint64_t value);
    std::uint64_t rax() const noexcept;
    std::uint64_t registerValue(std::size_t index) const;
    std::size_t instructionPointer() const noexcept;
    std::uint64_t readMemory64(std::uint64_t address) const;
    std::uint32_t readMemory32(std::uint64_t address) const;
    void writeMemory64(std::uint64_t address, std::uint64_t value);
    void writeMemory32(std::uint64_t address, std::uint32_t value);
private:
    std::vector<std::uint8_t> program_;
    std::vector<std::uint8_t> memory_;
    bool regionProtection_ = false;
    std::size_t dataBegin_ = 0;
    std::size_t dataEnd_ = 0;
    std::size_t stackBegin_ = 0;
    void checkMemory(std::uint64_t address, std::size_t bytes, bool write) const;
    std::array<std::uint64_t, 16> registers_{};
    std::size_t ip_ = 0;
    bool zeroFlag_ = false;
    bool carryFlag_ = false;
    bool signFlag_ = false;
    bool overflowFlag_ = false;
    bool parityFlag_ = true;
    void updateParity(std::uint64_t value);
    void updateArithmeticFlags(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, bool wide, bool subtraction);
    void updateArithmeticFlagsWidth(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, unsigned bits, bool subtraction);
    bool evaluateCondition(std::uint8_t condition) const;
    std::size_t callDepth_ = 0;
    struct Operand { bool isRegister; std::size_t index; std::uint64_t address; };
    Operand decodeOperand(std::uint8_t modrm, bool rexB, bool rexX, std::size_t trailingImmediateBytes = 0);
    std::uint64_t readOperand(const Operand& operand, bool wide) const;
    void writeOperand(const Operand& operand, std::uint64_t value, bool wide);
    std::uint64_t readOperandWidth(const Operand& operand, unsigned bits) const;
    void writeOperandWidth(const Operand& operand, std::uint64_t value, unsigned bits);
    std::uint8_t read8();
    std::uint32_t read32();
    std::int32_t readSigned32();
    void jumpRelative(std::int32_t displacement);
    void push64(std::uint64_t value);
    std::uint64_t pop64();
};
}
