#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <limits>
#include <string_view>
#include <vector>

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

int ComparePaths(const char* left, const char* right, std::size_t count, bool limited) {
    std::string first;
    std::string second;
    if (left == nullptr || right == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    const int firstError = CopyGuestString_nid_no_patch(left, SCE_FIOS_PATH_MAX, first);
    const int secondError = CopyGuestString_nid_no_patch(right, SCE_FIOS_PATH_MAX, second);
    if (firstError == 14 || secondError == 14) return SCE_FIOS_ERROR_BAD_PTR;
    if (firstError != 0 || secondError != 0) return SCE_FIOS_ERROR_PATH_TOO_LONG;
    if (!limited) count = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < count; ++i) {
        const auto a = static_cast<unsigned char>(first[i]);
        const auto b = static_cast<unsigned char>(second[i]);
        if (a != b) return static_cast<int>(a) - static_cast<int>(b);
        if (a == 0) return 0;
    }
    return 0;
}

std::string NormalizeGuestPath(std::string_view path) {
    std::vector<std::string_view> parts;
    const bool absolute = !path.empty() && path.front() == '/';
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view part = path.substr(start, end - start);
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") parts.pop_back();
            else if (!absolute) parts.push_back(part);
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        start = end + 1;
    }
    std::string normalized = absolute ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) normalized += '/';
        normalized += parts[i];
    }
    return normalized;
}

}

extern "C" {

std::size_t APS5_VABI sceFiosPathNormalize(char* output, const char* path, std::size_t capacity) {
    std::string input;
    if (output == nullptr || path == nullptr) return static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR);
    const int copyError = CopyGuestString_nid_no_patch(path, SCE_FIOS_PATH_MAX, input);
    if (copyError == 14) return static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR);
    if (copyError != 0) return static_cast<std::size_t>(SCE_FIOS_ERROR_PATH_TOO_LONG);
    std::replace(input.begin(), input.end(), '\\', '/');
    std::string normalized = NormalizeGuestPath(input);
    if (normalized.empty()) normalized = ".";
    while (normalized.size() > 1 && normalized.back() == '/') normalized.pop_back();
    if (normalized.size() + 1 > capacity) {
        const char terminator = 0;
        if (capacity != 0 && !Fios::CopyOut(output, &terminator, 1)) return static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR);
        return capacity + 1;
    }
    if (!Fios::CopyOut(output, normalized.c_str(), normalized.size() + 1)) return static_cast<std::size_t>(SCE_FIOS_ERROR_BAD_PTR);
    return normalized.size() + 1;
}

int APS5_VABI sceFiosPathcmp(const char* left, const char* right) {
    return ComparePaths(left, right, 0, false);
}

int APS5_VABI sceFiosPathncmp(const char* left, const char* right, std::size_t count) {
    return ComparePaths(left, right, count, true);
}

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
