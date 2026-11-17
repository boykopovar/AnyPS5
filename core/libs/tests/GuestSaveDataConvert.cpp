#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cerrno>
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
    Expect(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "progress query before initialize fails with not initialized");
    Expect(progress == 0.25f, "an uninitialized progress query leaves the value untouched");
    Expect(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes for the progress checks");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds");
    Expect(progress == 0.0f, "progress reports 0.0f before any backup");
    progress = 0.25f;
    Expect(sceSaveDataGetProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    errno = 71;
    Expect(sceSaveDataGetProgress(reinterpret_cast<float*>(0x10)) == SaveDataErrorParameter && errno == 71, "unwritable progress pointer is rejected");
    Expect(progress == 0.25f, "a rejected progress query leaves the value untouched");
    Expect(sceSaveDataBackup(nullptr) == SaveDataErrorParameter, "null backup pointer is rejected");
    Expect(progress == 0.25f, "a rejected backup leaves progress untouched");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a rejected backup");
    Expect(progress == 0.0f, "progress reports 0.0f after a rejected backup");
    Expect(Backup("progress") == 0, "a backup succeeds");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a backup");
    Expect(progress == 1.0f, "progress reports 1.0f after a backup");
    Expect(sceSaveDataTerminate() == 0, "SaveData terminates after a backup");
    Expect(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "progress query after terminate fails with not initialized");
    Expect(sceSaveDataInitialize3(nullptr) == 0, "SaveData reinitializes after the progress checks");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after reinitialize");
    Expect(progress == 0.0f, "progress does not survive a terminate");
    Expect(Backup("progress") == 0, "a backup succeeds after reinitialize");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after the second backup");
    Expect(progress == 1.0f, "progress reports 1.0f after the second backup");
    Expect(sceSaveDataClearProgress() == 0, "clear progress succeeds");
    Expect(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after clear progress");
    Expect(progress == 0.0f, "clear progress resets the value");
    SaveDataEvent event{};
    Expect(sceSaveDataGetEventResult(nullptr, &event) == 0, "the first backup queues a completion");
    Expect(event.type == 2 && event.error_code == 0, "the completion reports a successful backup");
    Expect(sceSaveDataGetEventResult(nullptr, &event) == 0, "the second backup queues a completion");
    Expect(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "the rejected backup queues no completion");
    Expect(sceSaveDataTerminate() == 0, "SaveData terminates after the progress checks");
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
    Expect(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query before initialize fails with not initialized");
    Expect(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Expect(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds");
    Expect(progress == 0.0f, "convert progress reports 0.0f initially");
    Expect(sceSaveDataGetConvertProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    errno = 71;
    Expect(sceSaveDataGetConvertProgress(reinterpret_cast<float*>(0x10)) == SaveDataErrorParameter && errno == 71, "unwritable progress pointer is rejected");
    Expect(sceSaveDataConvert(nullptr) == SaveDataErrorParameter, "null convert pointer is rejected");
    Expect(sceSaveDataConvert(reinterpret_cast<const SaveDataConvert*>(0x10)) == SaveDataErrorParameter, "unmapped convert pointer is rejected");
    SaveDataConvert nullDir{};
    Expect(sceSaveDataConvert(&nullDir) == SaveDataErrorParameter, "null dir_name pointer is rejected");
    SaveDataConvert unmappedDir{};
    unmappedDir.dir_name = reinterpret_cast<const SceSaveDataDirName*>(0x10);
    Expect(sceSaveDataConvert(&unmappedDir) == SaveDataErrorParameter, "unmapped dir_name pointer is rejected");
    Expect(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Expect(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after failed convert");
    Expect(progress < 1.0f, "convert progress is below 1.0f after failed convert");
    Expect(progress == 0.0f, "convert progress reports 0.0f after failed convert");
    Expect(Convert("kept", sizeof("kept")) == 0, "an existing save converts");
    SaveDataEvent event{};
    Expect(sceSaveDataGetEventResult(nullptr, &event) == 0, "successful conversion queues completion");
    Expect(event.type == 5 && event.error_code == 0, "conversion event reports successful conversion");
    Expect(event.user_id == 0x10000000 && std::strcmp(event.dir_name.data, "kept") == 0, "conversion event identifies the user and save");
    Expect(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "conversion event is delivered once");
    Expect(Read(kept) == std::vector<char>{'d', 'a', 't', 'a'}, "the converted save keeps its files");
    Expect(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after successful convert");
    Expect(progress == 1.0f, "convert progress reports 1.0f after successful convert");
    Expect(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Expect(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after subsequent failed convert");
    Expect(progress < 1.0f, "convert progress drops below 1.0f after subsequent failed convert");
    Expect(progress == 0.0f, "convert progress reports 0.0f after subsequent failed convert");
    for (const char* invalid : {"../kept", "", ".", "..", "a/b", "a\\b", "c:d"}) {
        Expect(Convert(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Expect(Convert(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");
    Expect(sceSaveDataTerminate() == 0, "SaveData terminates");
    Expect(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query after terminate fails with not initialized");
    CheckBackupProgress();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (Failures() != 0) return 1;
    std::printf("savedata convert tests passed\n");
    return 0;
}
