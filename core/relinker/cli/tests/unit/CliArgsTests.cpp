#include <Cli.hpp>
#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Testing;

const std::string UnusedFilterMessage = "unused-filter must be specified once with a value of 0, 1 or 2";

Cli::Args Parse(const std::vector<std::string>& arguments) {
    std::string program = "relinker";
    std::vector<std::string> storage = arguments;
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    argv.push_back(program.data());
    for (auto& argument : storage) argv.push_back(argument.data());
    return Cli::ParseArgs(static_cast<int>(argv.size()), argv.data());
}

void RequireRejected(const std::vector<std::string>& arguments, const std::string& expectedMessage,
                     std::source_location location = std::source_location::current()) {
    RequireThrowsWithMessage<std::runtime_error>([&] { Parse(arguments); }, expectedMessage, "ParseArgs rejection", location);
}

void RequireUsage(const std::vector<std::string>& arguments, std::source_location location = std::source_location::current()) {
    const auto error = RequireThrows<std::runtime_error>([&] { Parse(arguments); }, "ParseArgs usage", location);
    Require(std::string(error.what()).starts_with("Usage: relinker"),
            std::string("expected a usage message, got \"") + error.what() + "\"", location);
}

const Case pathsAreCaptured{"ParseArgs_TwoPositionals_CapturesInputAndOutputPaths", [] {
    const auto args = Parse({"input.elf", "output.elf"});

    RequireEqual(args.inputPath, std::string("input.elf"), "input path");
    RequireEqual(args.outputPath, std::string("output.elf"), "output path");
}};

const Case flagsDefaultToFalse{"ParseArgs_NoFlags_AllFlagsDefaultToFalse", [] {
    const auto args = Parse({"input.elf", "output.elf"});

    Require(!args.skipSyscallCheck, "skipSyscallCheck defaulted to true");
    Require(!args.skipSceModule, "skipSceModule defaulted to true");
    Require(!args.toIntel, "toIntel defaulted to true");
    Require(!args.writeRegistry, "writeRegistry defaulted to true");
    Require(!args.toWindows, "toWindows defaulted to true");
    Require(!args.lazyBinding, "lazyBinding defaulted to true");
    Require(!args.autorun, "autorun defaulted to true");
    Require(!args.windowsDiagnostics, "windowsDiagnostics defaulted to true");
    Require(!args.windowsGui, "windowsGui defaulted to true");
}};

const Case valuesHaveDefaults{"ParseArgs_NoOptions_KeepsDefaultValues", [] {
    const auto args = Parse({"input.elf", "output.elf"});

    RequireEqual(args.unusedFilterLevel, std::uint32_t{0}, "unused filter level");
    RequireEqual(args.runPath, std::string("$ORIGIN/libs"), "run path");
    Require(args.excludedSceModules.empty(), "excluded modules were not empty by default");
}};

const Case booleanFlagsAreSet{"ParseArgs_AllBooleanFlags_SetsEveryFlag", [] {
    const auto args = Parse({"--skip-syscall-check", "--skip-sce-module", "--to-intel", "--registry", "--windows",
                             "--lazy-binding", "--autorun", "--windows-diagnostics", "--windows-gui", "input.elf", "output.elf"});

    Require(args.skipSyscallCheck, "--skip-syscall-check was ignored");
    Require(args.skipSceModule, "--skip-sce-module was ignored");
    Require(args.toIntel, "--to-intel was ignored");
    Require(args.writeRegistry, "--registry was ignored");
    Require(args.toWindows, "--windows was ignored");
    Require(args.lazyBinding, "--lazy-binding was ignored");
    Require(args.autorun, "--autorun was ignored");
    Require(args.windowsDiagnostics, "--windows-diagnostics was ignored");
    Require(args.windowsGui, "--windows-gui was ignored");
}};

const Case booleanFlagsKeepPositionals{"ParseArgs_AllBooleanFlags_KeepsPositionalPaths", [] {
    const auto args = Parse({"--skip-syscall-check", "--skip-sce-module", "--to-intel", "--registry", "--windows",
                             "--lazy-binding", "--autorun", "--windows-diagnostics", "--windows-gui", "input.elf", "output.elf"});

    RequireEqual(args.inputPath, std::string("input.elf"), "input path");
    RequireEqual(args.outputPath, std::string("output.elf"), "output path");
}};

