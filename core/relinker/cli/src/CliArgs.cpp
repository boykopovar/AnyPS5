#include <Cli.hpp>
#include <io/NativePath.hpp>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <iterator>
#include <string_view>
#include <system_error>

namespace Cli {

const char* Usage() {
    return "Usage: relinker [options] <input.elf> <output>\n"
           "       relinker --verify-patches <file> [--verify-output] [options] <path>\n"
           "\n"
           "Output:\n"
           "  --windows                      Produce a Windows PE executable.\n"
           "  --macos                        Produce an x86-64 Mach-O executable; bundled modules become Mach-O libraries.\n"
           "  --windows-diagnostics          Include startup dependency diagnostics. Requires --windows.\n"
           "  --windows-gui                  Select the Windows GUI subsystem. Requires --windows.\n"
           "  --to-intel                     Convert supported AMD-only instructions for Intel CPUs.\n"
           "  --to-rosetta                   Convert as --to-intel and lower what Rosetta lacks too. Implies --to-intel.\n"
           "  --frame-red-zone               Windows: give leaf functions a frame so an exception on a guest thread can unwind.\n"
           "  --rpath <path>                 System library search path. Default: $ORIGIN/libs.\n"
           "  --registry                     Write <output-stem>.registry.json beside the output.\n"
           "  --autorun                      Run the output after conversion and print its exit code.\n"
           "  --dry-run                      Convert in memory and list planned outputs without writing them.\n"
           "\n"
           "Layout:\n"
           "  --sce-module-path <path>       Parent of the module directories. Default: the input file's directory.\n"
           "  --module-dir <dir>             Also relink the modules of this directory. Repeatable.\n"
           "  --executable <path>            Also convert another executable of the title. Repeatable.\n"
           "  --windows-fixed-base <address> Map the lowest load page at a fixed Windows address instead of 0x400000.\n"
           "  --linux-fixed-base <address>   Load the Linux executable at a fixed address instead of 0x400000.\n"
           "  --macos-fixed-base <address>   Map the lowest load page of the macOS executable at a fixed address.\n"
           "\n"
           "Patches:\n"
           "  --patches <file>               Apply the byte patches of this file.\n"
           "  --no-patches                   Do not apply patches or discover relinker-patches.toml.\n"
           "  --verify-patches <file>        Check a patch file against <path> without writing anything.\n"
           "  --verify-output                With --verify-patches, treat <path> as converted output.\n"
           "\n"
           "Imports:\n"
           "  --trace-imports                Pass each function import through the libc.prx call tracer.\n"
           "  unused-filter=0                Keep all imported NID references (default).\n"
           "  unused-filter=1                Filter unused non-PLT imports; preserve PLT imports.\n"
           "  unused-filter=2                Strict unused-import analysis; unsupported cases are an error.\n"
           "\n"
           "Deprecated:\n"
           "  --skip-sce-module              Disable bundled module processing.\n"
           "  --exclude-sce-module <file>    Exclude a bundled module by filename. Repeatable.\n"
           "  --skip-syscall-check           Disable syscall scanning.\n"
           "  --trap-sdk-syscall-stubs       Replace recognized SDK syscall stubs with a trap. For debugging only.\n"
           "  --lazy-binding                 Deprecated. Enable lazy symbol binding instead of eager binding.\n"
           "\n"
           "Other:\n"
           "  --help                         Print this help and exit with code 0.\n"
           "  --version                      Print the build revision and exit with code 0.\n"
           "  Exit codes: 0 success, help or version, 1 invalid arguments or a patch mismatch, 2 conversion failed.\n"
           "Example: relinker input.elf output.elf";
}

namespace {

constexpr std::string_view kVerifyRejectedOptions[] = {
    "--dry-run",
    "--windows", "--macos", "--windows-gui", "--windows-diagnostics", "--windows-fixed-base", "--trace-imports", "--linux-fixed-base", "--macos-fixed-base",
    "--to-intel", "--to-rosetta", "--frame-red-zone", "--skip-syscall-check", "--trap-sdk-syscall-stubs", "unused-filter=",
    "--rpath", "--registry", "--lazy-binding", "--autorun", "--patches", "--no-patches",
};

constexpr std::string_view kModuleDiscoveryOptions[] = {"--sce-module-path", "--module-dir", "--exclude-sce-module", "--skip-sce-module"};

template<std::size_t Count>
bool IsOneOf(const std::string& option, const std::string_view (&names)[Count]) {
    return std::find(std::begin(names), std::end(names), option) != std::end(names);
}

}

static std::uint64_t ParseFixedBase(std::string_view value, const std::uint64_t lowest, const char* error) {
    constexpr std::uint64_t highest = 0x7fffffff0000;
    constexpr std::uint64_t alignment = 0x10000;
    const bool hexadecimal = value.starts_with("0x") || value.starts_with("0X");
    if (hexadecimal) value.remove_prefix(2);
    std::uint64_t address = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), address, hexadecimal ? 16 : 10);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || address < lowest || address > highest || address % alignment != 0)
        throw std::runtime_error(error);
    return address;
}

