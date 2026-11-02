#include <lifter/Codegen.hpp>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace Ps5Lift {

void Optimize(llvm::Module& module, const Target& target) {
    if (!target.Optimize) return;
    auto machine = target.Machine();

    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager cgscc;
    llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder builder(machine.get());
    builder.registerModuleAnalyses(modules);
    builder.registerCGSCCAnalyses(cgscc);
    builder.registerFunctionAnalyses(functions);
    builder.registerLoopAnalyses(loops);
    builder.crossRegisterProxies(loops, functions, cgscc, modules);
    builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2).run(module, modules);
    std::string problems;
    llvm::raw_string_ostream stream(problems);
    if (llvm::verifyModule(module, &stream)) throw std::runtime_error("LLVM's optimizer left a module that does not verify: " + problems);
}

llvm::SmallVector<char, 0> CompileObject(llvm::Module& module, const Target& target) {
    auto machine = target.Machine();
    llvm::SmallVector<char, 0> object;
    llvm::raw_svector_ostream stream(object);
    llvm::legacy::PassManager passes;
    if (machine->addPassesToEmitFile(passes, stream, nullptr, llvm::CodeGenFileType::ObjectFile, false)) throw std::runtime_error("The target cannot emit object files");
    passes.run(module);
    return object;
}

void WriteIr(const llvm::Module& module, const std::string& path) {
    std::error_code error;
    llvm::raw_fd_ostream stream(path, error, llvm::sys::fs::OF_Text);
    if (error) throw std::runtime_error("Cannot write " + path + ": " + error.message());
    module.print(stream, nullptr);
}

}
