#include <compiler/AotCompilerDriver.hpp>
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

static unsigned Number(std::string_view value) {
    unsigned result;
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid unsigned integer: " + std::string(value));
    }
    return result;
}

int main(int argc, char* argv[]) {
    try {
        Aot::Compiler::CompilerOptions options;
        bool hasArguments = false;
        for (int index = 1; index < argc; ++index) {
            std::string_view argument(argv[index]);
            if (argument == "--help" || argument == "-h") {
                std::cout << "Translate one integer leaf function with an explicit System V ABI.\n"
                    "Usage: aot_compiler_cli --input file.elf --function name --args 0..6 --output file.o\n"
                    "Optional: --emit-llvm file.ll --emit-asm file.s --opt 0..3 --triple target\n"
                    "Arguments and result are uint64_t; no memory, calls, branches, syscalls or relocations.\n";
                return 0;
            }
            if (index + 1 == argc) {
                throw std::runtime_error("Missing value for " + std::string(argument));
            }
            const char* value = argv[++index];
            if (argument == "--input" || argument == "-i") {
                options.InputExecutablePath = value;
            } else if (argument == "--output" || argument == "-o") {
                options.OutputObjectPath = value;
            } else if (argument == "--function") {
                options.FunctionName = value;
            } else if (argument == "--args") {
                options.ArgumentCount = Number(value);
                hasArguments = true;
            } else if (argument == "--emit-llvm") {
                options.OutputLlvmIrPath = value;
            } else if (argument == "--emit-asm" || argument == "-S") {
                options.OutputAssemblyPath = value;
            } else if (argument == "--triple") {
                options.TargetTriple = value;
            } else if (argument == "--opt" || argument == "-O") {
                options.OptimizationLevel = Number(value);
            } else {
                throw std::runtime_error("Unknown option: " + std::string(argument));
            }
        }
        if (!hasArguments || options.FunctionName.empty()) {
            throw std::runtime_error("--function and --args are required; function ABI is never inferred");
        }
        auto statistics = Aot::Compiler::AotCompilerDriver::Compile(options);
        if (!statistics.Success) {
            throw std::runtime_error(statistics.Error);
        }
        std::cout << "Compiled " << options.FunctionName << " (" << statistics.InstructionsLifted
            << " instructions) to " << options.OutputObjectPath.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AOT compilation failed: " << error.what() << '\n';
        return 1;
    }
}