Args ParseArgs(int argc, char* argv[]) {
    Args args;
    bool unusedFilterSpecified = false;
    bool runPathSpecified = false;
    bool sceModulePathSpecified = false;
    std::vector<std::string> givenOptions;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--", 0) == 0) givenOptions.push_back(arg);
        else if (arg.rfind("unused-filter=", 0) == 0) givenOptions.push_back("unused-filter=");
        if (arg == "--help") {
            args.showHelp = true;
        } else if (arg == "--version") {
            args.showVersion = true;
        } else if (arg == "--windows-fixed-base") {
            if (args.windowsFixedBase) throw std::runtime_error("--windows-fixed-base cannot be repeated");
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--"))
                throw std::runtime_error("--windows-fixed-base requires an address");
            args.windowsFixedBase = ParseFixedBase(argv[++i], 0x20000, "--windows-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000");
        } else if (arg == "--linux-fixed-base") {
            if (args.linuxFixedBase) throw std::runtime_error("--linux-fixed-base cannot be repeated");
            if (i + 1 >= argc || std::string_view(argv[i + 1]).empty() || std::string_view(argv[i + 1]).starts_with("--"))
                throw std::runtime_error("--linux-fixed-base requires an address");
            args.linuxFixedBase = ParseFixedBase(argv[++i], 0x10000, "--linux-fixed-base requires a 64 KiB-aligned address from 0x10000 through 0x7fffffff0000");
        } else if (arg == "--macos-fixed-base") {
            if (args.macosFixedBase) throw std::runtime_error("--macos-fixed-base cannot be repeated");
            if (i + 1 >= argc || std::string_view(argv[i + 1]).empty() || std::string_view(argv[i + 1]).starts_with("--"))
                throw std::runtime_error("--macos-fixed-base requires an address");
            args.macosFixedBase = ParseFixedBase(argv[++i], 0x20000, "--macos-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000");
        } else if (arg == "--skip-syscall-check") {
            args.skipSyscallCheck = true;
        } else if (arg == "--trap-sdk-syscall-stubs") {
            args.trapSdkSyscallStubs = true;
        } else if (arg == "--skip-sce-module") {
            args.skipSceModule = true;
        } else if (arg == "--sce-module-path") {
            if (sceModulePathSpecified) throw std::runtime_error("--sce-module-path must be specified once");
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--")) throw std::runtime_error("--sce-module-path requires a nonempty path");
            args.sceModulePath = argv[++i];
            sceModulePathSpecified = true;
        } else if (arg == "--exclude-sce-module") {
            if (i + 1 >= argc || std::string_view(argv[i + 1]).empty())
                throw std::runtime_error("--exclude-sce-module requires a file name");
            args.excludedSceModules.insert(argv[++i]);
        } else if (arg == "--module-dir") {
            if (i + 1 >= argc || std::string_view(argv[i + 1]).empty())
                throw std::runtime_error("--module-dir requires a directory");
            args.moduleDirectories.push_back(argv[++i]);
        } else if (arg == "--executable") {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--")) throw std::runtime_error("--executable requires a nonempty path");
            args.executables.push_back(argv[++i]);
        } else if (arg == "--to-intel") {
            args.toIntel = true;
        } else if (arg == "--frame-red-zone") {
            args.frameRedZone = true;
        } else if (arg == "--to-rosetta") {
            args.toRosetta = true;
            args.toIntel = true;
        } else if (arg.rfind("unused-filter=", 0) == 0) {
            const std::string value = arg.substr(14);
            if (unusedFilterSpecified || value.size() != 1 || value[0] < '0' || value[0] > '2')
                throw std::runtime_error("unused-filter must be specified once with a value of 0, 1 or 2");
            args.unusedFilterLevel = static_cast<std::uint32_t>(value[0] - '0');
            unusedFilterSpecified = true;
        } else if (arg == "--patches") {
            if (!args.patchesPath.empty()) throw std::runtime_error("--patches cannot be repeated");
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--")) throw std::runtime_error("--patches requires a file");
            args.patchesPath = argv[++i];
        } else if (arg == "--no-patches") {
            args.noPatches = true;
        } else if (arg == "--verify-patches") {
            if (!args.verifyPatchesPath.empty()) throw std::runtime_error("--verify-patches cannot be repeated");
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--")) throw std::runtime_error("--verify-patches requires a file");
            args.verifyPatchesPath = argv[++i];
        } else if (arg == "--verify-output") {
            args.verifyOutput = true;
        } else if (arg == "--registry") {
            args.writeRegistry = true;
        } else if (arg == "--rpath") {
            if (runPathSpecified) throw std::runtime_error("--rpath cannot be repeated");
            if (i + 1 >= argc)
                throw std::runtime_error("--rpath requires a value");
            args.runPath = argv[++i];
            runPathSpecified = true;
        } else if (arg == "--windows") {
            args.toWindows = true;
        } else if (arg == "--macos") {
            args.toMacos = true;
        } else if (arg == "--lazy-binding") {
            args.lazyBinding = true;
        } else if (arg == "--autorun") {
            args.autorun = true;
        } else if (arg == "--dry-run") {
            args.dryRun = true;
        } else if (arg == "--windows-diagnostics") {
            args.windowsDiagnostics = true;
        } else if (arg == "--windows-gui") {
            args.windowsGui = true;
        } else if (arg == "--trace-imports") {
            args.traceImports = true;
        } else if (arg.rfind("--", 0) == 0 || arg == "unused-filter") {
            throw std::runtime_error("unknown option: " + arg);
        } else if (arg.empty()) {
            throw std::runtime_error("input and output paths must be nonempty");
        } else if (args.inputPath.empty()) {
            args.inputPath = arg;
        } else if (args.outputPath.empty()) {
            args.outputPath = arg;
        } else {
            throw std::runtime_error("unexpected argument: " + arg);
        }
    }

    const bool verifying = !args.verifyPatchesPath.empty();
    if (args.dryRun && args.autorun) throw std::runtime_error("--dry-run conflicts with --autorun");
    if (args.verifyOutput && !verifying) throw std::runtime_error("--verify-output requires --verify-patches");
    if (!args.patchesPath.empty() && args.noPatches) throw std::runtime_error("--patches conflicts with --no-patches");
    if (verifying) {
        for (const auto& option : givenOptions) {
            if (IsOneOf(option, kVerifyRejectedOptions))
                throw std::runtime_error(option + " cannot be used with --verify-patches");
            if (args.verifyOutput && IsOneOf(option, kModuleDiscoveryOptions))
                throw std::runtime_error("module discovery options have no effect with --verify-output");
        }
        if (!args.outputPath.empty()) throw std::runtime_error("unexpected argument: " + args.outputPath);
    }

    if (args.skipSceModule && sceModulePathSpecified) throw std::runtime_error("--sce-module-path conflicts with --skip-sce-module");

    if (args.skipSyscallCheck && args.trapSdkSyscallStubs) throw std::runtime_error("--skip-syscall-check conflicts with --trap-sdk-syscall-stubs");

    if (args.skipSceModule && !args.excludedSceModules.empty())
        throw std::runtime_error("--exclude-sce-module conflicts with --skip-sce-module");

    if (args.skipSceModule && !args.moduleDirectories.empty())
        throw std::runtime_error("--module-dir conflicts with --skip-sce-module");

    if (args.skipSceModule && !args.executables.empty())
        throw std::runtime_error("--executable conflicts with --skip-sce-module");

    if (args.toWindows && args.toMacos)
        throw std::runtime_error("--windows conflicts with --macos");
    if (args.toRosetta && args.toWindows)
        throw std::runtime_error("--to-rosetta conflicts with --windows");
    if (args.windowsDiagnostics && !args.toWindows)
        throw std::runtime_error("--windows-diagnostics requires --windows");
    if (args.frameRedZone && !args.toWindows)
        throw std::runtime_error("--frame-red-zone requires --windows");

    if (args.windowsGui && !args.toWindows)
        throw std::runtime_error("--windows-gui requires --windows");

    if (args.windowsFixedBase && !args.toWindows)
        throw std::runtime_error("--windows-fixed-base requires --windows");
    if (args.traceImports && args.lazyBinding)
        throw std::runtime_error("--trace-imports conflicts with --lazy-binding");
    if (args.linuxFixedBase && (args.toWindows || args.toMacos))
        throw std::runtime_error("--linux-fixed-base cannot be used with --windows or --macos");
    if (args.linuxFixedBase && args.lazyBinding)
        throw std::runtime_error("--linux-fixed-base conflicts with --lazy-binding");
    if (args.macosFixedBase && !args.toMacos)
        throw std::runtime_error("--macos-fixed-base requires --macos");

    if (!args.showHelp && !args.showVersion && (args.inputPath.empty() || (!verifying && args.outputPath.empty())))
        throw std::runtime_error(Usage());


    if (!sceModulePathSpecified && !args.skipSceModule && !args.inputPath.empty() && !args.verifyOutput) args.sceModulePath = Io::Utf8Path(std::filesystem::absolute(Io::NativePath(args.inputPath)).parent_path());

    return args;
}

}
