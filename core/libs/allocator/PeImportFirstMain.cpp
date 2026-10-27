#include "PeImportFirst.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {

int Usage() {
    std::fprintf(stderr, "usage: pe_import_first <file> <dll>\n");
    return 2;
}

int Run(const std::filesystem::path& file, const std::string& dll) {
    try {
        Allocator::MoveImportFirstInFile(file, dll);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pe_import_first: %s: %s\n", file.string().c_str(), error.what());
        return 1;
    }
    return 0;
}

}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return Usage();
    const auto dll = std::filesystem::path(argv[2]).u8string();
    return Run(argv[1], std::string(dll.begin(), dll.end()));
}
#else
int main(int argc, char** argv) {
    if (argc != 3) return Usage();
    return Run(argv[1], argv[2]);
}
#endif
