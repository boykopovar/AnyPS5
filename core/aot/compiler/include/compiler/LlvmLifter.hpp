#pragma once

#include <compiler/ElfBinaryParser.hpp>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>

namespace Aot::Compiler {

struct LiftedModuleResult {
    std::unique_ptr<llvm::LLVMContext> Context;
    std::unique_ptr<llvm::Module> Module;
    size_t InstructionCount = 0;
};

class LlvmLifter {
public:
    static LiftedModuleResult LiftFunction(const ElfFunction& function, unsigned argumentCount);
};

} // namespace Aot::Compiler
