#include <nid/BinaryPatcherFactory.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> _readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void _writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: nid_patcher <library_name> <file.so> [file2.so ...]\n";
        return 1;
    }

    const std::string libraryName = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string path = argv[i];
        try {
            auto binary = _readFile(path);
            const auto patcher = Nid::MakePatcher(binary);
            patcher->PatchNids(binary, libraryName);
            _writeFile(path, binary);
            std::cout << "OK: " << path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << path << ": " << e.what() << "\n";
            return 2;
        }
    }

    return 0;
}
