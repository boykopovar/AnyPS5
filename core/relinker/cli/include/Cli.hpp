#pragma once
#include <string>
#include <cstdint>

namespace Cli {

struct Args {
    bool skipSyscallCheck = false;
    bool toIntel = false;
    bool writeRegistry = false;
    bool toWindows = false;
    bool lazyBinding = false;
    bool autorun = false;
    std::uint32_t unusedFilterLevel = 2;
    std::string inputPath;
    std::string outputPath;
    std::string runPath = "$ORIGIN/libs";
};

Args ParseArgs(int argc, char* argv[]);

void Autorun(const std::string& absPath, bool toWindows);

}
