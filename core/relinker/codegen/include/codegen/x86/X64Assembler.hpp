#ifndef CODEGEN_X86_X64ASSEMBLER_HPP
#define CODEGEN_X86_X64ASSEMBLER_HPP

#include <codegen/CodegenException.hpp>
#include <codegen/x86/X64Register.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Codegen {

template<typename TEmitter> class X64Assembler {
public:
    X64Assembler(TEmitter& code, const std::map<std::string, std::uint32_t>& callTable, const std::map<std::string, std::uint32_t>& storage) : code(code), callTable(callTable), storage(storage) {}

    void Mark(const std::string& label) {
        if (!labels.emplace(label, code.GetRva()).second)
            throw CodegenException("Duplicate assembler label: " + label, code.GetRva());
    }

    void Jump(const std::string& label, const std::uint8_t condition = 0) {
        const auto branch = condition == 0 ? code.Branch({0xe9}) : code.Branch({0x0f, condition});
        branches.emplace_back(branch, label);
    }

    void Call(const std::string& label) { branches.emplace_back(code.Branch({0xe8}), label); }
    void Api(const std::string& name) { code.Rip({0xff, 0x15}, callTable.at(name)); }

    void Begin(const std::string& name) {
        Mark(name);
        const auto start = code.GetRva();
        functions.emplace_back(start, start);
        code.Emit({0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x48});
    }

    void End() {
        code.Emit({0x48, 0x83, 0xc4, 0x48, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5f, 0x5e, 0x5d, 0x5b, 0xc3});
        functions.back().second = code.GetRva();
    }

    void Mov(const X64Register destination, const X64Register source) { binary(0x89, destination, source); }
    void Add(const X64Register destination, const X64Register source) { binary(0x01, destination, source); }
    void Sub(const X64Register destination, const X64Register source) { binary(0x29, destination, source); }
    void Compare(const X64Register left, const X64Register right) { binary(0x39, left, right); }
    void Test(const X64Register value) { binary(0x85, value, value); }

    void Value(const X64Register destination, const std::uint64_t value) {
        code.Emit({static_cast<std::uint8_t>(0x48 | (raw(destination) >> 3)), static_cast<std::uint8_t>(0xb8 | (raw(destination) & 7))});
        code.U64(value);
    }

    void AddValue(const X64Register destination, const std::uint32_t value) { immediate(destination, 0, value); }
    void SubValue(const X64Register destination, const std::uint32_t value) { immediate(destination, 5, value); }
    void CompareValue(const X64Register destination, const std::uint32_t value) { immediate(destination, 7, value); }
    void Mask(const X64Register destination, const std::uint32_t value) { immediate(destination, 4, value); }

    void Load(const X64Register destination, const X64Register base, const std::uint32_t offset = 0, const std::uint8_t size = 8) { memory(destination, base, offset, size == 1 ? 0xb6 : size == 2 ? 0xb7 : 0x8b, size); }
    void Store(const X64Register base, const std::uint32_t offset, const X64Register source) { memory(source, base, offset, 0x89, 8); }
    void StoreByte(const X64Register base, const X64Register source) { memory(source, base, 0, 0x88, 1, false); }
    void Address(const X64Register destination, const X64Register base, const std::uint32_t offset) { memory(destination, base, offset, 0x8d, 8); }

    void Data(const X64Register destination, const std::string& name) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((raw(destination) >> 3) << 2)), 0x8d, static_cast<std::uint8_t>(0x05 | ((raw(destination) & 7) << 3))}, storage.at(name));
    }

    void Global(const X64Register destination, const std::string& name) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((raw(destination) >> 3) << 2)), 0x8b, static_cast<std::uint8_t>(0x05 | ((raw(destination) & 7) << 3))}, storage.at(name));
    }

    void Save(const std::string& name, const X64Register source) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((raw(source) >> 3) << 2)), 0x89, static_cast<std::uint8_t>(0x05 | ((raw(source) & 7) << 3))}, storage.at(name));
    }

    void Text(const std::string& name) { Data(X64Register::Cx, name); Call("write"); }

    void Finish() {
        for (const auto& [offset, label] : branches)
            code.PatchBranch(offset, labels.at(label));
    }

    std::uint32_t Label(const std::string& name) const { return labels.at(name); }
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& Functions() const { return functions; }

private:
    static constexpr std::uint8_t raw(const X64Register value) { return static_cast<std::uint8_t>(value); }

    void binary(const std::uint8_t opcode, const X64Register destination, const X64Register source) {
        code.Emit({static_cast<std::uint8_t>(0x48 | ((raw(source) >> 3) << 2) | (raw(destination) >> 3)), opcode, static_cast<std::uint8_t>(0xc0 | ((raw(source) & 7) << 3) | (raw(destination) & 7))});
    }

    void immediate(const X64Register destination, const std::uint8_t operation, const std::uint32_t value) {
        code.Emit({static_cast<std::uint8_t>(0x48 | (raw(destination) >> 3)), 0x81, static_cast<std::uint8_t>(0xc0 | (operation << 3) | (raw(destination) & 7))});
        code.U32(value);
    }

    void memory(const X64Register value, const X64Register base, const std::uint32_t offset, const std::uint8_t opcode, const std::uint8_t size, const bool extend = true) {
        code.Emit({static_cast<std::uint8_t>((size == 8 ? 0x48 : 0x40) | ((raw(value) >> 3) << 2) | (raw(base) >> 3))});
        if (size < 4 && extend)
            code.Emit({0x0f});
        code.Emit({opcode, static_cast<std::uint8_t>(0x80 | ((raw(value) & 7) << 3) | (raw(base) & 7))});
        if ((raw(base) & 7) == raw(X64Register::Sp))
            code.Emit({0x24});
        code.U32(offset);
    }

    TEmitter& code;
    const std::map<std::string, std::uint32_t>& callTable;
    const std::map<std::string, std::uint32_t>& storage;
    std::map<std::string, std::uint32_t> labels;
    std::vector<std::pair<std::size_t, std::string>> branches;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> functions;
};

}

#endif
