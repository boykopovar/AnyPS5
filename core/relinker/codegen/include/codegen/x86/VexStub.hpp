#ifndef CODEGEN_X86_VEXSTUB_HPP
#define CODEGEN_X86_VEXSTUB_HPP

#include <codegen/x86/VexEncoding.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstdint>
#include <string>

namespace Codegen::VexStub {

inline void ConstantLanes(StubBodyBuilder& body, const VexEncoding::Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const std::array<std::uint32_t, 4>& lanes) {
    auto bytes = VexEncoding::Header(form, reg, vvvv, false, false);
    bytes.push_back(static_cast<std::uint8_t>(((reg & 7) << 3) | 5));
    const auto displacement = bytes.size();
    bytes.insert(bytes.end(), 4, 0);
    StubConstant constant{};
    for (std::size_t index = 0; index < constant.size(); ++index)
        constant[index] = static_cast<std::uint8_t>(lanes[index / 4] >> (8 * (index % 4)));
    body.RipConstant(bytes, displacement, constant);
}

inline void Constant(StubBodyBuilder& body, const VexEncoding::Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint32_t value) {
    ConstantLanes(body, form, reg, vvvv, {value, value, value, value});
}

inline void OrReduce(StubBodyBuilder& body, const std::uint8_t flags, const std::uint8_t temporary) {
    constexpr VexEncoding::Form shuffle{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0x70};
    constexpr VexEncoding::Form combine{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0xEB};
    VexEncoding::Register(body, shuffle, temporary, 0, flags, std::uint8_t{0x4E});
    VexEncoding::Register(body, combine, flags, flags, temporary);
    VexEncoding::Register(body, shuffle, temporary, 0, flags, std::uint8_t{0xB1});
    VexEncoding::Register(body, combine, flags, flags, temporary);
}

inline void RaiseFlags(StubBodyBuilder& body, const std::uint8_t flags, const std::uint8_t temporary, const std::int32_t saved, const std::int32_t work) {
    constexpr VexEncoding::Form loadDword{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0x6E};
    constexpr VexEncoding::Form storeDword{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0x7E};
    constexpr VexEncoding::Form combine{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0xEB};
    constexpr VexEncoding::Form control{VexEncoding::Map0F, 0, false, false, 0xAE};
    constexpr std::uint8_t loadControl = 2;
    VexEncoding::StackOperand(body, loadDword, temporary, 0, saved);
    VexEncoding::Register(body, combine, temporary, temporary, flags);
    VexEncoding::StackOperand(body, storeDword, temporary, 0, work);
    VexEncoding::StackOperand(body, control, loadControl, 0, work);
}

class ScratchPool {
public:
    ScratchPool(const std::array<std::uint8_t, 16>& registers, const std::size_t count, const char* owner) : _registers(registers), _count(count), _owner(owner) {}

    [[nodiscard]] std::uint8_t Take() {
        for (std::size_t index = 0; index < _count; ++index) {
            if (_busy[index]) continue;
            _busy[index] = true;
            return _registers[index];
        }
        throw CodegenException(std::string(_owner) + " lowering ran out of scratch registers");
    }

    void Give(const std::uint8_t reg) {
        for (std::size_t index = 0; index < _count; ++index) {
            if (_registers[index] != reg || !_busy[index]) continue;
            _busy[index] = false;
            return;
        }
        throw CodegenException(std::string(_owner) + " lowering released a register it does not own");
    }

private:
    std::array<std::uint8_t, 16> _registers;
    std::size_t _count;
    const char* _owner;
    std::array<bool, 16> _busy{};
};

}

#endif
