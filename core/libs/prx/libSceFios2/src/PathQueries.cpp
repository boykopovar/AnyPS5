#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

enum class Kind { Any, File, Directory };

SceFiosOp QueryExists(const SceFiosOpAttr* attr, const char* path, bool* outExists, Kind kind) {
    Fios::OpInfo info;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return Fios::SubmitFailed(attr, std::move(info), error);
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [target, outExists, kind](Fios::Completion& completion) {
        SceFiosStat stat{};
        const std::int32_t error = Fios::StatPath(target, stat);
        bool exists = false;
        if (error == SCE_FIOS_OK) {
            const bool directory = (stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0;
            exists = kind == Kind::Any || (kind == Kind::Directory) == directory;
        } else if (error != SCE_FIOS_ERROR_BAD_PATH) {
            completion.error = error;
            return;
        }
        completion.result = exists ? 1 : 0;
        if (outExists != nullptr && !Fios::CopyOut(outExists, &exists, sizeof(exists))) completion.error = SCE_FIOS_ERROR_BAD_PTR;
    });
}

bool ExistsSync(SceFiosOp op) {
    return op >= 0 && Fios::SyncCount(op) == 1;
}

}

extern "C" {

SceFiosOp APS5_VABI sceFiosExists(const SceFiosOpAttr* attr, const char* path, bool* outExists) {
    return QueryExists(attr, path, outExists, Kind::Any);
}

bool APS5_VABI sceFiosExistsSync(const SceFiosOpAttr* attr, const char* path) {
    return ExistsSync(QueryExists(attr, path, nullptr, Kind::Any));
}

SceFiosOp APS5_VABI sceFiosFileExists(const SceFiosOpAttr* attr, const char* path, bool* outExists) {
    return QueryExists(attr, path, outExists, Kind::File);
}

bool APS5_VABI sceFiosFileExistsSync(const SceFiosOpAttr* attr, const char* path) {
    return ExistsSync(QueryExists(attr, path, nullptr, Kind::File));
}

SceFiosOp APS5_VABI sceFiosDirectoryExists(const SceFiosOpAttr* attr, const char* path, bool* outExists) {
    return QueryExists(attr, path, outExists, Kind::Directory);
}

bool APS5_VABI sceFiosDirectoryExistsSync(const SceFiosOpAttr* attr, const char* path) {
    return ExistsSync(QueryExists(attr, path, nullptr, Kind::Directory));
}

static SceFiosOp QuerySize(const SceFiosOpAttr* attr, const char* path) {
    Fios::OpInfo info;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return Fios::SubmitFailed(attr, std::move(info), error);
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [target](Fios::Completion& completion) {
        SceFiosStat stat{};
        if (const std::int32_t error = Fios::StatPath(target, stat)) {
            completion.error = error;
        } else if ((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0) {
            completion.error = SCE_FIOS_ERROR_NOT_A_FILE;
        } else {
            completion.result = stat.fileSize;
        }
    });
}

SceFiosOp APS5_VABI sceFiosFileGetSize(const SceFiosOpAttr* attr, const char* path) {
    return QuerySize(attr, path);
}

SceFiosSize APS5_VABI sceFiosFileGetSizeSync(const SceFiosOpAttr* attr, const char* path) {
    return Fios::SyncCount(QuerySize(attr, path));
}

static SceFiosOp QueryStat(const SceFiosOpAttr* attr, const char* path, SceFiosStat* outStatus) {
    Fios::OpInfo info;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return Fios::SubmitFailed(attr, std::move(info), error);
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [target, outStatus](Fios::Completion& completion) {
        SceFiosStat stat{};
        if (const std::int32_t error = Fios::StatPath(target, stat)) {
            completion.error = error;
        } else if (outStatus == nullptr || !Fios::CopyOut(outStatus, &stat, sizeof(stat))) {
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
        }
    });
}

SceFiosOp APS5_VABI sceFiosStat(const SceFiosOpAttr* attr, const char* path, SceFiosStat* outStatus) {
    return QueryStat(attr, path, outStatus);
}

int APS5_VABI sceFiosStatSync(const SceFiosOpAttr* attr, const char* path, SceFiosStat* outStatus) {
    return Fios::SyncStatus(QueryStat(attr, path, outStatus));
}

}
