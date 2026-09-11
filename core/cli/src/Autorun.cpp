#include <Cli.hpp>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <cstdlib>

namespace Cli {

void Autorun(const std::string& absPath, bool toWindows) {
    if (!toWindows) {
        std::filesystem::permissions(absPath,
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add);
    }

    const std::string cmd = "\"" + absPath + "\"";
    const int rawCode = std::system(cmd.c_str());

    if (toWindows) {
        std::cout << "Exit code: " << rawCode << '\n';
    } else {
        std::cout << "Raw exit code: " << rawCode << "; Unpacked: " << (rawCode >> 8) << '\n';
    }

    std::cout << "\nPress Enter to exit...\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

}
