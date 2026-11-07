#include <compiler/AotCompilerDriver.hpp>
#include <compiler/ElfBinaryParser.hpp>
#include <compiler/LlvmLifter.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace Aot::Compiler {

namespace {

struct Output {
    std::filesystem::path Path;
    llvm::SmallVector<char, 0> Bytes;
};

std::string DefaultTriple() {
#if defined(_WIN32)
    return "x86_64-w64-windows-gnu";
#elif defined(__APPLE__)
    return "x86_64-apple-darwin";
#else
    return "x86_64-unknown-linux-gnu";
#endif
}

void ValidatePaths(const CompilerOptions& options) {
    if (options.InputExecutablePath.empty() || options.OutputObjectPath.empty()) {
        throw std::runtime_error("Input and output paths are required");
    }
    std::vector<std::filesystem::path> paths{options.InputExecutablePath};
    for (const auto& path : {options.OutputObjectPath, options.OutputLlvmIrPath, options.OutputAssemblyPath}) {
        if (path.empty()) {
            continue;
        }
        for (const auto& other : paths) {
            if (std::filesystem::weakly_canonical(path) == std::filesystem::weakly_canonical(other) ||
                (std::filesystem::exists(path) && std::filesystem::exists(other) &&
                    std::filesystem::equivalent(path, other))) {
                throw std::runtime_error("Input and output paths must be distinct");
            }
        }
        if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("Output must be a regular file: " + path.string());
        }
        paths.push_back(path);
    }
}

Output Emit(llvm::Module& module, llvm::TargetMachine& machine,
    const std::filesystem::path& path, llvm::CodeGenFileType type) {
    Output output{path, {}};
    llvm::raw_svector_ostream stream(output.Bytes);
    auto copy = llvm::CloneModule(module);
    llvm::legacy::PassManager passes;
    if (machine.addPassesToEmitFile(passes, stream, nullptr, type)) {
        throw std::runtime_error("Target cannot emit the requested output");
    }
    passes.run(*copy);
    if (output.Bytes.empty()) {
        throw std::runtime_error("Target produced empty output");
    }
    return output;
}

void Write(const Output& output) {
    int descriptor = -1;
    llvm::SmallString<256> temporary;
    auto error = llvm::sys::fs::createUniqueFile(output.Path.string() + ".%%%%%%.tmp", descriptor, temporary);
    if (error) {
        throw std::runtime_error("Cannot create output " + output.Path.string() + ": " + error.message());
    }
    struct Cleanup {
        llvm::SmallString<256>& Path;
        ~Cleanup() { llvm::sys::fs::remove(Path); }
    } cleanup{temporary};
    llvm::raw_fd_ostream stream(descriptor, true);
    stream.write(output.Bytes.data(), output.Bytes.size());
    stream.close();
    if (stream.has_error()) {
        error = stream.error();
        stream.clear_error();
        throw std::runtime_error("Cannot write output: " + error.message());
    }
    error = llvm::sys::fs::rename(temporary, output.Path.string());
    if (error) {
        throw std::runtime_error("Cannot replace output: " + error.message());
    }
}

} // namespace

CompilationStatistics AotCompilerDriver::Compile(const CompilerOptions& options) {
    CompilationStatistics statistics;
    try {
        ValidatePaths(options);
        if (options.OptimizationLevel > 3) {
            throw std::runtime_error("Optimization level must be between 0 and 3");
        }
        const std::string triple = options.TargetTriple.empty() ? DefaultTriple() : options.TargetTriple;
        if (triple != "x86_64-unknown-linux-gnu" && triple != "x86_64-w64-windows-gnu" &&
            triple != "x86_64-pc-windows-msvc" && triple != "x86_64-apple-darwin") {
            throw std::runtime_error("Unsupported target triple: " + triple);
        }
        static std::once_flag targets;
        std::call_once(targets, [] {
            LLVMInitializeX86TargetInfo();
            LLVMInitializeX86Target();
            LLVMInitializeX86TargetMC();
            LLVMInitializeX86AsmPrinter();
        });
        std::string error;
        auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (!target) {
            throw std::runtime_error(error);
        }
        llvm::TargetOptions targetOptions;
        std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
            triple, "generic", "", targetOptions, llvm::Reloc::PIC_, std::nullopt,
            static_cast<llvm::CodeGenOptLevel>(options.OptimizationLevel)));
        if (!machine) {
            throw std::runtime_error("Cannot create the target machine");
        }
        auto function = ElfBinaryParser::ReadFunction(options.InputExecutablePath, options.FunctionName);
        auto lifted = LlvmLifter::LiftFunction(function, options.ArgumentCount);
        lifted.Module->setTargetTriple(triple);
        lifted.Module->setDataLayout(machine->createDataLayout());
        statistics.InstructionsLifted = lifted.InstructionCount;
        llvm::raw_string_ostream diagnostics(error);
        if (llvm::verifyModule(*lifted.Module, &diagnostics)) {
            throw std::runtime_error("Invalid target IR: " + error);
        }
        std::vector<Output> outputs;
        outputs.push_back(Emit(*lifted.Module, *machine, options.OutputObjectPath, llvm::CodeGenFileType::ObjectFile));
        if (!options.OutputAssemblyPath.empty()) {
            outputs.push_back(Emit(*lifted.Module, *machine, options.OutputAssemblyPath, llvm::CodeGenFileType::AssemblyFile));
        }
        if (!options.OutputLlvmIrPath.empty()) {
            Output output{options.OutputLlvmIrPath, {}};
            llvm::raw_svector_ostream stream(output.Bytes);
            lifted.Module->print(stream, nullptr);
            outputs.push_back(std::move(output));
        }
        for (const auto& output : outputs) {
            ValidatePaths(options);
            Write(output);
        }
        statistics.Success = true;
    } catch (const std::exception& exception) {
        statistics.Error = exception.what();
    }
    return statistics;
}

} // namespace Aot::Compiler
