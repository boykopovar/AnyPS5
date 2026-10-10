#ifndef CORE_RELINKER_CLI_INCLUDE_CLI_HPP
#define CORE_RELINKER_CLI_INCLUDE_CLI_HPP

#include <set>
#include <string>
#include <cstdint>

namespace Cli {

struct Args {
    bool showHelp = false;
    bool skipSyscallCheck = false;
    bool skipSceModule = false;
    bool toIntel = false;
    bool writeRegistry = false;
    bool toWindows = false;
    bool toMacos = false;
    bool lazyBinding = false;
    bool autorun = false;
    bool windowsDiagnostics = false;
    bool windowsGui = false;
    std::uint32_t unusedFilterLevel = 0;
    std::string inputPath;
    std::string outputPath;
    std::string runPath = "$ORIGIN/libs";
    std::set<std::string> excludedSceModules;
};

Args ParseArgs(int argc, char* argv[]);

const char* Usage();

int Autorun(const std::string& absPath, bool toWindows);

}

#endif
