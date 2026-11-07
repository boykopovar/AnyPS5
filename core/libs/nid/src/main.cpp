#include <nid/BinaryPatcherFactory.hpp>
#include <nid/ExportExclusions.hpp>
#include <nid/NativePath.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> _readFile(const std::string& path) {
    std::ifstream f(Nid::Internal::NativePath(path), std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void _writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(Nid::Internal::NativePath(path), std::ios::binary);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    f.close();
    if (!f) throw std::runtime_error("cannot write: " + path);
}

}

int Run(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: nid_patcher <library_name> [--preserve-exports <unpatched-library> | --preserve-export-list <export-list>] <file> [file2 ...]\n"
                     "       nid_patcher --exec [--only-sce] [--preserve-exports <unpatched-library> | --preserve-export-list <export-list>] <file> [file2 ...]\n"
                     "       nid_patcher --write-export-list <unpatched-library> <export-list>\n";
        return 1;
    }

    if (std::string(argv[1]) == "--write-export-list") {
        if (argc != 4) {
            std::cerr << "FAIL: --write-export-list requires a reference library and an export list\n";
            return 2;
        }
        try {
            std::cout << (Nid::WriteExportList(argv[2], argv[3]) ? "WROTE: " : "UNCHANGED: ") << argv[3] << "\n";
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << argv[3] << ": " << e.what() << "\n";
            return 2;
        }
        return 0;
    }

    Nid::PatchOptions options;
    options.executable = std::string(argv[1]) == "--exec";
    const std::string libraryName = options.executable ? "" : argv[1];
    std::unordered_set<std::string> excludedExports;
    std::vector<std::string> paths;
    try {
        if (!options.executable && libraryName.starts_with("--")) throw std::runtime_error("unknown option: " + libraryName);
        bool hasReference = false;
        for (int i = 2; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--only-sce") {
                if (!options.executable || options.onlySce) throw std::runtime_error("--only-sce requires --exec and may appear once");
                options.onlySce = true;
            } else if (argument == "--preserve-exports" || argument == "--preserve-export-list") {
                if (hasReference || i + 1 == argc) throw std::runtime_error(argument + " requires exactly one reference");
                hasReference = true;
                excludedExports = argument == "--preserve-exports" ? Nid::ReadExportExclusions(argv[++i]) : Nid::ReadExportList(argv[++i]);
            } else {
                if (argument.starts_with("--")) throw std::runtime_error("unknown option: " + argument);
                paths.push_back(argument);
            }
        }
        if (paths.empty()) throw std::runtime_error("no files to patch");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 2;
    }

    for (const std::string& path : paths) {
        try {
            auto binary = _readFile(path);
            const auto patcher = Nid::MakePatcher(binary);
            patcher->PatchNids(binary, libraryName, excludedExports, options);
            _writeFile(path, binary);
            std::cout << "OK: " << path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << path << ": " << e.what() << "\n";
            return 2;
        }
    }

    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    try {
        std::vector<std::string> values;
        values.reserve(static_cast<std::size_t>(argc));
        for (int index = 0; index < argc; ++index) {
            const auto utf8 = std::filesystem::path(argv[index]).u8string();
            values.emplace_back(utf8.begin(), utf8.end());
        }
        std::vector<char*> arguments;
        arguments.reserve(values.size());
        for (auto& value : values) arguments.push_back(value.data());
        return Run(argc, arguments.data());
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 2;
    }
}
#else
int main(int argc, char* argv[]) {
    return Run(argc, argv);
}
#endif
