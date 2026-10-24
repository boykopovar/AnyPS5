#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceSaveDataInitialize2(const void*);
int APS5_VABI sceSaveDataTerminate(void);
int APS5_VABI sceSaveDataGetProgress(float*);
int APS5_VABI sceSaveDataClearProgress(void);
int APS5_VABI sceSaveDataBackup(const SaveDataBackup*);
int APS5_VABI sceSaveDataGetEventResult(const void*, SaveDataEvent*);
}

namespace {

constexpr int ErrorParameter = -2137063424;
constexpr int ErrorNotInitialized = -2137063423;

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "save-data progress check failed at line %d\n", line);
    std::abort();
}
#define Check(condition) Require((condition), __LINE__)

}  // namespace

int main() {
    float progress = -1.0f;
    Check(sceSaveDataGetProgress(&progress) == ErrorNotInitialized);
    Check(sceSaveDataGetProgress(nullptr) == ErrorNotInitialized);
    Check(sceSaveDataClearProgress() == ErrorNotInitialized);

    Check(sceSaveDataInitialize2(nullptr) == 0);
    Check(sceSaveDataGetProgress(nullptr) == ErrorParameter);
    Check(sceSaveDataGetProgress(&progress) == 0);
    Check(progress == 0.0f);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "progress0", sizeof("progress0"));
    SaveDataBackup backup{};
    backup.dir_name = &dirName;
    Check(sceSaveDataBackup(&backup) == 0);
    Check(sceSaveDataGetProgress(&progress) == 0);
    Check(progress == 1.0f);

    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0);
    Check(progress == 1.0f);

    Check(sceSaveDataClearProgress() == 0);
    Check(sceSaveDataGetProgress(&progress) == 0);
    Check(progress == 0.0f);

    Check(sceSaveDataBackup(nullptr) == ErrorParameter);
    Check(sceSaveDataGetProgress(&progress) == 0);
    Check(progress == 0.0f);

    Check(sceSaveDataTerminate() == 0);
    Check(sceSaveDataGetProgress(&progress) == ErrorNotInitialized);
    Check(sceSaveDataClearProgress() == ErrorNotInitialized);
    return 0;
}
