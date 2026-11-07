#include <lifter/Compile.hpp>
#include <lifter/Program.hpp>
#include <lifter/Target.hpp>

#include <llvm/Support/InitLLVM.h>

#include <algorithm>
#include <charconv>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr std::string_view Usage = "usage: ps5lift <input.elf> -o <output.a> [--jobs <n>] [--triple <triple>] [--cpu <cpu>|native] [--features <list>] [--memory tso|relaxed] [--float exact|fast] [--preserve-flags] [--import-nids] [--emit-ir <prefix>] [-O0]\n";

}

int main(int argc, char** argv) {
    const llvm::InitLLVM llvm(argc, argv);
    std::string input;
    Ps5Lift::CompileOptions compile{std::max(1u, std::thread::hardware_concurrency()), "", ""};
    Ps5Lift::LiftOptions lift;
    std::string triple = "aarch64-unknown-linux-gnu";
    std::string cpu;
    std::string features;
    bool optimize = true;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << Usage;
                std::exit(2);
            }
            return argv[++i];
        };
        if (argument == "-o") compile.Archive = value();
        else if (argument == "--jobs") {
            const auto text = value();
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), compile.Jobs);
            if (error != std::errc() || end != text.data() + text.size() || compile.Jobs == 0) {
                std::cerr << "--jobs needs a positive number, not " << text << "\n";
                return 2;
            }
        }
        else if (argument == "--triple") triple = value();
        else if (argument == "--cpu") cpu = value();
        else if (argument == "--features") features = value();
        else if (argument == "--emit-ir") compile.IrPrefix = value();
        else if (argument == "-O0") optimize = false;
        else if (argument == "--preserve-flags") lift.PreserveFlags = true;
        else if (argument == "--import-nids") lift.ImportNids = true;
        else if (argument == "--memory") {
            const auto model = value();
            if (model == "tso") lift.Memory = Ps5Lift::MemoryModel::Tso;
            else if (model == "relaxed") lift.Memory = Ps5Lift::MemoryModel::Relaxed;
            else {
                std::cerr << "unknown memory model " << model << "\n";
                return 2;
            }
        } else if (argument == "--float") {
            const auto model = value();
            if (model == "exact") lift.Float = Ps5Lift::FloatModel::Exact;
            else if (model == "fast") lift.Float = Ps5Lift::FloatModel::Fast;
            else {
                std::cerr << "unknown float model " << model << "\n";
                return 2;
            }
        } else if (input.empty() && !argument.starts_with("-")) input = argument;
        else {
            std::cerr << Usage;
            return 2;
        }
    }
    if (input.empty() || (compile.Archive.empty() && compile.IrPrefix.empty())) {
        std::cerr << Usage;
        return 2;
    }

    Ps5Lift::Target target;
    try {
        target = Ps5Lift::ResolveTarget(llvm::Triple(triple), cpu, features, optimize);
    } catch (const std::exception& error) {
        std::cerr << "ps5lift: " << error.what() << "\n";
        return 2;
    }

    try {
        const auto program = Ps5Lift::GuestProgram::Load(input);
        std::vector<std::map<std::uint64_t, Ps5Lift::GuestFunction>> functions;
        for (const auto& image : program.Images) functions.push_back(Ps5Lift::DiscoverFunctions(image));
        const auto report = Ps5Lift::CompileProgram(program, functions, lift, target, compile);

        std::cerr << "ps5lift: " << report.Functions << " functions, " << report.Instructions << " instructions";
        std::size_t unsupported = 0;
        for (const auto& [mnemonic, count] : report.Unsupported) unsupported += count;
        std::size_t tables = 0;
        std::size_t unguarded = 0;
        std::size_t closed = 0;
        for (const auto& image : functions) {
            for (const auto& [entry, function] : image) {
                tables += function.JumpTables.size();
                unguarded += function.UnguardedJumpTables.size();
                closed += function.ClosedJumpTables.size();
            }
        }
        std::cerr << ", " << unsupported << " unsupported, " << tables << " jump tables (" << unguarded << " sized by layout, " << closed << " closed)\n";
        for (const auto& [mnemonic, count] : report.Unsupported) std::cerr << "  unsupported " << mnemonic << ": " << count << "\n";
    } catch (const std::exception& error) {
        std::cerr << "ps5lift: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
