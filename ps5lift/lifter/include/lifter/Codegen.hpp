#ifndef PS5LIFT_CODEGEN_HPP
#define PS5LIFT_CODEGEN_HPP

#include <lifter/Target.hpp>

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Module.h>

#include <string>

namespace Ps5Lift {

void Optimize(llvm::Module& module, const Target& target);
llvm::SmallVector<char, 0> CompileObject(llvm::Module& module, const Target& target);
void WriteIr(const llvm::Module& module, const std::string& path);

}

#endif
