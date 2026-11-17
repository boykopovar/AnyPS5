#include <compiler/LlvmLifter.hpp>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <array>
#include <span>
#include <stdexcept>

namespace Aot::Compiler {

namespace {

class LeafLifter {
public:
    LeafLifter(const ElfFunction& input, unsigned argumentCount, llvm::Module& module)
        : code(input.Code), builder(module.getContext()) {
        if (argumentCount > 6 || code.empty()) {
            throw std::runtime_error("A leaf function requires code and at most six integer arguments");
        }
        auto* type = llvm::FunctionType::get(builder.getInt64Ty(),
            std::vector<llvm::Type*>(argumentCount, builder.getInt64Ty()), false);
        auto* function = llvm::Function::Create(type, llvm::Function::ExternalLinkage, input.Name, module);
        builder.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(), "entry", function));
        constexpr std::array<unsigned, 6> argumentRegisters{7, 6, 2, 1, 8, 9};
        for (unsigned index = 0; index < argumentCount; ++index) {
            registers[argumentRegisters[index]] = function->getArg(index);
        }
    }

    size_t Lift() {
        size_t count = 0;
        while (offset < code.size()) {
            instructionOffset = offset;
            uint8_t opcode = byte();
            uint8_t rex = 0;
            if (opcode >= 0x40 && opcode <= 0x4f) {
                rex = opcode;
                opcode = byte();
            }
            unsigned width = rex & 8 ? 64 : 32;
            ++count;
            if (opcode == 0xc3 && !rex) {
                if (offset != code.size()) {
                    fail("bytes after RET are outside the supported straight-line function contract");
                }
                builder.CreateRet(read(0, 64));
                return count;
            }
            if (opcode == 0x90 && !rex) {
                continue;
            }
            if (opcode >= 0xb8 && opcode <= 0xbf) {
                write((opcode - 0xb8) + ((rex & 1) ? 8 : 0),
                    builder.getIntN(width, immediate(width / 8)));
                continue;
            }
            if (opcode == 0x05 || opcode == 0x0d || opcode == 0x25 ||
                opcode == 0x2d || opcode == 0x35) {
                auto* rhs = constant(immediate(4), 32, width);
                write(0, binary((opcode >> 3) & 7, read(0, width), rhs));
                continue;
            }
            if (opcode == 0x0f) {
                if (byte() != 0xaf) {
                    fail("unsupported two-byte opcode");
                }
                auto [rm, reg, extension] = operands(rex);
                write(reg, builder.CreateMul(read(reg, width), read(rm, width)));
                continue;
            }
            if (opcode == 0x89 || opcode == 0x8b) {
                auto [rm, reg, extension] = operands(rex);
                write(opcode == 0x89 ? rm : reg, read(opcode == 0x89 ? reg : rm, width));
                continue;
            }
            if (opcode == 0x63 && width == 64) {
                auto [rm, reg, extension] = operands(rex);
                write(reg, builder.CreateSExt(read(rm, 32), builder.getInt64Ty()));
                continue;
            }
            if (opcode == 0xc7) {
                auto [rm, reg, extension] = operands(rex);
                if (extension != 0) {
                    fail("unsupported MOV extension");
                }
                write(rm, constant(immediate(4), 32, width));
                continue;
            }
            if (opcode == 0x81 || opcode == 0x83) {
                auto [rm, reg, extension] = operands(rex);
                const unsigned bits = opcode == 0x83 ? 8 : 32;
                auto* rhs = constant(immediate(bits / 8), bits, width);
                write(rm, binary(extension, read(rm, width), rhs));
                continue;
            }
            if ((opcode & 0xc4) == 0 && (opcode & 1) &&
                (opcode <= 0x33) && ((opcode & 7) == 1 || (opcode & 7) == 3)) {
                auto [rm, reg, extension] = operands(rex);
                unsigned destination = (opcode & 2) ? reg : rm;
                unsigned source = (opcode & 2) ? rm : reg;
                const unsigned operation = (opcode >> 3) & 7;
                if (destination == source && (operation == 5 || operation == 6)) {
                    write(destination, builder.getIntN(width, 0));
                } else {
                    write(destination, binary(operation, read(destination, width), read(source, width)));
                }
                continue;
            }
            if (opcode == 0xf7) {
                auto [rm, reg, extension] = operands(rex);
                auto* value = read(rm, width);
                if (extension == 2) {
                    write(rm, builder.CreateNot(value));
                } else if (extension == 3) {
                    write(rm, builder.CreateNeg(value));
                } else {
                    fail("unsupported unary opcode");
                }
                continue;
            }
            if (opcode == 0xd1 || opcode == 0xd3 || opcode == 0xc1) {
                auto [rm, reg, extension] = operands(rex);
                llvm::Value* shift = opcode == 0xd3 ? read(1, width) :
                    builder.getIntN(width, opcode == 0xd1 ? 1 : byte());
                shift = builder.CreateAnd(shift, builder.getIntN(width, width - 1));
                auto* value = read(rm, width);
                if (extension == 4) {
                    write(rm, builder.CreateShl(value, shift));
                } else if (extension == 5) {
                    write(rm, builder.CreateLShr(value, shift));
                } else if (extension == 7) {
                    write(rm, builder.CreateAShr(value, shift));
                } else {
                    fail("unsupported shift opcode");
                }
                continue;
            }
            fail("unsupported opcode or prefix");
        }
        fail("function has no terminating RET");
    }

private:
    struct Operands {
        unsigned Rm;
        unsigned Reg;
        unsigned Extension;
    };

