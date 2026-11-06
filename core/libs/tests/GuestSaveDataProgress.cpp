#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata progress check failed: %s\n", what.c_str());
    ++failures;
}

int Backup(const char* data) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, std::strlen(data) + 1);
    SaveDataBackup backup{};
    backup.user_id = 0x10000000;
    backup.dir_name = &name;
    return sceSaveDataBackup(&backup);
}

}

int main() {
    float progress = 0.25f;
    Check(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "query before initialize fails with not initialized");
    Check(progress == 0.25f, "an uninitialized query leaves the value untouched");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds");
    Check(progress == 0.0f, "progress reports 0.0f before any backup");
    Check(sceSaveDataGetProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    Check(sceSaveDataGetProgress(reinterpret_cast<float*>(0x10)) == SaveDataErrorParameter, "unwritable progress pointer is rejected");
    Check(progress == 0.0f, "a rejected query leaves the value untouched");
    Check(sceSaveDataBackup(nullptr) == SaveDataErrorParameter, "null backup pointer is rejected");
    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'a', sizeof(unterminated.data));
    SaveDataBackup unterminatedBackup{};
    unterminatedBackup.user_id = 0x10000000;
    unterminatedBackup.dir_name = &unterminated;
    Check(sceSaveDataBackup(&unterminatedBackup) == SaveDataErrorParameter, "an unterminated name is rejected");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a rejected backup");
    Check(progress == 0.0f, "a rejected backup leaves progress at 0.0f");
    Check(Backup("progress") == 0, "a backup succeeds");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a backup");
    Check(progress == 1.0f, "progress reports 1.0f after a backup");
    Check(Backup("progress") == 0, "a second backup succeeds");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after a second backup");
    Check(progress == 1.0f, "progress stays at 1.0f after a second backup");
    Check(sceSaveDataClearProgress() == 0, "clear progress succeeds");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after clear progress");
    Check(progress == 0.0f, "clear progress resets the value");
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "the backup queues a completion");
    Check(event.type == 2 && event.error_code == 0, "the completion reports a successful backup");
    Check(event.user_id == 0x10000000 && std::strcmp(event.dir_name.data, "progress") == 0, "the completion identifies the user and save");
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "the second backup queues a completion");
    Check(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "a rejected backup queues no completion");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates");
    Check(sceSaveDataGetProgress(&progress) == SaveDataErrorNotInitialized, "query after terminate fails with not initialized");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData reinitializes");
    Check(sceSaveDataGetProgress(&progress) == 0, "get progress succeeds after reinitialize");
    Check(progress == 0.0f, "progress does not survive a terminate");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates again");
    if (failures != 0) return 1;
    std::printf("savedata progress tests passed\n");
    return 0;
}
