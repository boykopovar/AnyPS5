#include "prx/libSceSaveData/SaveDataFile.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data write failure check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<char> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void WriteFile(const std::filesystem::path& path, const std::vector<char>& data) {
    std::ofstream file(path, std::ios::binary);
    Require(file.is_open());
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    Require(static_cast<bool>(file));
}

static bool ReplaceFile(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    return !error;
#endif
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-savedata-write-failure-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));

    const auto savePath = root / std::filesystem::path(u8"m\u00e9moire-\u65e5\u672c.dat");
    const std::vector<char> oldData{'o', 'l', 'd'};
    const std::vector<char> replacement{'n', 'e', 'w'};
    WriteFile(savePath, oldData);

    int replaceAttempts = 0;
    std::filesystem::path failedTemporary;
    const bool replaced = savedata::replace_file_with(
        savePath, replacement.data(), replacement.size(),
        [&](const std::filesystem::path& temporary, const std::filesystem::path& destination) {
            ++replaceAttempts;
            Require(destination == savePath);
            Require(temporary != destination && temporary.parent_path() == destination.parent_path());
            Require(ReadFile(temporary) == replacement);
            failedTemporary = temporary;
            return false;
        });

    Require(!replaced);
    Require(replaceAttempts == 1);
    Require(ReadFile(savePath) == oldData);
    Require(!std::filesystem::exists(failedTemporary));

    auto temporary = savePath;
    temporary += ".tmp";
    WriteFile(temporary, oldData);
    Require(savedata::replace_file(savePath, replacement.data(), replacement.size()));
    Require(ReadFile(savePath) == replacement);
    Require(std::filesystem::exists(temporary));
    Require(ReadFile(temporary) == oldData);
    std::filesystem::remove(temporary);

    const auto unrelated = root / "unrelated.dat";
    WriteFile(unrelated, oldData);
    std::filesystem::create_hard_link(unrelated, temporary);
    Require(savedata::replace_file(savePath, replacement.data(), replacement.size()));
    Require(ReadFile(unrelated) == oldData);
    Require(std::filesystem::equivalent(unrelated, temporary));
    Require(!std::filesystem::equivalent(unrelated, savePath));
    std::filesystem::remove(temporary);

    std::error_code symlinkError;
#ifdef _WIN32
    if (!CreateSymbolicLinkW(temporary.c_str(), unrelated.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        symlinkError = std::error_code(GetLastError(), std::system_category());
    }
#else
    std::filesystem::create_symlink(unrelated, temporary, symlinkError);
#endif
#ifdef _WIN32
    if (symlinkError == std::errc::permission_denied ||
        symlinkError == std::error_code(ERROR_PRIVILEGE_NOT_HELD, std::system_category())) {
        std::printf("Symlink case skipped: creating Windows symlinks requires developer mode or privilege\n");
    } else
#endif
    {
        Require(!symlinkError);
        Require(savedata::replace_file(savePath, replacement.data(), replacement.size()));
        Require(ReadFile(unrelated) == oldData);
#ifdef _WIN32
        const auto linkAttributes = GetFileAttributesW(temporary.c_str());
        const auto saveAttributes = GetFileAttributesW(savePath.c_str());
        Require(linkAttributes != INVALID_FILE_ATTRIBUTES && (linkAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        Require(saveAttributes != INVALID_FILE_ATTRIBUTES && (saveAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
#else
        Require(std::filesystem::is_symlink(temporary));
        Require(!std::filesystem::is_symlink(savePath));
#endif
        Require(ReadFile(savePath) == replacement);
        std::filesystem::remove(temporary);
    }

    Require(std::filesystem::create_directory(temporary));
    Require(savedata::replace_file(savePath, replacement.data(), replacement.size()));
    Require(std::filesystem::is_directory(temporary));
    std::filesystem::remove(temporary);

    std::filesystem::path outerTemporary;
    Require(savedata::replace_file_with(savePath, replacement.data(), replacement.size(),
        [&](const auto& outer, const auto& destination) {
            outerTemporary = outer;
            Require(savedata::replace_file_with(savePath, oldData.data(), oldData.size(),
                [&](const auto& inner, const auto& innerDestination) {
                    Require(inner != outer);
                    Require(ReadFile(outer) == replacement);
                    Require(ReadFile(inner) == oldData);
                    return ReplaceFile(inner, innerDestination);
                }));
            Require(ReadFile(destination) == oldData);
            Require(ReadFile(outer) == replacement);
            return ReplaceFile(outer, destination);
        }));
    Require(ReadFile(savePath) == replacement);
    Require(!std::filesystem::exists(outerTemporary));

    Require(savedata::replace_file_with(savePath, replacement.data(), replacement.size(),
        [&](const auto& created, const auto& destination) {
            Require(ReplaceFile(created, destination));
            outerTemporary = created;
            WriteFile(created, oldData);
            return true;
        }));
    Require(ReadFile(outerTemporary) == oldData);
    std::filesystem::remove(outerTemporary);

    std::filesystem::path thrownTemporary;
    bool threw = false;
    try {
        savedata::replace_file_with(savePath, oldData.data(), oldData.size(),
            [&](const auto& created, const auto&) -> bool {
                thrownTemporary = created;
                throw std::runtime_error("injected replacement failure");
            });
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && !thrownTemporary.empty());
    Require(!std::filesystem::exists(thrownTemporary));
    Require(ReadFile(savePath) == replacement);

    Require(!savedata::replace_file(savePath, nullptr, 1));
    Require(ReadFile(savePath) == replacement);
    Require(savedata::replace_file(savePath, nullptr, 0));
    Require(ReadFile(savePath).empty());
    Require(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == 2);

    std::filesystem::remove_all(root);
}
