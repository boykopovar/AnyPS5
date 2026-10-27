#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

static void Require(bool value) { if (!value) std::abort(); }

static void WriteParamSfo(const std::filesystem::path& path) {
    struct Entry { std::uint16_t key, format; std::uint32_t length, maximum, offset; };
    const std::uint32_t header[5] = {0x46535000, 0x0101, 68, 104, 3};
    const Entry entries[3] = {{0, 0x0404, 4, 4, 0}, {19, 0x0204, 8, 8, 4}, {25, 0x0204, 10, 10, 12}};
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(entries), sizeof(entries));
    file << std::string("DOWNLOAD_DATA_SIZE\0TITLE\0TITLE_ID\0\0\0", 36) << std::string("\0\1\0\0Example\0CUSA00000\0", 22);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    Require(std::filesystem::is_directory("download0") == (mode == "declared" || mode == "sfo-child"));
    const bool data = std::filesystem::is_directory("data");
    std::filesystem::remove_all("data");
    Require(data == (mode != "missing" && mode != "sfo"));
    if (mode == "missing") return 0;
    if (mode == "sfo") {
        std::filesystem::create_directories("app0/sce_sys");
        WriteParamSfo("app0/sce_sys/param.sfo");
#ifdef _WIN32
        const std::string command = "\"\"" + std::string(argv[0]) + "\" sfo-child\"";
#else
        const std::string command = "\"" + std::string(argv[0]) + "\" sfo-child";
#endif
        const int status = std::system(command.c_str());
        std::filesystem::remove_all("app0");
        Require(status == 0);
        return 0;
    }
    if (mode == "invalid") {
        bool rejected = false;
        try {
            GetAppTitleId_nid_postfix();
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected);
        return 0;
    }
    Require(std::strcmp(GetAppTitleId_nid_postfix().value, mode == "sfo-child" ? "CUSA00000" : "PPSA00000") == 0);
    std::filesystem::remove_all("download0");
}
