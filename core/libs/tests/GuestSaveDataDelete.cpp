#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

extern "C" int APS5_VABI sceSaveDataDelete(const SaveDataDelete*);

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata delete check failed: %s\n", what.c_str());
    ++failures;
}

int Delete(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataDelete del{};
    del.dir_name = &name;
    return sceSaveDataDelete(&del);
}

void Write(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "data";
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-delete-" + std::to_string(std::random_device{}()));
    const auto work = root / "work";
    const auto kept = work / "_sd" / "kept" / "data.bin";
    const auto victim = work / "victim" / "important.txt";
    Write(kept);
    Write(victim);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    for (const char* invalid : {"../victim", "", ".", "..", "../..", "kept/..", "a\\b", "c:d"}) {
        Check(Delete(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
        Check(std::filesystem::exists(kept) && std::filesystem::exists(victim), std::string("\"") + invalid + "\" deletes nothing");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Check(Delete(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");

    Check(Delete("kept", 5) == 0, "a valid name is deleted");
    Check(!std::filesystem::exists(kept.parent_path()), "the valid save directory is gone");
    Check(std::filesystem::exists(victim), "the directory beside the save root is untouched");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata delete tests passed\n");
    return 0;
}
