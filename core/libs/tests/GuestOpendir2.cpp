#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

static std::map<std::string, int> Entries(void* directory) {
    std::map<std::string, int> entries;
    while (auto* entry = readdir_nid_postfix(directory)) entries[entry->name] = entry->type;
    Require(closedir_nid_postfix(directory) == 0);
    return entries;
}

int main() {
    const auto root = std::filesystem::path("anyps5-opendir2-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "subdirectory"));
    { std::ofstream file(root / "sample.txt"); file << "test"; }
    const auto path = root.string();
    void* plain = opendir_nid_postfix(path.c_str());
    Require(plain != nullptr);
    const auto expected = Entries(plain);
    Require(expected.at("sample.txt") == 8 && expected.at("subdirectory") == 4);
    for (const int flags : {0x7, 0x6}) {
        void* directory = __opendir2_nid_postfix(path.c_str(), flags);
        Require(directory != nullptr);
        Require(Entries(directory) == expected);
    }
    const auto missing = (root / "missing").string();
    Require(opendir_nid_postfix(missing.c_str()) == nullptr);
    const int missingError = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(__opendir2_nid_postfix(missing.c_str(), 0x7) == nullptr && *__error_nid_postfix() == missingError);
    std::filesystem::remove(root / "sample.txt");
    std::filesystem::remove(root / "subdirectory");
    std::filesystem::remove(root);
}
