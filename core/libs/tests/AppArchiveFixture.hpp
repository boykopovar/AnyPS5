#ifndef CORE_LIBS_TESTS_APPARCHIVEFIXTURE_HPP
#define CORE_LIBS_TESTS_APPARCHIVEFIXTURE_HPP

#include <zarchive/zarchivewriter.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace AppArchiveFixture {

inline std::vector<char> Pattern(std::size_t size) {
    std::vector<char> data(size);
    for (std::size_t i = 0; i < size; ++i) data[i] = static_cast<char>((i * 7 + i / 4096) & 0xff);
    return data;
}

inline std::filesystem::path EnterFreshRoot(const char* name) {
    std::filesystem::path root;
    do {
        root = std::filesystem::temp_directory_path() /
            (std::string(name) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    } while (!std::filesystem::create_directory(root));
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    std::filesystem::current_path(root);
    return std::filesystem::canonical(root);
}

inline void WriteArchive(const std::filesystem::path& path, const std::map<std::string, std::vector<char>>& files, const std::vector<std::string>& directories = {}) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) std::abort();
    ZArchiveWriter writer(
        [](std::int32_t, void*) {},
        [](const void* data, std::size_t length, void* context) {
            static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
        },
        &output);
    for (const auto& directory : directories) if (!writer.MakeDir(directory.c_str(), true)) std::abort();
    for (const auto& [name, data] : files) {
        const auto slash = name.find_last_of('/');
        if (slash != std::string::npos) writer.MakeDir(name.substr(0, slash).c_str(), true);
        if (!writer.StartNewFile(name.c_str())) std::abort();
        if (!data.empty()) writer.AppendData(data.data(), data.size());
    }
    writer.Finalize();
    output.close();
    if (!output) std::abort();
}

}

#endif
