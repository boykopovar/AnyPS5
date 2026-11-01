#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

int CheckNoIcon(const char* root) {
    std::filesystem::current_path(root);
    Require(!HasAppIcon_nid_postfix());
    bool threw = false;
    try {
        GetAppIconData_nid_postfix();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("icon not available") != std::string_view::npos;
    }
    Require(threw);
    return 0;
}

int CheckValidIcon(const char* root) {
    std::filesystem::current_path(root);
    Require(HasAppIcon_nid_postfix());
    const auto icon = GetAppIconData_nid_postfix();
    Require(icon.bytes != nullptr);
    Require(icon.size == 16);
    Require(icon.bytes[0] == 0x89 && icon.bytes[1] == 'P' && icon.bytes[2] == 'N' && icon.bytes[3] == 'G');
    return 0;
}

int CheckEmptyIcon(const char* root) {
    std::filesystem::current_path(root);
    bool threw = false;
    try {
        HasAppIcon_nid_postfix();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("icon0.png is empty or unreadable") != std::string_view::npos;
    }
    Require(threw);
    return 0;
}

int CheckCorruptPng(const char* root) {
    std::filesystem::current_path(root);
    bool threw = false;
    try {
        HasAppIcon_nid_postfix();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("icon0.png is not a valid PNG file") != std::string_view::npos;
    }
    Require(threw);
    return 0;
}

int CheckConcurrentAccess(const char* root) {
    std::filesystem::current_path(root);
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([] {
            Require(HasAppIcon_nid_postfix());
            const auto icon = GetAppIconData_nid_postfix();
            Require(icon.size == 16);
        });
    }
    for (auto& t : threads) t.join();
    return 0;
}

void RunCase(const char* self, const char* mode, std::initializer_list<std::pair<const char*, std::string>> files) {
    const auto root = std::filesystem::temp_directory_path() / ("guest_app_icon_" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    for (const auto& [name, content] : files) {
        std::ofstream stream(root / "app0" / "sce_sys" / name, std::ios::binary);
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    const std::string arguments = "\"" + std::string(self) + "\" " + mode + " \"" + root.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    const int status = std::system(command.c_str());
    std::filesystem::remove_all(root);
    Require(status == 0);
}

}

int main(int argc, char** argv) {
    if (argc == 3) {
        const std::string_view mode = argv[1];
        if (mode == "--no-icon") return CheckNoIcon(argv[2]);
        if (mode == "--valid-icon") return CheckValidIcon(argv[2]);
        if (mode == "--empty-icon") return CheckEmptyIcon(argv[2]);
        if (mode == "--corrupt-png") return CheckCorruptPng(argv[2]);
        if (mode == "--concurrent") return CheckConcurrentAccess(argv[2]);
        return 1;
    }

    const std::string validPng("\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00", 16);
    const std::string corruptPng("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 16);
    const std::string emptyPng("");

    RunCase(argv[0], "--no-icon", {});
    RunCase(argv[0], "--valid-icon", {{"icon0.png", validPng}});
    RunCase(argv[0], "--empty-icon", {{"icon0.png", emptyPng}});
    RunCase(argv[0], "--corrupt-png", {{"icon0.png", corruptPng}});
    RunCase(argv[0], "--concurrent", {{"icon0.png", validPng}});

    return 0;
}
