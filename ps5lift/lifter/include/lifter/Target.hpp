#ifndef PS5LIFT_TARGET_HPP
#define PS5LIFT_TARGET_HPP

#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#include <memory>
#include <string>

namespace Ps5Lift {

struct Target {
    llvm::Triple Triple;
    std::string Cpu;
    std::string Features;
    std::string DataLayout;
    bool Optimize = true;

    std::unique_ptr<llvm::TargetMachine> Machine() const;
    void Configure(llvm::Module& module) const;
};

Target ResolveTarget(const llvm::Triple& triple, const std::string& cpu, const std::string& features, bool optimize);

}

#endif
