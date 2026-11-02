#include <RuntimeLayout.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string Header =
    "Arrange the files like this:\n"
    "folder/\n";

const std::string Libraries =
    "  |-- libs/\n"
    "        |-- *.prx (system libraries)\n"
    "  |-- app0/\n"
    "        |-- <another app files>\n";

std::string print(const std::string& executable, const std::vector<std::string>& modules, const std::vector<std::string>& extraExecutables = {}) {
    std::ostringstream stream;
    Cli::RuntimeLayout::Print(stream, executable, modules, extraExecutables);
    return stream.str();
}

void requireText(const std::string& actual, const std::string& expected, const std::string& what) {
    if (actual != expected) throw std::runtime_error(what + ":\n--- got ---\n" + actual + "--- expected ---\n" + expected);
}

void withoutModules() {
    requireText(print("eboot.elf", {}), Header + "  |-- eboot.elf\n" + Libraries + "\n", "a layout without modules");
}

void extraExecutables() {
    requireText(print("eboot.exe", {}, {"picker.picker.bin.exe", "es-de.es-de.bin.exe"}),
        Header + "  |-- eboot.exe\n  |-- picker.picker.bin.exe\n  |-- es-de.es-de.bin.exe\n" + Libraries + "\n",
        "extra executables were not listed in order after the main one");
}

void moduleTree() {
    const std::vector<std::string> modules{
        "sce_module/libc.prx",
        "Media/Plugins/a.prx",
        "sce_module/libSceFios2.prx",
        "Media/Plugins/b.prx",
        "sce_module/libc.prx",
    };
    requireText(print("eboot.elf", modules),
        Header + "  |-- eboot.elf\n" + Libraries +
        "        |-- Media/\n"
        "              |-- Plugins/\n"
        "                    |-- a.prx\n"
        "                    |-- b.prx\n"
        "        |-- sce_module/\n"
        "              |-- libSceFios2.prx\n"
        "              |-- libc.prx\n"
        "\n",
        "modules were not merged into one sorted tree");
}

void topLevelModule() {
    requireText(print("eboot.elf", {"plugin.prx", "prx/x.prx"}),
        Header + "  |-- eboot.elf\n" + Libraries +
        "        |-- plugin.prx\n"
        "        |-- prx/\n"
        "              |-- x.prx\n"
        "\n",
        "a module outside any directory was not listed as a file");
}

}

int main() {
    try {
        withoutModules();
        extraExecutables();
        moduleTree();
        topLevelModule();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
