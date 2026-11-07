#pragma once

#include <filesystem>
#include <string>

namespace Aot::Compiler {

struct CompilerOptions {
    std::filesystem::path InputExecutablePath;
    std::filesystem::path OutputObjectPath;
    std::filesystem::path OutputLlvmIrPath;
    std::filesystem::path OutputAssemblyPath;
    std::string FunctionName;
    std::string TargetTriple;
    unsigned ArgumentCount = 0;
    unsigned OptimizationLevel = 2;
};

struct CompilationStatistics {
    size_t InstructionsLifted = 0;
    bool Success = false;
    std::string Error;
};

class AotCompilerDriver {
public:
    static CompilationStatistics Compile(const CompilerOptions& options);
};

} // namespace Aot::Compiler