const Case interleavedFlagsAreSet{"ParseArgs_FlagsInterleavedWithPositionals_SetsEveryFlag", [] {
    const auto args = Parse({"--windows", "input.elf", "--registry", "output.elf", "--autorun"});

    Require(args.toWindows, "--windows before the positionals was ignored");
    Require(args.writeRegistry, "--registry between the positionals was ignored");
    Require(args.autorun, "--autorun after the positionals was ignored");
}};

const Case interleavedPositionalsKeepOrder{"ParseArgs_FlagsInterleavedWithPositionals_KeepsPositionalOrder", [] {
    const auto args = Parse({"--windows", "input.elf", "--registry", "output.elf", "--autorun"});

    RequireEqual(args.inputPath, std::string("input.elf"), "input path");
    RequireEqual(args.outputPath, std::string("output.elf"), "output path");
}};

const Case singleDashIsPositional{"ParseArgs_SingleDashArguments_TreatsThemAsPositionals", [] {
    const auto args = Parse({"-x", "-o"});

    RequireEqual(args.inputPath, std::string("-x"), "input path");
    RequireEqual(args.outputPath, std::string("-o"), "output path");
}};

const Case rpathIsCaptured{"ParseArgs_RpathWithValue_CapturesRunPath", [] {
    const auto args = Parse({"--rpath", "/custom/libs", "input.elf", "output.elf"});

    RequireEqual(args.runPath, std::string("/custom/libs"), "run path");
}};

const Case rpathConsumesFlagToken{"ParseArgs_RpathFollowedByFlag_ConsumesFlagAsValue", [] {
    const auto args = Parse({"--rpath", "--windows", "input.elf", "output.elf"});

    RequireEqual(args.runPath, std::string("--windows"), "run path");
    Require(!args.toWindows, "the token consumed by --rpath was also parsed as a flag");
}};

const Case rpathWithoutValueIsRejected{"ParseArgs_RpathWithoutValue_Throws", [] {
    RequireRejected({"--rpath"}, "--rpath requires a value");
}};

const Case excludedModulesAreRecorded{"ParseArgs_ExcludeSceModuleTwice_RecordsBothModules", [] {
    const auto args = Parse({"--exclude-sce-module", "libc.prx", "--exclude-sce-module", "libkernel.prx", "input.elf", "output.elf"});

    RequireEqual(args.excludedSceModules.size(), std::size_t{2}, "excluded module count");
    RequireEqual(args.excludedSceModules.count("libc.prx"), std::size_t{1}, "libc.prx exclusions");
    RequireEqual(args.excludedSceModules.count("libkernel.prx"), std::size_t{1}, "libkernel.prx exclusions");
}};

const Case excludeConsumesFlagToken{"ParseArgs_ExcludeSceModuleFollowedByFlag_ConsumesFlagAsValue", [] {
    const auto args = Parse({"--exclude-sce-module", "--skip-sce-module", "input.elf", "output.elf"});

    RequireEqual(args.excludedSceModules.count("--skip-sce-module"), std::size_t{1}, "--skip-sce-module exclusions");
    Require(!args.skipSceModule, "the token consumed by --exclude-sce-module was also parsed as a flag");
}};

const Case excludeWithoutValueIsRejected{"ParseArgs_ExcludeSceModuleWithoutValue_Throws", [] {
    RequireRejected({"--exclude-sce-module"}, "--exclude-sce-module requires a file name");
}};

const Case excludeAfterSkipIsRejected{"ParseArgs_ExcludeSceModuleAfterSkipSceModule_Throws", [] {
    RequireRejected({"--skip-sce-module", "--exclude-sce-module", "m", "input.elf", "output.elf"},
                    "--exclude-sce-module conflicts with --skip-sce-module");
}};

const Case skipAfterExcludeIsRejected{"ParseArgs_SkipSceModuleAfterExcludeSceModule_Throws", [] {
    RequireRejected({"--exclude-sce-module", "m", "--skip-sce-module", "input.elf", "output.elf"},
                    "--exclude-sce-module conflicts with --skip-sce-module");
}};

const Case unusedFilterZero{"ParseArgs_UnusedFilterZero_SetsLevelZero", [] {
    const auto args = Parse({"unused-filter=0", "input.elf", "output.elf"});

    RequireEqual(args.unusedFilterLevel, std::uint32_t{0}, "unused filter level");
}};

const Case unusedFilterOne{"ParseArgs_UnusedFilterOne_SetsLevelOne", [] {
    const auto args = Parse({"unused-filter=1", "input.elf", "output.elf"});

    RequireEqual(args.unusedFilterLevel, std::uint32_t{1}, "unused filter level");
}};

