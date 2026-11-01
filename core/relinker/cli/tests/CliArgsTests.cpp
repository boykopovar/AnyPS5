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
    require(!args.skipSyscallCheck && !args.trapSdkSyscallStubs && !args.skipSceModule && !args.toIntel && !args.writeRegistry &&
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

void trapSdkSyscallStubs() {
    const auto args = parse({"--trap-sdk-syscall-stubs", "input.elf", "output.elf"});
    require(args.trapSdkSyscallStubs, "--trap-sdk-syscall-stubs was ignored");
    expectThrows({"--skip-syscall-check", "--trap-sdk-syscall-stubs", "input.elf", "output.elf"},
        "--skip-syscall-check conflicts with --trap-sdk-syscall-stubs");
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
    expectThrows({"--exclude-sce-module", "", "input.elf", "output.elf"}, "--exclude-sce-module requires a file name");
    const auto args = parse({"--exclude-sce-module", "module with spaces.prx", "input.elf", "output.elf"});
    require(args.excludedSceModules.count("module with spaces.prx") == 1, "a nonempty excluded file name was changed");
}

void excludeConflictsWithSkip() {
    const std::string message = "--exclude-sce-module conflicts with --skip-sce-module";
    expectThrows({"--skip-sce-module", "--exclude-sce-module", "m", "input.elf", "output.elf"}, message);
    expectThrows({"--exclude-sce-module", "m", "--skip-sce-module", "input.elf", "output.elf"}, message);
}

void executables() {
    const auto args = parse({"--executable", "picker/picker.bin", "input.elf", "--executable", "es-de/es-de.bin", "output.elf"});
    require(args.executables == std::vector<std::string>{"picker/picker.bin", "es-de/es-de.bin"}, "--executable paths were not recorded in order");
    expectThrows({"--executable"}, "--executable requires a nonempty path");
    expectThrows({"--executable", "", "input.elf", "output.elf"}, "--executable requires a nonempty path");
    expectThrows({"--executable", "--registry", "input.elf", "output.elf"}, "--executable requires a nonempty path");
    expectThrows({"--skip-sce-module", "--executable", "picker.bin", "input.elf", "output.elf"}, "--executable conflicts with --skip-sce-module");
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

void emptyPositionals() {
    for (const auto& arguments : std::vector<std::vector<std::string>>{
            {"", "input.elf", "output.elf"}, {"input.elf", "", "output.elf"},
            {"", "output.elf"}, {"input.elf", ""}})
        expectThrows(arguments, "input and output paths must be nonempty");
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

void frameRedZoneRequiresWindows() {
    const std::string message = "--frame-red-zone requires --windows";
    expectThrows({"--frame-red-zone", "input.elf", "output.elf"}, message);
    expectThrows({"--macos", "--frame-red-zone", "input.elf", "output.elf"}, message);
    const auto ok = parse({"--windows", "--frame-red-zone", "input.elf", "output.elf"});
    require(ok.frameRedZone && ok.toWindows, "--frame-red-zone with --windows was rejected");
}

void missingPositionals() {
    expectUsage({});
    expectUsage({"input.elf"});
    expectUsage({"--windows"});
    expectUsage({"--rpath", "/libs"});
}

void windowsFixedBase() {
    require(!parse({"--windows", "input.elf", "output.exe"}).windowsFixedBase, "fixed base was enabled by default");
    for (const auto& value : {"0x400000", "0X400000", "4194304"}) {
        const auto args = parse({"--windows-fixed-base", value, "--windows", "input.elf", "output.exe"});
        require(args.windowsFixedBase == 0x400000, "fixed base was parsed incorrectly");
    }
    require(parse({"--windows", "--windows-fixed-base", "0x20000", "input.elf", "output.exe"}).windowsFixedBase == 0x20000, "lowest fixed base was rejected");
    require(parse({"--windows", "--windows-fixed-base", "0x7fffffff0000", "input.elf", "output.exe"}).windowsFixedBase == 0x7fffffff0000, "highest fixed base was rejected");
    require(parse({"--windows", "--windows-fixed-base", "0x400000000", "input.elf", "output.exe"}).windowsFixedBase == 0x400000000, "fixed base was truncated to 32 bits");
    for (const auto& value : {"0", "0x10000", "0x400001", "0x", "-4194304", "+4194304", " 4194304", "0x400000junk", "18446744073709551616", "0x800000000000"})
        expectThrows({"--windows", "--windows-fixed-base", value, "input.elf", "output.exe"}, "--windows-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000");
    expectThrows({"--windows-fixed-base"}, "--windows-fixed-base requires an address");
    expectThrows({"--windows-fixed-base", "--windows"}, "--windows-fixed-base requires an address");
    expectThrows({"--windows-fixed-base", "0x400000", "input.elf", "output.exe"}, "--windows-fixed-base requires --windows");
    expectThrows({"--macos", "--windows-fixed-base", "0x400000", "input.elf", "output.exe"}, "--windows-fixed-base requires --windows");
    expectThrows({"--windows", "--windows-fixed-base", "0x400000", "--windows-fixed-base", "0x800000", "input.elf", "output.exe"}, "--windows-fixed-base cannot be repeated");
}

void linuxFixedBase() {
    require(!parse({"input.elf", "output.elf"}).linuxFixedBase, "fixed base was enabled by default");
    for (const auto& value : {"0x400000", "0X400000", "4194304"})
        require(parse({"--linux-fixed-base", value, "input.elf", "output.elf"}).linuxFixedBase == 0x400000, "fixed base was parsed incorrectly");
    require(parse({"input.elf", "--linux-fixed-base", "0x10000", "output.elf"}).linuxFixedBase == 0x10000, "lowest fixed base was rejected");
    require(parse({"--linux-fixed-base", "0x7fffffff0000", "input.elf", "output.elf"}).linuxFixedBase == 0x7fffffff0000, "highest fixed base was rejected");
    require(parse({"--linux-fixed-base", "0x400000000", "input.elf", "output.elf"}).linuxFixedBase == 0x400000000, "fixed base was truncated to 32 bits");
    const std::string range = "--linux-fixed-base requires a 64 KiB-aligned address from 0x10000 through 0x7fffffff0000";
    for (const auto& value : {"0", "0x0", "0xf000", "0x400001", "0x408000", "0x", "0X", "-4194304", "+4194304", " 4194304", "4194304 ", "0x400000junk", "0x-400000", "18446744073709551616", "0x800000000000", "0x7fffffff10000"})
        expectThrows({"--linux-fixed-base", value, "input.elf", "output.elf"}, range);
    expectThrows({"--linux-fixed-base"}, "--linux-fixed-base requires an address");
    expectThrows({"--linux-fixed-base", "", "input.elf", "output.elf"}, "--linux-fixed-base requires an address");
    expectThrows({"--linux-fixed-base", "--windows", "input.elf", "output.elf"}, "--linux-fixed-base requires an address");
    expectThrows({"--linux-fixed-base", "0x400000", "--linux-fixed-base", "0x400000", "input.elf", "output.elf"}, "--linux-fixed-base cannot be repeated");
    expectThrows({"--windows", "--linux-fixed-base", "0x400000", "input.elf", "output.exe"}, "--linux-fixed-base cannot be used with --windows or --macos");
    expectThrows({"--linux-fixed-base", "0x400000", "--macos", "input.elf", "output"}, "--linux-fixed-base cannot be used with --windows or --macos");
    expectThrows({"--lazy-binding", "--linux-fixed-base", "0x400000", "input.elf", "output.elf"}, "--linux-fixed-base conflicts with --lazy-binding");
}

void macosFixedBase() {
    require(!parse({"--macos", "input.elf", "output"}).macosFixedBase, "fixed base was enabled by default");
    for (const auto& value : {"0x400000", "0X400000", "4194304"})
        require(parse({"--macos-fixed-base", value, "--macos", "input.elf", "output"}).macosFixedBase == 0x400000, "fixed base was parsed incorrectly");
    require(parse({"--macos", "input.elf", "--macos-fixed-base", "0x20000", "output"}).macosFixedBase == 0x20000, "lowest fixed base was rejected");
    require(parse({"--macos", "--macos-fixed-base", "0x7fffffff0000", "input.elf", "output"}).macosFixedBase == 0x7fffffff0000, "highest fixed base was rejected");
    require(parse({"--macos", "--macos-fixed-base", "0x400000000", "input.elf", "output"}).macosFixedBase == 0x400000000, "fixed base was truncated to 32 bits");
    const std::string range = "--macos-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000";
    for (const auto& value : {"0", "0x10000", "0x400001", "0x408000", "0x", "-4194304", "+4194304", " 4194304", "0x400000junk", "18446744073709551616", "0x800000000000"})
        expectThrows({"--macos", "--macos-fixed-base", value, "input.elf", "output"}, range);
    expectThrows({"--macos-fixed-base"}, "--macos-fixed-base requires an address");
    expectThrows({"--macos-fixed-base", "", "--macos", "input.elf", "output"}, "--macos-fixed-base requires an address");
    expectThrows({"--macos-fixed-base", "--macos", "input.elf", "output"}, "--macos-fixed-base requires an address");
    expectThrows({"--macos", "--macos-fixed-base", "0x400000", "--macos-fixed-base", "0x400000", "input.elf", "output"}, "--macos-fixed-base cannot be repeated");
    expectThrows({"--macos-fixed-base", "0x400000", "input.elf", "output.elf"}, "--macos-fixed-base requires --macos");
    expectThrows({"--windows", "--macos-fixed-base", "0x400000", "input.elf", "output.exe"}, "--macos-fixed-base requires --macos");
}

}

void moduleDirectories() {
    const auto args = parse({"--module-dir", "Media/Plugins", "input.elf", "--module-dir", "extra", "output.elf"});
    require(args.moduleDirectories == std::vector<std::string>{"Media/Plugins", "extra"}, "--module-dir directories were not recorded in order");
    require(args.inputPath == "input.elf" && args.outputPath == "output.elf", "--module-dir consumed a positional path");
    expectThrows({"--module-dir"}, "--module-dir requires a directory");
    expectThrows({"--module-dir", "", "input.elf", "output.elf"}, "--module-dir requires a directory");
    const auto literal = parse({"--module-dir", "--windows", "input.elf", "output.elf"});
    require(literal.moduleDirectories == std::vector<std::string>{"--windows"} && !literal.toWindows,
        "--module-dir must keep consuming the next token verbatim");
    const auto spaced = parse({"--module-dir", "module with spaces", "input.elf", "output.elf"});
    require(spaced.moduleDirectories == std::vector<std::string>{"module with spaces"}, "a nonempty module directory was changed");
    expectThrows({"--skip-sce-module", "--module-dir", "Media/Plugins", "input.elf", "output.elf"}, "--module-dir conflicts with --skip-sce-module");
    expectThrows({"--module-dir", "Media/Plugins", "--skip-sce-module", "input.elf", "output.elf"}, "--module-dir conflicts with --skip-sce-module");
}

void sceModulePath() {
    const auto args = parse({"--sce-module-path", "modules", "input.elf", "output.elf"});
    require(args.sceModulePath == "modules", "--sce-module-path value was not captured");
    expectThrows({"--sce-module-path"}, "--sce-module-path requires a nonempty path");
    expectThrows({"--sce-module-path", "", "input.elf", "output.elf"}, "--sce-module-path requires a nonempty path");
    expectThrows({"--sce-module-path", "--registry", "input.elf", "output.elf"}, "--sce-module-path requires a nonempty path");
    expectThrows({"--sce-module-path", "a", "--sce-module-path", "b", "input.elf", "output.elf"}, "--sce-module-path must be specified once");
    expectThrows({"--skip-sce-module", "--sce-module-path", "modules", "input.elf", "output.elf"}, "--sce-module-path conflicts with --skip-sce-module");
    expectThrows({"--sce-module-path", "modules", "--skip-sce-module", "input.elf", "output.elf"}, "--sce-module-path conflicts with --skip-sce-module");
}

void targetSelection() {
    const auto macos = parse({"--macos", "input.elf", "output.elf"});
    require(macos.toMacos && !macos.toWindows, "--macos did not select only the macOS target");
    const auto rosetta = parse({"--to-rosetta", "input.elf", "output.elf"});
    require(rosetta.toRosetta && rosetta.toIntel, "--to-rosetta did not imply --to-intel");
    const auto rosettaOnMacos = parse({"--macos", "--to-rosetta", "input.elf", "output.elf"});
    require(rosettaOnMacos.toMacos && rosettaOnMacos.toRosetta && rosettaOnMacos.toIntel, "--to-rosetta was rejected with --macos");
    const std::string targetConflict = "--windows conflicts with --macos";
    expectThrows({"--windows", "--macos", "input.elf", "output.elf"}, targetConflict);
    expectThrows({"--macos", "--windows", "input.elf", "output.elf"}, targetConflict);
    const std::string rosettaConflict = "--to-rosetta conflicts with --windows";
    expectThrows({"--windows", "--to-rosetta", "input.elf", "output.elf"}, rosettaConflict);
    expectThrows({"--to-rosetta", "--windows", "input.elf", "output.elf"}, rosettaConflict);
}

void patchOptions() {
    auto args = parse({"--patches", "p.toml", "in.elf", "out.elf"});
    require(args.patchesPath == "p.toml" && !args.noPatches, "--patches value");
    args = parse({"--no-patches", "in.elf", "out.elf"});
    require(args.noPatches && args.patchesPath.empty(), "--no-patches");
    expectThrows({"--patches", "p.toml", "--no-patches", "in.elf", "out.elf"}, "--patches conflicts with --no-patches");
    expectThrows({"--patches", "p.toml", "--patches", "q.toml", "in.elf", "out.elf"}, "--patches cannot be repeated");
    expectThrows({"in.elf", "out.elf", "--patches"}, "--patches requires a file");
}

void verifyMode() {
    auto args = parse({"--verify-patches", "p.toml", "in.elf"});
    require(args.verifyPatchesPath == "p.toml" && args.inputPath == "in.elf" && args.outputPath.empty() && !args.verifyOutput, "verify positional");
    require(!args.sceModulePath.empty(), "input mode keeps the default module path");
    args = parse({"in.elf", "--verify-patches", "p.toml", "--sce-module-path", "m", "--module-dir", "d", "--exclude-sce-module", "x.prx", "--executable", "a/b.bin"});
    require(args.sceModulePath == "m" && args.executables.size() == 1, "input mode options");
    args = parse({"--verify-patches", "p.toml", "--skip-sce-module", "in.elf"});
    require(args.skipSceModule, "input mode --skip-sce-module");
    args = parse({"--verify-patches", "p.toml", "--verify-output", "out.elf", "--executable", "a/b.bin"});
    require(args.verifyOutput && args.sceModulePath.empty() && args.executables.size() == 1, "output mode accepts --executable");
    const std::string noEffect = "module discovery options have no effect with --verify-output";
    expectThrows({"--verify-patches", "p.toml", "--verify-output", "out.elf", "--sce-module-path", "m"}, noEffect);
    expectThrows({"--verify-patches", "p.toml", "--verify-output", "out.elf", "--module-dir", "d"}, noEffect);
    expectThrows({"--verify-patches", "p.toml", "--verify-output", "out.elf", "--exclude-sce-module", "x.prx"}, noEffect);
    expectThrows({"--verify-patches", "p.toml", "--verify-output", "out.elf", "--skip-sce-module"}, noEffect);
    expectThrows({"--verify-patches", "p.toml", "in.elf", "extra"}, "unexpected argument: extra");
    for (const auto* option : {"--windows", "--macos", "--to-intel", "--to-rosetta", "--frame-red-zone", "--trace-imports", "--lazy-binding", "--autorun", "--registry", "--skip-syscall-check", "--no-patches"})
        expectThrows({"--verify-patches", "p.toml", option, "in.elf"}, std::string(option) + " cannot be used with --verify-patches");
    expectThrows({"--verify-patches", "p.toml", "--rpath", "x", "in.elf"}, "--rpath cannot be used with --verify-patches");
    expectThrows({"--verify-patches", "p.toml", "--patches", "q.toml", "in.elf"}, "--patches cannot be used with --verify-patches");
    expectThrows({"--verify-patches", "p.toml", "unused-filter=1", "in.elf"}, "unused-filter= cannot be used with --verify-patches");
    expectThrows({"--verify-patches", "p.toml", "--linux-fixed-base", "0x10000", "in.elf"}, "--linux-fixed-base cannot be used with --verify-patches");
    expectThrows({"--verify-patches", "p.toml", "--macos-fixed-base", "0x20000", "in.elf"}, "--macos-fixed-base cannot be used with --verify-patches");
    expectThrows({"--verify-output", "in.elf", "out.elf"}, "--verify-output requires --verify-patches");
    expectThrows({"--verify-patches"}, "--verify-patches requires a file");
    expectUsage({"--verify-patches", "p.toml"});
}

int main() {
    try {
        defaults();
        booleanFlags();
        trapSdkSyscallStubs();
        positionalsInterleaveWithFlags();
        singleDashIsPositional();
        rpath();
        rpathConsumesNextTokenVerbatim();
        rpathWithoutValue();
        excludeSceModule();
        excludeSceModuleConsumesNextTokenVerbatim();
        excludeSceModuleWithoutValue();
        excludeConflictsWithSkip();
        moduleDirectories();
        sceModulePath();
        targetSelection();
        executables();
        unusedFilter();
        unusedFilterRejectsBadValues();
        unusedFilterRejectsDuplicates();
        unusedFilterBareWordIsUnknown();
        unknownOption();
        emptyPositionals();
        tooManyPositionals();
        windowsDiagnosticsRequiresWindows();
        windowsGuiRequiresWindows();
        frameRedZoneRequiresWindows();
        missingPositionals();
        windowsFixedBase();
        linuxFixedBase();
        patchOptions();
        verifyMode();
        macosFixedBase();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
