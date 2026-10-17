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
        missingPositionals();
        windowsFixedBase();
        linuxFixedBase();
        macosFixedBase();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
