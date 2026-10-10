#include "prx/libSceSaveData/SaveDataFile.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data write failure check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-savedata-write-failure-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));

    const auto savePath = root / "memory.dat";
    const std::vector<char> oldData{'o', 'l', 'd'};
    const std::vector<char> replacement{'n', 'e', 'w'};
    {
        std::ofstream file(savePath, std::ios::binary);
        Require(file.is_open());
        file.write(oldData.data(), static_cast<std::streamsize>(oldData.size()));
        Require(static_cast<bool>(file));
    }

    int replaceAttempts = 0;
    const bool replaced = savedata::replace_file_with(
        savePath, replacement.data(), replacement.size(),
        [&replaceAttempts, &savePath](const std::filesystem::path& temporary,
                                      const std::filesystem::path& destination) {
            ++replaceAttempts;
            Require(destination == savePath);
            auto expectedTemporary = savePath;
            expectedTemporary += ".tmp";
            Require(temporary == expectedTemporary);
            return false;
        });

    Require(!replaced);
    Require(replaceAttempts == 1);
    std::vector<char> savedData;
    {
        std::ifstream savedFile(savePath, std::ios::binary);
        savedData.assign(std::istreambuf_iterator<char>(savedFile), std::istreambuf_iterator<char>());
    }
    Require(savedData == oldData);
    auto temporary = savePath;
    temporary += ".tmp";
    Require(!std::filesystem::exists(temporary));

    std::filesystem::remove_all(root);
}
