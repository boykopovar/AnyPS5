#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata convert check failed: %s\n", what.c_str());
    ++failures;
}

int Convert(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataConvert convert{};
    convert.user_id = 0x10000000;
    convert.dir_name = &name;
    return sceSaveDataConvert(&convert);
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int Backup(const char* data) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, std::strlen(data) + 1);
    SaveDataBackup backup{};
    backup.user_id = 0x10000000;
    backup.dir_name = &name;
    return sceSaveDataBackup(&backup);
}

void CheckBackupProgress() {
    float progress = 0.25f;
    Check(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "progress query before initialize fails with not initialized");
    Check(progress == 0.25f, "an uninitialized progress query leaves the value untouched");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes for the progress checks");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds");
    Check(progress == 0.0f, "progress reports 0.0f before any backup");
    progress = 0.25f;
    Check(sceSaveDataGetProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    Check(sceSaveDataGetProgress(reinterpret_cast<float*>(0x10)) == SaveDataErrorParameter, "unwritable progress pointer is rejected");
    Check(progress == 0.25f, "a rejected progress query leaves the value untouched");
    Check(sceSaveDataBackup(nullptr) == SaveDataErrorParameter, "null backup pointer is rejected");
    Check(progress == 0.25f, "a rejected backup leaves progress untouched");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a rejected backup");
    Check(progress == 0.0f, "progress reports 0.0f after a rejected backup");
    Check(Backup("progress") == 0, "a backup succeeds");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a backup");
    Check(progress == 1.0f, "progress reports 1.0f after a backup");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates after a backup");
    Check(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "progress query after terminate fails with not initialized");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData reinitializes after the progress checks");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after reinitialize");
    Check(progress == 0.0f, "progress does not survive a terminate");
    Check(Backup("progress") == 0, "a backup succeeds after reinitialize");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after the second backup");
    Check(progress == 1.0f, "progress reports 1.0f after the second backup");
    Check(sceSaveDataClearProgress() == 0, "clear progress succeeds");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after clear progress");
    Check(progress == 0.0f, "clear progress resets the value");
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "the first backup queues a completion");
    Check(event.type == 2 && event.error_code == 0, "the completion reports a successful backup");
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "the second backup queues a completion");
    Check(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "the rejected backup queues no completion");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates after the progress checks");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-convert-" + std::to_string(std::random_device{}()));
    const auto work = root / "work";
    const auto kept = work / "_sd" / "kept" / "data.bin";
    std::filesystem::create_directories(kept.parent_path());
    std::ofstream(kept) << "data";
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    float progress = 0.0f;
    Check(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query before initialize fails with not initialized");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds");
    Check(progress == 0.0f, "convert progress reports 0.0f initially");
    Check(sceSaveDataGetConvertProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    Check(sceSaveDataGetConvertProgress(reinterpret_cast<float*>(0x10)) == SaveDataErrorParameter, "unwritable progress pointer is rejected");
    Check(sceSaveDataConvert(nullptr) == SaveDataErrorParameter, "null convert pointer is rejected");
    Check(sceSaveDataConvert(reinterpret_cast<const SaveDataConvert*>(0x10)) == SaveDataErrorParameter, "unmapped convert pointer is rejected");
    SaveDataConvert nullDir{};
    Check(sceSaveDataConvert(&nullDir) == SaveDataErrorParameter, "null dir_name pointer is rejected");
    SaveDataConvert unmappedDir{};
    unmappedDir.dir_name = reinterpret_cast<const SceSaveDataDirName*>(0x10);
    Check(sceSaveDataConvert(&unmappedDir) == SaveDataErrorParameter, "unmapped dir_name pointer is rejected");
    Check(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after failed convert");
    Check(progress < 1.0f, "convert progress is below 1.0f after failed convert");
    Check(progress == 0.0f, "convert progress reports 0.0f after failed convert");
    Check(Convert("kept", sizeof("kept")) == 0, "an existing save converts");
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "successful conversion queues completion");
    Check(event.type == 5 && event.error_code == 0, "conversion event reports successful conversion");
    Check(event.user_id == 0x10000000 && std::strcmp(event.dir_name.data, "kept") == 0, "conversion event identifies the user and save");
    Check(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "conversion event is delivered once");
    Check(Read(kept) == std::vector<char>{'d', 'a', 't', 'a'}, "the converted save keeps its files");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after successful convert");
    Check(progress == 1.0f, "convert progress reports 1.0f after successful convert");
    Check(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after subsequent failed convert");
    Check(progress < 1.0f, "convert progress drops below 1.0f after subsequent failed convert");
    Check(progress == 0.0f, "convert progress reports 0.0f after subsequent failed convert");
    for (const char* invalid : {"../kept", "", ".", "..", "a/b", "a\\b", "c:d"}) {
        Check(Convert(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Check(Convert(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates");
    Check(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query after terminate fails with not initialized");
    CheckBackupProgress();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata convert tests passed\n");
    return 0;
}
