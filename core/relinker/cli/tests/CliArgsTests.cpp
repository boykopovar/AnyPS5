#include <Cli.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Cli::Args parse(const std::vector<std::string>& arguments) {
    std::string program = "relinker";
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    argv.push_back(program.data());
    std::vector<std::string> storage = arguments;
    for (auto& argument : storage) argv.push_back(argument.data());
    return Cli::ParseArgs(static_cast<int>(argv.size()), argv.data());
}

std::string join(const std::vector<std::string>& arguments) {
    std::string text;
    for (const auto& argument : arguments) text += (text.empty() ? "" : " ") + argument;
    return text;
}

void expectThrows(const std::vector<std::string>& arguments, const std::string& expectedMessage) {
    try {
        parse(arguments);
    } catch (const std::exception& error) {
        require(error.what() == expectedMessage,
            "wrong error for [" + join(arguments) + "]: got \"" + error.what() + "\", expected \"" + expectedMessage + "\"");
        return;
    }
    throw std::runtime_error("expected an exception for [" + join(arguments) + "]");
}

void expectUsage(const std::vector<std::string>& arguments) {
    try {
        parse(arguments);
    } catch (const std::exception& error) {
        require(std::string(error.what()).rfind("Usage: relinker", 0) == 0,
            "expected a usage message for [" + join(arguments) + "], got \"" + error.what() + "\"");
        return;
    }
    throw std::runtime_error("expected a usage message for [" + join(arguments) + "]");
}

void defaults() {
    const auto args = parse({"input.elf", "output.elf"});
    require(args.inputPath == "input.elf", "input path was not captured");
    require(args.outputPath == "output.elf", "output path was not captured");
    require(!args.skipSyscallCheck && !args.skipSceModule && !args.toIntel && !args.writeRegistry &&
        !args.toWindows && !args.lazyBinding && !args.autorun && !args.windowsDiagnostics && !args.windowsGui,
        "a flag defaulted to true");
    require(args.unusedFilterLevel == 0u, "unused filter level did not default to 0");
    require(args.runPath == "$ORIGIN/libs", "run path did not keep its default");
    require(args.excludedSceModules.empty(), "excluded modules were not empty by default");
}

void booleanFlags() {
    const auto args = parse({"--skip-syscall-check", "--skip-sce-module", "--to-intel", "--registry",
        "--windows", "--lazy-binding", "--autorun", "--windows-diagnostics", "--windows-gui",
        "input.elf", "output.elf"});
    require(args.skipSyscallCheck, "--skip-syscall-check was ignored");
    require(args.skipSceModule, "--skip-sce-module was ignored");
    require(args.toIntel, "--to-intel was ignored");
    require(args.writeRegistry, "--registry was ignored");
    require(args.toWindows, "--windows was ignored");
    require(args.lazyBinding, "--lazy-binding was ignored");
    require(args.autorun, "--autorun was ignored");
    require(args.windowsDiagnostics, "--windows-diagnostics was ignored");
    require(args.windowsGui, "--windows-gui was ignored");
    require(args.inputPath == "input.elf" && args.outputPath == "output.elf", "flags consumed the positional paths");
}

void positionalsInterleaveWithFlags() {
    const auto args = parse({"--windows", "input.elf", "--registry", "output.elf", "--autorun"});
    require(args.toWindows && args.writeRegistry && args.autorun, "a flag after a positional was ignored");
    require(args.inputPath == "input.elf" && args.outputPath == "output.elf", "positionals were misordered around flags");
}

void singleDashIsPositional() {
    const auto args = parse({"-x", "-o"});
    require(args.inputPath == "-x" && args.outputPath == "-o", "a single-dash argument was not treated as a positional");
}

void rpath() {
    const auto args = parse({"--rpath", "/custom/libs", "input.elf", "output.elf"});
    require(args.runPath == "/custom/libs", "--rpath value was not captured");
}

void rpathConsumesNextTokenVerbatim() {
    const auto args = parse({"--rpath", "--windows", "input.elf", "output.elf"});
    require(args.runPath == "--windows", "--rpath did not consume the following token as its value");
    require(!args.toWindows, "the token consumed by --rpath was also parsed as a flag");
}

void rpathWithoutValue() {
    expectThrows({"--rpath"}, "--rpath requires a value");
}

void excludeSceModule() {
    const auto args = parse({"--exclude-sce-module", "libc.prx", "--exclude-sce-module", "libkernel.prx",
        "input.elf", "output.elf"});
    require(args.excludedSceModules.size() == 2, "both excluded modules were not recorded");
    require(args.excludedSceModules.count("libc.prx") == 1 && args.excludedSceModules.count("libkernel.prx") == 1,
        "an excluded module name was lost");
}

