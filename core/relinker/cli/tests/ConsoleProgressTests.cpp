#include <ConsoleProgress.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main() {
    try {
        std::ostringstream stream;
        Cli::ConsoleProgress progress(stream);
        const std::string name = std::string("title") + '\x1b' + "[2J" + '\n' + '\r' + '\t' + '\x7f' + ".prx";
        progress.IncludeFile(name);
        progress.IncludeFile("中文.prx");
        progress.Report({Domain::ProgressStage::Executable, "folder/" + name, 0, 1});
        const auto initial = stream.str();
        if (initial != "Converting title\\x1B[2J\\x0A\\x0D\\x09\\x7F.prx\n")
            throw std::runtime_error("Executable name was not escaped literally");
        stream.str("");
        progress.Report({Domain::ProgressStage::Modules, name, 1, 1});
        progress.Report({Domain::ProgressStage::Modules, "中文.prx", 1, 1});
        progress.Finish();
        const auto output = stream.str();
        if (output.find('\x1b') != std::string::npos || output.find('\t') != std::string::npos || output.find('\x7f') != std::string::npos)
            throw std::runtime_error("Progress output contains filename controls");
        if (output.find("title\\x1B[2J\\x0A\\x0D\\x09\\x7F.prx | 100% (1/1)") == std::string::npos)
            throw std::runtime_error("Escaped name or completion was lost");
        if (output.find("中文.prx" + std::string(26, ' ') + " | 100% (1/1)") == std::string::npos)
            throw std::runtime_error("Escaped width or UTF-8 name was lost");
        for (unsigned byte = 0; byte < 128; ++byte) {
            if (byte == '/' || byte == '\\') continue;
            std::ostringstream single;
            Cli::ConsoleProgress item(single);
            item.Report({Domain::ProgressStage::Executable, std::string(1, static_cast<char>(byte)), 0, 1});
            if ((byte < 32 || byte == 127) && single.str().find("\\x") == std::string::npos)
                throw std::runtime_error("ASCII control was not escaped");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
