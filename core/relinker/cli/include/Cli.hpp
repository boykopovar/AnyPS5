#ifndef CORE_RELINKER_CLI_INCLUDE_CLI_HPP
#define CORE_RELINKER_CLI_INCLUDE_CLI_HPP

#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace Cli {

struct Args {
    bool showHelp = false;
    bool skipSyscallCheck = false;
    bool trapSdkSyscallStubs = false;
    bool skipSceModule = false;
    bool toIntel = false;
    bool toRosetta = false;
    bool frameRedZone = false;
    bool writeRegistry = false;
    bool toWindows = false;
    bool toMacos = false;
    bool lazyBinding = false;
    bool autorun = false;
    bool windowsDiagnostics = false;
    bool windowsGui = false;
    std::optional<std::uint64_t> windowsFixedBase;
    bool traceImports = false;
    std::optional<std::uint64_t> linuxFixedBase;
    std::optional<std::uint64_t> macosFixedBase;
    std::uint32_t unusedFilterLevel = 0;
    std::string inputPath;
    std::string outputPath;
    std::string sceModulePath;
    std::string patchesPath;
    bool noPatches = false;
    std::string verifyPatchesPath;
    bool verifyOutput = false;
    std::string runPath = "$ORIGIN/libs";
    std::set<std::string> excludedSceModules;
    std::vector<std::string> moduleDirectories;
    std::vector<std::string> executables;
};

Args ParseArgs(int argc, char* argv[]);

const char* Usage();

int Autorun(const std::string& absPath, bool toWindows);

}

#endif