void excludeSceModuleConsumesNextTokenVerbatim() {
    const auto args = parse({"--exclude-sce-module", "--skip-sce-module", "input.elf", "output.elf"});
    require(args.excludedSceModules.count("--skip-sce-module") == 1, "--exclude-sce-module did not consume the following token as its value");
    require(!args.skipSceModule, "the token consumed by --exclude-sce-module was also parsed as a flag");
}

void excludeSceModuleWithoutValue() {
    expectThrows({"--exclude-sce-module"}, "--exclude-sce-module requires a file name");
}

void excludeConflictsWithSkip() {
    const std::string message = "--exclude-sce-module conflicts with --skip-sce-module";
    expectThrows({"--skip-sce-module", "--exclude-sce-module", "m", "input.elf", "output.elf"}, message);
    expectThrows({"--exclude-sce-module", "m", "--skip-sce-module", "input.elf", "output.elf"}, message);
}

void unusedFilter() {
    for (const auto& value : {std::string("0"), std::string("1"), std::string("2")}) {
        const auto args = parse({"unused-filter=" + value, "input.elf", "output.elf"});
        require(args.unusedFilterLevel == static_cast<std::uint32_t>(value[0] - '0'),
            "unused-filter=" + value + " produced the wrong level");
    }
}

void unusedFilterRejectsBadValues() {
    const std::string message = "unused-filter must be specified once with a value of 0, 1 or 2";
    expectThrows({"unused-filter=3", "input.elf", "output.elf"}, message);
    expectThrows({"unused-filter=", "input.elf", "output.elf"}, message);
    expectThrows({"unused-filter=12", "input.elf", "output.elf"}, message);
    expectThrows({"unused-filter=a", "input.elf", "output.elf"}, message);
}

void unusedFilterRejectsDuplicates() {
    expectThrows({"unused-filter=1", "unused-filter=2", "input.elf", "output.elf"},
        "unused-filter must be specified once with a value of 0, 1 or 2");
}

void unusedFilterBareWordIsUnknown() {
    expectThrows({"unused-filter", "input.elf", "output.elf"}, "unknown option: unused-filter");
}

void unknownOption() {
    expectThrows({"--nonexistent", "input.elf", "output.elf"}, "unknown option: --nonexistent");
    expectThrows({"--", "input.elf", "output.elf"}, "unknown option: --");
}

void tooManyPositionals() {
    expectThrows({"input.elf", "output.elf", "extra.elf"}, "unexpected argument: extra.elf");
}

void windowsDiagnosticsRequiresWindows() {
    const std::string message = "--windows-diagnostics requires --windows";
    expectThrows({"--windows-diagnostics", "input.elf", "output.elf"}, message);
    const auto before = parse({"--windows-diagnostics", "--windows", "input.elf", "output.elf"});
    require(before.windowsDiagnostics && before.toWindows, "--windows-diagnostics before --windows was rejected");
    const auto after = parse({"--windows", "--windows-diagnostics", "input.elf", "output.elf"});
    require(after.windowsDiagnostics && after.toWindows, "--windows-diagnostics after --windows was rejected");
}

void windowsGuiRequiresWindows() {
    const std::string message = "--windows-gui requires --windows";
    expectThrows({"--windows-gui", "input.elf", "output.elf"}, message);
    const auto ok = parse({"--windows", "--windows-gui", "input.elf", "output.elf"});
    require(ok.windowsGui && ok.toWindows, "--windows-gui with --windows was rejected");
}

void missingPositionals() {
    expectUsage({});
    expectUsage({"input.elf"});
    expectUsage({"--windows"});
    expectUsage({"--rpath", "/libs"});
}

}

int main() {
    try {
        defaults();
        booleanFlags();
        positionalsInterleaveWithFlags();
        singleDashIsPositional();
        rpath();
        rpathConsumesNextTokenVerbatim();
        rpathWithoutValue();
        excludeSceModule();
        excludeSceModuleConsumesNextTokenVerbatim();
        excludeSceModuleWithoutValue();
        excludeConflictsWithSkip();
        unusedFilter();
        unusedFilterRejectsBadValues();
        unusedFilterRejectsDuplicates();
        unusedFilterBareWordIsUnknown();
        unknownOption();
        tooManyPositionals();
        windowsDiagnosticsRequiresWindows();
        windowsGuiRequiresWindows();
        missingPositionals();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
