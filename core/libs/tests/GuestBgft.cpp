#include "prx/libSceBgft/libSceBgft.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

class RawParams {
public:
    void SetPointer(std::size_t offset, const char* text) {
        std::memcpy(bytes + offset, &text, sizeof(text));
    }
    const BgftStorageTaskParam* AsParams() const {
        return reinterpret_cast<const BgftStorageTaskParam*>(bytes);
    }
private:
    std::byte bytes[sizeof(BgftStorageTaskParam)]{};
};

static void Fill(char* text, std::size_t length) {
    std::memset(text, 'A', length);
    text[length] = 0;
}

int main() {
    constexpr int alreadyInitialized = static_cast<int>(0x80990001);
    constexpr int notInitialized = static_cast<int>(0x80990002);
    constexpr int invalidArgument = static_cast<int>(0x80990004);
    constexpr int notSupported = static_cast<int>(0x80990006);

    char idTooLong[0x32];
    char idAllowed[0x31];
    char text16TooLong[0x801];
    char text16Allowed[0x800];
    char text20TooLong[0x25a];
    char text20Allowed[0x259];
    char text28TooLong[0x801];
    char text28Allowed[0x800];
    Fill(idTooLong, 0x31);
    Fill(idAllowed, 0x30);
    Fill(text16TooLong, 0x800);
    Fill(text16Allowed, 0x7ff);
    Fill(text20TooLong, 0x259);
    Fill(text20Allowed, 0x258);
    Fill(text28TooLong, 0x800);
    Fill(text28Allowed, 0x7ff);

    RawParams valid;
    valid.SetPointer(0x8, "UP0001-CUSA00000_00-AAAAAAAAAAAAAAAA");
    std::int32_t taskId = 0x5a5a5a5a;
    std::byte progressBytes[88];
    std::memset(progressBytes, 0x5a, sizeof(progressBytes));
    std::byte untouched[88];
    std::memset(untouched, 0x5a, sizeof(untouched));
    BgftProgress* const progress = reinterpret_cast<BgftProgress*>(progressBytes);

    Require(sceBgftServiceDownloadStartTask(0) == notInitialized);
    Require(sceBgftServiceDownloadStartTaskAll() == notInitialized);
    Require(sceBgftServiceDownloadStopTask(0) == notInitialized);
    Require(sceBgftServiceDownloadStopTaskAll() == notInitialized);
    Require(sceBgftServiceDownloadGetProgress(0, progress) == notInitialized);
    Require(sceBgftServiceDownloadFindTaskByContentId(nullptr, 0, &taskId) == notInitialized);
    Require(sceBgftServiceDownloadFindActivePupTask(&taskId) == notInitialized);
    Require(sceBgftServiceDownloadFindActivePatchTask(nullptr, &taskId) == notInitialized);
    Require(sceBgftServiceDownloadPauseTask(0) == notInitialized);
    Require(sceBgftServiceDownloadResumeTask(0) == notInitialized);
    Require(sceBgftServiceDownloadPauseTaskAll() == notInitialized);
    Require(sceBgftServiceDownloadResumeTaskAll() == notInitialized);
    Require(sceBgftServiceDownloadQueryTask() == notInitialized);
    Require(sceBgftServiceIntGetTaskInfoValueInt() == notInitialized);
    Require(sceBgftServiceIntGetTaskInfoValueString() == notInitialized);
    Require(sceBgftServiceIntGetTaskInfoValueStringIndex() == notInitialized);
    Require(libSceBgftUnknown_fJnnIc5sHOU_nid_no_patch() == notInitialized);
    Require(sceBgftServiceIntDownloadUnregisterTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadUnregisterCompleteTask() == notInitialized);
    Require(sceBgftServiceIntDownloadUnregisterTaskAll() == notInitialized);
    Require(sceBgftServiceIntDownloadClearAllInstalled() == notInitialized);
    Require(sceBgftServiceIntDownloadClearInstalled() == notInitialized);
    Require(sceBgftServiceIntDownloadStartTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadRestartTask() == notInitialized);
    Require(sceBgftServiceIntDownloadStartTaskAll() == notInitialized);
    Require(sceBgftServiceIntDownloadStopTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadStopTaskAll() == notInitialized);
    Require(sceBgftServiceIntDownloadPauseTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadPauseTaskAll() == notInitialized);
    Require(sceBgftServiceIntDownloadResumeTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadResumeTaskAll() == notInitialized);
    Require(sceBgftServiceIntDownloadGetProgress(0, progress) == notInitialized);
    Require(sceBgftServiceIntDownloadStartGroupTasks() == notInitialized);
    Require(sceBgftServiceIntDownloadPauseGroupTasks() == notInitialized);
    Require(sceBgftServiceIntDownloadResumeGroupTasks() == notInitialized);
    Require(libSceBgftUnknown_85axuUkg9eE_nid_no_patch() == notInitialized);
    Require(sceBgftServiceIntDownloadGetGameAndGameAcProgress(nullptr, progress) == notInitialized);
    Require(sceBgftServiceIntDownloadGetPlayGoProgress(nullptr, progress) == notInitialized);
    Require(sceBgftServiceIntDownloadQueryTask() == notInitialized);
    Require(sceBgftServiceIntDownloadGetUserStorageSize() == notInitialized);
    Require(sceBgftServiceIntDownloadGetExtUsbUserStorageSize() == notInitialized);
    Require(libSceBgftUnknown_tBOVSvi9EjY_nid_no_patch() == notInitialized);
    Require(sceBgftServiceIntDownloadFindActiveTask(nullptr, 0, &taskId) == notInitialized);
    Require(sceBgftServiceIntDownloadFindActiveGameAndGameAcTask(nullptr, &taskId) == notInitialized);
    Require(sceBgftServiceIntPlayGoFindTaskId() == notInitialized);
    Require(sceBgftServiceIntPlayGoResume() == notInitialized);
    Require(sceBgftServiceIntPlayGoIsPaused() == notInitialized);
    Require(sceBgftServiceIntPlayGoGetDiscIniChunksInstallProgress() == notInitialized);
    Require(sceBgftServiceIntExtUsbCheckActiveDownloadTasks() == notInitialized);
    Require(sceBgftServiceIntUploadRegisterTask() == notInitialized);
    Require(sceBgftServiceIntUploadUnregisterTask() == notInitialized);
    Require(sceBgftServiceIntUploadUnregisterCompleteTask() == notInitialized);
    Require(sceBgftServiceIntUploadUnregisterTaskAll() == notInitialized);
    Require(sceBgftServiceIntUploadStartTask() == notInitialized);
    Require(sceBgftServiceIntUploadRestartTask() == notInitialized);
    Require(sceBgftServiceIntUploadStartTaskAll() == notInitialized);
    Require(sceBgftServiceIntUploadStopTask() == notInitialized);
    Require(sceBgftServiceIntUploadStopTaskAll() == notInitialized);
    Require(sceBgftServiceIntUploadGetProgress() == notInitialized);
    Require(sceBgftServiceIntUploadQueryTask() == notInitialized);
    Require(sceBgftServiceIntDebugDownloadRequest() == notInitialized);
    Require(sceBgftServiceIntDebugDownloadCorruptPkg() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoRevertToInitialState() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoRevertToSnapshot() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoSuspend() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoResume() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoIsPaused() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoSnapshotByTitleId() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoRevertToFullState() == notInitialized);
    Require(sceBgftServiceIntDebugPlayGoGetPlayGoStatusString() == notInitialized);
    Require(libSceBgftUnknown_3cvL6QTobNg_nid_no_patch() == notInitialized);
    Require(libSceBgftUnknown_4JQKNtsVDpg_nid_no_patch() == notInitialized);
    Require(sceBgftServiceIntDownloadDebugGetBgftEnvInfoString() == notInitialized);
    Require(sceBgftServiceIntDownloadDebugDownloadBgftEnvFile() == notInitialized);
    Require(sceBgftServiceIntDownloadDebugDeleteBgftEnvFile() == notInitialized);
    Require(sceBgftServiceIntDownloadDebugGetStat() == notInitialized);
    Require(sceBgftServiceTerm() == notInitialized);
    Require(sceBgftServiceIntTerm() == notInitialized);
    Require(taskId == 0x5a5a5a5a);
    Require(std::memcmp(progressBytes, untouched, sizeof(untouched)) == 0);


    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), &taskId) == notInitialized);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(valid.AsParams(), &taskId) == notInitialized);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(nullptr, &taskId) == invalidArgument);
    Require(sceBgftServiceIntInit(nullptr) == invalidArgument);
    errno = 71;
    Require(sceBgftServiceIntInit(reinterpret_cast<const BgftInitParam*>(0x10)) == invalidArgument);
    Require(errno == 71);

    std::byte heap[1024]{};
    const BgftInitParam initialized{heap, sizeof(heap)};
    Require(sceBgftServiceIntInit(&initialized) == 0);
    Require(sceBgftServiceIntInit(&initialized) == alreadyInitialized);
    Require(sceBgftServiceIntInit(nullptr) == alreadyInitialized);
    Require(sceBgftServiceIntInit(&initialized) == alreadyInitialized);

    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(nullptr, nullptr) == invalidArgument);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), nullptr) == invalidArgument);

    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(reinterpret_cast<const BgftStorageTaskParam*>(0x10), &taskId) == invalidArgument);
    Require(errno == 71);
    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(reinterpret_cast<const BgftStorageTaskParam*>(0x10), &taskId) == invalidArgument);
    Require(errno == 71);

    RawParams badId = valid;
    badId.SetPointer(0x8, reinterpret_cast<const char*>(0x10));
    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(badId.AsParams(), &taskId) == invalidArgument);
    Require(errno == 71);
    RawParams badText16 = valid;
    badText16.SetPointer(0x10, reinterpret_cast<const char*>(0x10));
    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(badText16.AsParams(), &taskId) == invalidArgument);
    Require(errno == 71);
    RawParams badText32 = valid;
    badText32.SetPointer(0x20, reinterpret_cast<const char*>(0x10));
    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(badText32.AsParams(), &taskId) == invalidArgument);
    Require(errno == 71);
    RawParams badText40 = valid;
    badText40.SetPointer(0x28, reinterpret_cast<const char*>(0x10));
    errno = 71;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(badText40.AsParams(), &taskId) == invalidArgument);
    Require(errno == 71);

    RawParams missingId;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(missingId.AsParams(), &taskId) == invalidArgument);
    RawParams idOverLong = valid;
    idOverLong.SetPointer(0x8, idTooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(idOverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text16OverLong = valid;
    text16OverLong.SetPointer(0x10, text16TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text16OverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text20OverLong = valid;
    text20OverLong.SetPointer(0x20, text20TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text20OverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text28OverLong = valid;
    text28OverLong.SetPointer(0x28, text28TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text28OverLong.AsParams(), &taskId) == invalidArgument);

    RawParams boundary;
    boundary.SetPointer(0x8, idAllowed);
    boundary.SetPointer(0x10, text16Allowed);
    boundary.SetPointer(0x20, text20Allowed);
    boundary.SetPointer(0x28, text28Allowed);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(boundary.AsParams(), &taskId) == notSupported);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), &taskId) == notSupported);
    Require(taskId == 0x5a5a5a5a);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(boundary.AsParams(), &taskId) == notSupported);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(nullptr, &taskId) == invalidArgument);
    Require(taskId == 0x5a5a5a5a);

    Require(sceBgftServiceIntTerm() == 0);
    Require(sceBgftServiceIntTerm() == notInitialized);
    Require(sceBgftServiceTerm() == notInitialized);
    Require(sceBgftServiceIntDownloadStartTask(0) == notInitialized);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), &taskId) == notInitialized);

    Require(sceBgftServiceInit(nullptr) == invalidArgument);
    Require(sceBgftServiceInit(&initialized) == 0);
    Require(sceBgftServiceInit(&initialized) == alreadyInitialized);
    Require(sceBgftServiceIntInit(&initialized) == alreadyInitialized);
    Require(sceBgftServiceTerm() == 0);
    Require(sceBgftServiceIntInit(&initialized) == 0);
    Require(sceBgftServiceInit(nullptr) == alreadyInitialized);
    Require(sceBgftServiceTerm() == 0);
    Require(sceBgftServiceIntTerm() == notInitialized);
    Require(taskId == 0x5a5a5a5a);
    return 0;
}