const Case unusedFilterTwo{"ParseArgs_UnusedFilterTwo_SetsLevelTwo", [] {
    const auto args = Parse({"unused-filter=2", "input.elf", "output.elf"});

    RequireEqual(args.unusedFilterLevel, std::uint32_t{2}, "unused filter level");
}};

const Case unusedFilterOutOfRange{"ParseArgs_UnusedFilterThree_Throws", [] {
    RequireRejected({"unused-filter=3", "input.elf", "output.elf"}, UnusedFilterMessage);
}};

const Case unusedFilterEmpty{"ParseArgs_UnusedFilterEmptyValue_Throws", [] {
    RequireRejected({"unused-filter=", "input.elf", "output.elf"}, UnusedFilterMessage);
}};

const Case unusedFilterMultiDigit{"ParseArgs_UnusedFilterMultiDigitValue_Throws", [] {
    RequireRejected({"unused-filter=12", "input.elf", "output.elf"}, UnusedFilterMessage);
}};

const Case unusedFilterNonNumeric{"ParseArgs_UnusedFilterNonNumericValue_Throws", [] {
    RequireRejected({"unused-filter=a", "input.elf", "output.elf"}, UnusedFilterMessage);
}};

const Case unusedFilterDuplicate{"ParseArgs_UnusedFilterTwice_Throws", [] {
    RequireRejected({"unused-filter=1", "unused-filter=2", "input.elf", "output.elf"}, UnusedFilterMessage);
}};

const Case unusedFilterBareWord{"ParseArgs_UnusedFilterWithoutEquals_ThrowsUnknownOption", [] {
    RequireRejected({"unused-filter", "input.elf", "output.elf"}, "unknown option: unused-filter");
}};

const Case unknownOption{"ParseArgs_UnknownDoubleDashOption_Throws", [] {
    RequireRejected({"--nonexistent", "input.elf", "output.elf"}, "unknown option: --nonexistent");
}};

const Case bareDoubleDash{"ParseArgs_BareDoubleDash_ThrowsUnknownOption", [] {
    RequireRejected({"--", "input.elf", "output.elf"}, "unknown option: --");
}};

const Case tooManyPositionals{"ParseArgs_ThreePositionals_Throws", [] {
    RequireRejected({"input.elf", "output.elf", "extra.elf"}, "unexpected argument: extra.elf");
}};

const Case diagnosticsWithoutWindows{"ParseArgs_WindowsDiagnosticsWithoutWindows_Throws", [] {
    RequireRejected({"--windows-diagnostics", "input.elf", "output.elf"}, "--windows-diagnostics requires --windows");
}};

const Case diagnosticsBeforeWindows{"ParseArgs_WindowsDiagnosticsBeforeWindows_SetsBothFlags", [] {
    const auto args = Parse({"--windows-diagnostics", "--windows", "input.elf", "output.elf"});

    Require(args.windowsDiagnostics, "--windows-diagnostics before --windows was ignored");
    Require(args.toWindows, "--windows after --windows-diagnostics was ignored");
}};

const Case diagnosticsAfterWindows{"ParseArgs_WindowsDiagnosticsAfterWindows_SetsBothFlags", [] {
    const auto args = Parse({"--windows", "--windows-diagnostics", "input.elf", "output.elf"});

    Require(args.windowsDiagnostics, "--windows-diagnostics after --windows was ignored");
    Require(args.toWindows, "--windows before --windows-diagnostics was ignored");
}};

const Case guiWithoutWindows{"ParseArgs_WindowsGuiWithoutWindows_Throws", [] {
    RequireRejected({"--windows-gui", "input.elf", "output.elf"}, "--windows-gui requires --windows");
}};

const Case guiWithWindows{"ParseArgs_WindowsGuiWithWindows_SetsBothFlags", [] {
    const auto args = Parse({"--windows", "--windows-gui", "input.elf", "output.elf"});

    Require(args.windowsGui, "--windows-gui with --windows was ignored");
    Require(args.toWindows, "--windows with --windows-gui was ignored");
}};

const Case noArguments{"ParseArgs_NoArguments_ThrowsUsage", [] {
    RequireUsage({});
}};

const Case onlyInput{"ParseArgs_OnlyInputPath_ThrowsUsage", [] {
    RequireUsage({"input.elf"});
}};

const Case onlyFlag{"ParseArgs_OnlyFlag_ThrowsUsage", [] {
    RequireUsage({"--windows"});
}};

const Case onlyRpath{"ParseArgs_OnlyRpathWithValue_ThrowsUsage", [] {
    RequireUsage({"--rpath", "/libs"});
}};

} // namespace