    std::span<const uint8_t> code;
    llvm::IRBuilder<> builder;
    std::array<llvm::Value*, 16> registers{};
    size_t offset = 0;
    size_t instructionOffset = 0;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error("At function byte " + std::to_string(instructionOffset) + ": " + message);
    }

    uint8_t byte() {
        if (offset == code.size()) {
            fail("truncated instruction");
        }
        return code[offset++];
    }

    uint64_t immediate(unsigned size) {
        uint64_t value = 0;
        for (unsigned index = 0; index < size; ++index) {
            value |= uint64_t(byte()) << (index * 8);
        }
        return value;
    }

    Operands operands(uint8_t rex) {
        const uint8_t modrm = byte();
        if ((modrm >> 6) != 3) {
            fail("memory operands are not supported");
        }
        return {(modrm & 7u) + ((rex & 1) ? 8u : 0u),
            ((modrm >> 3) & 7u) + ((rex & 4) ? 8u : 0u), (modrm >> 3) & 7u};
    }

    llvm::Value* constant(uint64_t value, unsigned bits, unsigned width) {
        return builder.CreateSExtOrTrunc(builder.getIntN(bits, value), builder.getIntNTy(width));
    }

    bool volatileRegister(unsigned reg) const {
        return reg == 0 || reg == 1 || reg == 2 || reg == 6 || reg == 7 ||
            (reg >= 8 && reg <= 11);
    }

    llvm::Value* read(unsigned reg, unsigned width) {
        if (!volatileRegister(reg) || !registers[reg]) {
            fail("register read depends on undeclared input or callee-saved state");
        }
        return builder.CreateTruncOrBitCast(registers[reg], builder.getIntNTy(width));
    }

    void write(unsigned reg, llvm::Value* value) {
        if (!volatileRegister(reg)) {
            fail("writes to the stack pointer or callee-saved registers are not supported");
        }
        registers[reg] = builder.CreateZExtOrBitCast(value, builder.getInt64Ty());
    }

    llvm::Value* binary(unsigned operation, llvm::Value* lhs, llvm::Value* rhs) {
        switch (operation) {
            case 0: return builder.CreateAdd(lhs, rhs);
            case 1: return builder.CreateOr(lhs, rhs);
            case 4: return builder.CreateAnd(lhs, rhs);
            case 5: return builder.CreateSub(lhs, rhs);
            case 6: return builder.CreateXor(lhs, rhs);
            default: fail("unsupported arithmetic operation");
        }
    }
};

} // namespace

LiftedModuleResult LlvmLifter::LiftFunction(const ElfFunction& function, unsigned argumentCount) {
    LiftedModuleResult result;
    result.Context = std::make_unique<llvm::LLVMContext>();
    result.Module = std::make_unique<llvm::Module>("anyps5_leaf", *result.Context);
    LeafLifter lifter(function, argumentCount, *result.Module);
    result.InstructionCount = lifter.Lift();
    std::string error;
    llvm::raw_string_ostream diagnostics(error);
    if (llvm::verifyModule(*result.Module, &diagnostics)) {
        throw std::runtime_error("Invalid lifted IR: " + error);
    }
    return result;
}

} // namespace Aot::Compiler
