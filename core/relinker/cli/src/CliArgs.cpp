#include <Cli.hpp>
#include <io/NativePath.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Cli {

const char* Usage() {
    return "Usage: relinker [--help] [--windows | --macos] [--windows-diagnostics] [--windows-gui] [--skip-syscall-check] [--skip-sce-module | --sce-module-path <path>] [--exclude-sce-module <file>]... [--to-intel] [unused-filter=0|1|2] [--registry] [--rpath <path>] [--lazy-binding] [--autorun] <input.elf> <output.elf>\n"
           "Example: relinker input.elf output.elf";
}

Args ParseArgs(int argc, char* argv[]) {
    Args args;
    bool unusedFilterSpecified = false;
    bool sceModulePathSpecified = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            args.showHelp = true;
        } else if (arg == "--skip-syscall-check") {
            args.skipSyscallCheck = true;
        } else if (arg == "--skip-sce-module") {
            args.skipSceModule = true;
        } else if (arg == "--sce-module-path") {
            if (sceModulePathSpecified) throw std::runtime_error("--sce-module-path must be specified once");
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || std::string(argv[i + 1]).starts_with("--")) throw std::runtime_error("--sce-module-path requires a nonempty path");
            args.sceModulePath = argv[++i];
            sceModulePathSpecified = true;
        } else if (arg == "--exclude-sce-module") {
            if (i + 1 >= argc)
                throw std::runtime_error("--exclude-sce-module requires a file name");
            args.excludedSceModules.insert(argv[++i]);
        } else if (arg == "--to-intel") {
            args.toIntel = true;
        } else if (arg.rfind("unused-filter=", 0) == 0) {
            const std::string value = arg.substr(14);
            if (unusedFilterSpecified || value.size() != 1 || value[0] < '0' || value[0] > '2')
                throw std::runtime_error("unused-filter must be specified once with a value of 0, 1 or 2");
            args.unusedFilterLevel = static_cast<std::uint32_t>(value[0] - '0');
            unusedFilterSpecified = true;
        } else if (arg == "--registry") {
            args.writeRegistry = true;
        } else if (arg == "--rpath") {
            if (i + 1 >= argc)
                throw std::runtime_error("--rpath requires a value");
            args.runPath = argv[++i];
        } else if (arg == "--windows") {
            args.toWindows = true;
        } else if (arg == "--macos") {
            args.toMacos = true;
        } else if (arg == "--lazy-binding") {
            args.lazyBinding = true;
        } else if (arg == "--autorun") {
            args.autorun = true;
        } else if (arg == "--windows-diagnostics") {
            args.windowsDiagnostics = true;
        } else if (arg == "--windows-gui") {
            args.windowsGui = true;
        } else if (arg.rfind("--", 0) == 0 || arg == "unused-filter") {
            throw std::runtime_error("unknown option: " + arg);
        } else if (args.inputPath.empty()) {
            args.inputPath = arg;
        } else if (args.outputPath.empty()) {
            args.outputPath = arg;
        } else {
            throw std::runtime_error("unexpected argument: " + arg);
        }
    }

    if (args.skipSceModule && sceModulePathSpecified) throw std::runtime_error("--sce-module-path conflicts with --skip-sce-module");

    if (args.skipSceModule && !args.excludedSceModules.empty())
        throw std::runtime_error("--exclude-sce-module conflicts with --skip-sce-module");

    if (args.toWindows && args.toMacos)
        throw std::runtime_error("--windows conflicts with --macos");
    if (args.windowsDiagnostics && !args.toWindows)
        throw std::runtime_error("--windows-diagnostics requires --windows");

    if (args.windowsGui && !args.toWindows)
        throw std::runtime_error("--windows-gui requires --windows");

    if (!args.showHelp && (args.inputPath.empty() || args.outputPath.empty()))
        throw std::runtime_error(Usage());


    if (!sceModulePathSpecified && !args.skipSceModule && !args.inputPath.empty()) args.sceModulePath = Io::Utf8Path(std::filesystem::absolute(Io::NativePath(args.inputPath)).parent_path());

    return args;
}

}
