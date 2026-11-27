#include "SceTypes.hpp"
#include "prx/libSceSaveData/SaveDataFile.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace {

void Require(bool value) { if (!value) std::abort(); }

struct SandboxCleanup {
    std::filesystem::path root;
    ~SandboxCleanup() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

std::string ReadFileText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.is_open());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::size_t CountDirectoryEntries(const std::filesystem::path& dir) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++count;
    }
    return count;
}

void TestSaveDataFile() {
    std::random_device random;
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto sandbox = std::filesystem::temp_directory_path() /
                         ("anyps5_savedata_file_test_" + std::to_string(ticks) + "_" + std::to_string(random()));
    SandboxCleanup cleanup{sandbox};
    Require(std::filesystem::create_directories(sandbox));

    const char sample[] = "payload";
    int callback_calls = 0;
    auto move_callback = [&](const std::filesystem::path& temporary, const std::filesystem::path& destination) {
        ++callback_calls;
        return savedata::move_replacing(temporary, destination);
    };

    Require(!savedata::replace_file_with(std::filesystem::path(), sample, sizeof(sample) - 1, move_callback));
    Require(callback_calls == 0);

    Require(!savedata::replace_file_with(sandbox / "stray_trailing" / "", sample, sizeof(sample) - 1, move_callback));
    Require(callback_calls == 0);
    Require(!std::filesystem::exists(sandbox / "stray_trailing"));

    Require(!savedata::replace_file_with(sandbox / "stray_dot" / ".", sample, sizeof(sample) - 1, move_callback));
    Require(callback_calls == 0);
    Require(!std::filesystem::exists(sandbox / "stray_dot"));

    Require(!savedata::replace_file_with(sandbox / "stray_dotdot" / "..", sample, sizeof(sample) - 1, move_callback));
    Require(callback_calls == 0);
    Require(!std::filesystem::exists(sandbox / "stray_dotdot"));

    const auto existing_dir = sandbox / "existing_dir";
    Require(std::filesystem::create_directory(existing_dir));
    Require(!savedata::replace_file_with(existing_dir, sample, sizeof(sample) - 1, move_callback));
    Require(callback_calls == 0);
    Require(std::filesystem::is_directory(existing_dir));

    const auto nested_dir = sandbox / "nested" / "sub";
    const auto target_file = nested_dir / "save.bin";
    const std::string initial_bytes = "initial-save-data";
    Require(savedata::replace_file(target_file, initial_bytes.data(), initial_bytes.size()));
    Require(std::filesystem::is_regular_file(target_file));
    Require(ReadFileText(target_file) == initial_bytes);
    Require(CountDirectoryEntries(nested_dir) == 1);

    const std::string updated_bytes = "updated-save-payload-v2";
    Require(savedata::replace_file(target_file, updated_bytes.data(), updated_bytes.size()));
    Require(ReadFileText(target_file) == updated_bytes);
    Require(CountDirectoryEntries(nested_dir) == 1);

    int failing_callback_calls = 0;
    const std::string aborted_bytes = "should-not-overwrite";
    Require(!savedata::replace_file_with(
        target_file, aborted_bytes.data(), aborted_bytes.size(),
        [&](const std::filesystem::path& temporary, const std::filesystem::path& destination) {
            ++failing_callback_calls;
            Require(std::filesystem::is_regular_file(temporary));
            Require(destination == target_file);
            return false;
        }));
    Require(failing_callback_calls == 1);
    Require(ReadFileText(target_file) == updated_bytes);
    Require(CountDirectoryEntries(nested_dir) == 1);

    Require(!savedata::move_replacing(std::filesystem::path(), target_file));
    Require(!savedata::move_replacing(target_file, std::filesystem::path()));
    Require(std::filesystem::is_regular_file(target_file));
    Require(ReadFileText(target_file) == updated_bytes);
}

}

int main() {
    for (int call = 0; call < 3; ++call) {
        Require(sceSaveDataGetSavePoint() == 2);
        Require(sceSaveDataGetClientThreadPriority() == 0);
        Require(sceSaveDataDebugGetThreadId() == 0);
    }
    TestSaveDataFile();
    return 0;
}
