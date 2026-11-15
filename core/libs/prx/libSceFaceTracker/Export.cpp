#include "prx/libSceFaceTracker/include/Api.hpp"
#include "prx/libSceFace/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

}

namespace {

constexpr int kErrInvalid = 0x812a0001;
constexpr int kErrNullContext = 0x812a0002;
constexpr int kErrNotInitialized = 0x812a0003;
constexpr int kErrFull = 0x812a0005;
constexpr int kErrNotFound = 0x812a0006;

constexpr std::size_t kUserDb = 0x1598;
constexpr std::size_t kTrackCount = 0x1778;
constexpr std::size_t kTrackBase = 0x1780;
constexpr std::size_t kInitFlag = 0x180c;
constexpr std::size_t kRecordStride = 0x1340;
constexpr std::size_t kRecordTrackId = 0x18;
constexpr std::size_t kRecordUserId = 0x24;
constexpr std::size_t kUserDbEntries = 0x28;
constexpr std::size_t kUserDbFeatures = 0x30;
constexpr std::size_t kUserDbCount = 0x48;
constexpr std::size_t kUserEntryStride = 0xc;
constexpr std::size_t kFeatureStride = 0x4100;
constexpr int kMaxFeatureSize = 0x4100;
constexpr std::size_t kFeatureElementStride = 0x410;
constexpr int kFeatureElementCount = 0x10;

bool CopyIn(void *destination, const void *source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool CopyOut(void *destination, const void *source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool ValidSpan(const void *base, std::size_t bytes) {
    const auto addr = reinterpret_cast<std::uintptr_t>(base);
    return addr != 0 && (bytes == 0 || bytes - 1 <= std::numeric_limits<std::uintptr_t>::max() - addr);
}

bool WritableSpan(void *base, std::size_t bytes) {
    if (!ValidSpan(base, bytes)) {
        return false;
    }
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(base, bytes);
    errno = savedErrno;
    return ok;
}

template <typename T>
bool OffsetAddress(const void *base, std::size_t offset, T *&out) {
    const auto addr = reinterpret_cast<std::uintptr_t>(base);
    if (addr == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - addr) {
        return false;
    }
    out = reinterpret_cast<T *>(addr + offset);
    return true;
}

bool ElementAddress(void *base, std::int32_t index, std::size_t stride, std::size_t elemSize, void *&out) {
    const auto addr = reinterpret_cast<std::uintptr_t>(base);
    if (addr == 0 || index < 0) {
        return false;
    }
    const auto uindex = static_cast<std::size_t>(index);
    if (uindex > (std::numeric_limits<std::uintptr_t>::max() - addr) / stride) {
        return false;
    }
    const auto elemAddr = addr + uindex * stride;
    if (elemSize > 0 && elemSize - 1 > std::numeric_limits<std::uintptr_t>::max() - elemAddr) {
        return false;
    }
    out = reinterpret_cast<void *>(elemAddr);
    return true;
}

template <typename T>
bool ReadAt(const void *base, std::size_t offset, T &value) {
    const void *src = nullptr;
    if (!OffsetAddress(base, offset, src) || !ValidSpan(src, sizeof(T))) {
        return false;
    }
    return CopyIn(&value, src, sizeof(T));
}

template <typename T>
bool WriteAt(void *base, std::size_t offset, const T &value) {
    void *dst = nullptr;
    if (!OffsetAddress(base, offset, dst) || !ValidSpan(dst, sizeof(T))) {
        return false;
    }
    return CopyOut(dst, &value, sizeof(T));
}

bool CheckContext(const void *context, int &error) {
    if (context == nullptr) {
        error = kErrNullContext;
        return false;
    }
    std::uint8_t initFlag = 0;
    if (!ReadAt(context, kInitFlag, initFlag)) {
        error = kErrInvalid;
        return false;
    }
    if (initFlag == 0) {
        error = kErrNotInitialized;
        return false;
    }
    return true;
}

int registerCallback(void *callback, void *userArg, void *context,
                     std::size_t cbOff, std::size_t argOff) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    void *cbDst = nullptr;
    void *argDst = nullptr;
    if (!OffsetAddress(context, cbOff, cbDst) || !OffsetAddress(context, argOff, argDst) ||
        !WritableSpan(cbDst, sizeof(callback)) || !WritableSpan(argDst, sizeof(userArg))) {
        return kErrInvalid;
    }
    if (!CopyOut(cbDst, &callback, sizeof(callback)) || !CopyOut(argDst, &userArg, sizeof(userArg))) {
        return kErrInvalid;
    }
    return 0;
}

int userDbRegister(void *context, int userId, const void *feature, int featureSize) {
    if (userId <= 0 || feature == nullptr || featureSize > kMaxFeatureSize ||
        featureSize <= 0) {
        return kErrInvalid;
    }
    std::vector<std::uint8_t> localFeature(static_cast<std::size_t>(featureSize));
    if (!CopyIn(localFeature.data(), feature, localFeature.size())) {
        return kErrInvalid;
    }
    int32_t count = 0;
    if (!ReadAt(context, kUserDb + kUserDbCount, count)) {
        return kErrInvalid;
    }
    if (count <= 0) {
        return kErrFull;
    }
    void *entries = nullptr;
    if (!ReadAt(context, kUserDb + kUserDbEntries, entries) || entries == nullptr) {
        return kErrInvalid;
    }
    int32_t freeIndex = -1;
    bool haveFree = false;
    for (int32_t i = 0; i < count; ++i) {
        void *entry = nullptr;
        std::uint8_t used = 0;
        std::int32_t existingUserId = 0;
        if (!ElementAddress(entries, i, kUserEntryStride, kUserEntryStride, entry) ||
            !ReadAt(entry, 0, used) || !ReadAt(entry, 8, existingUserId)) {
            return kErrInvalid;
        }
        if (!haveFree && used == 0) {
            freeIndex = i;
            haveFree = true;
        }
        if (existingUserId == userId) {
            return kErrInvalid;
        }
    }
    if (freeIndex < 0) {
        return kErrFull;
    }
    void *features = nullptr;
    if (!ReadAt(context, kUserDb + kUserDbFeatures, features) || features == nullptr) {
        return kErrInvalid;
    }
    void *featureDest = nullptr;
    void *entryDest = nullptr;
    if (!ElementAddress(features, freeIndex, kFeatureStride, localFeature.size(), featureDest) ||
        !ElementAddress(entries, freeIndex, kUserEntryStride, kUserEntryStride, entryDest)) {
        return kErrInvalid;
    }
    if (!WritableSpan(featureDest, localFeature.size()) || !WritableSpan(entryDest, kUserEntryStride)) {
        return kErrInvalid;
    }
    const std::uint8_t occupied = 1;
    if (!CopyOut(featureDest, localFeature.data(), localFeature.size()) ||
        !WriteAt(entryDest, 0, occupied) ||
        !WriteAt(entryDest, 8, userId)) {
        return kErrInvalid;
    }
    return 0;
}

bool wipeFeature(void *block) {
    const uint64_t sentinel = 0x1ffffffff;
    const uint64_t zero = 0;
    std::vector<std::uint8_t> wiped(kFeatureStride, 0);
    for (int i = 0; i < kFeatureElementCount; ++i) {
        std::uint8_t *element = wiped.data() + static_cast<std::size_t>(i) * kFeatureElementStride;
        std::memcpy(element + 0x400, &sentinel, sizeof(sentinel));
        std::memcpy(element + 0x408, &zero, sizeof(zero));
    }
    return CopyOut(block, wiped.data(), wiped.size());
}

int userDbUnregister(void *context, int userId) {
    if (userId <= 0) {
        return kErrInvalid;
    }
    int32_t count = 0;
    if (!ReadAt(context, kUserDb + kUserDbCount, count)) {
        return kErrInvalid;
    }
    if (count <= 0) {
        return kErrNotFound;
    }
    void *entries = nullptr;
    if (!ReadAt(context, kUserDb + kUserDbEntries, entries) || entries == nullptr) {
        return kErrInvalid;
    }
    for (int32_t i = 0; i < count; ++i) {
        void *entry = nullptr;
        std::uint8_t used = 0;
        std::int32_t existingUserId = 0;
        if (!ElementAddress(entries, i, kUserEntryStride, kUserEntryStride, entry) ||
            !ReadAt(entry, 0, used) || !ReadAt(entry, 8, existingUserId)) {
            return kErrInvalid;
        }
        if (used == 0 || existingUserId != userId) {
            continue;
        }
        void *features = nullptr;
        void *featureBlock = nullptr;
        if (!ReadAt(context, kUserDb + kUserDbFeatures, features) || features == nullptr ||
            !ElementAddress(features, i, kFeatureStride, kFeatureStride, featureBlock)) {
            return kErrInvalid;
        }
        if (!WritableSpan(entry, kUserEntryStride) || !WritableSpan(featureBlock, kFeatureStride)) {
            return kErrInvalid;
        }
        const int32_t neg = -1;
        const std::uint8_t cleared = 0;
        if (!WriteAt(entry, 4, neg) || !WriteAt(entry, 8, neg) ||
            !WriteAt(entry, 0, cleared) || !wipeFeature(featureBlock)) {
            return kErrInvalid;
        }
        return 0;
    }
    return kErrNotFound;
}

int lookupTrackField(void *context, std::size_t matchOffset, std::int32_t matchValue, std::size_t resultOffset) {
    int32_t count = 0;
    if (!ReadAt(context, kTrackCount, count)) {
        return kErrInvalid;
    }
    if (count <= 0) {
        return kErrNotFound;
    }
    void *base = nullptr;
    if (!ReadAt(context, kTrackBase, base) || base == nullptr) {
        return kErrInvalid;
    }
    for (int32_t i = 0; i < count; ++i) {
        void *record = nullptr;
        std::int32_t candidate = 0;
        if (!ElementAddress(base, i, kRecordStride, kRecordStride, record) ||
            !ReadAt(record, matchOffset, candidate)) {
            return kErrInvalid;
        }
        if (candidate == matchValue) {
            std::int32_t value = 0;
            if (!ReadAt(record, resultOffset, value)) {
                return kErrInvalid;
            }
            return value;
        }
    }
    return kErrNotFound;
}

}
extern "C" {

int APS5_VABI sceFaceTrackerRegisterStartTrackingCallback(void *callback, void *userArg, void *context) {
    return registerCallback(callback, userArg, context, 0x1810, 0x1838);
}

int APS5_VABI sceFaceTrackerRegisterFixUserIdCallback(void *callback, void *userArg, void *context) {
    return registerCallback(callback, userArg, context, 0x1818, 0x1840);
}

int APS5_VABI sceFaceTrackerRegisterStopTrackingCallback(void *callback, void *userArg, void *context) {
    return registerCallback(callback, userArg, context, 0x1820, 0x1848);
}

int APS5_VABI sceFaceTrackerRegisterLostTargetCallback(void *callback, void *userArg, void *context) {
    return registerCallback(callback, userArg, context, 0x1828, 0x1850);
}

int APS5_VABI sceFaceTrackerRegisterRecoveryFromLostTargetCallback(void *callback, void *userArg, void *context) {
    return registerCallback(callback, userArg, context, 0x1830, 0x1858);
}

int APS5_VABI sceFaceTrackerStartTracking() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerStopTracking() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerReset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterUser(int userId, const void *feature, int featureSize, void *context) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    return userDbRegister(context, userId, feature, featureSize);
}

int APS5_VABI sceFaceTrackerUnregisterUser(int userId, void *context) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    const int ret = userDbUnregister(context, userId);
    if (ret != 0) {
        return ret;
    }
    int32_t count = 0;
    if (!ReadAt(context, kTrackCount, count)) {
        return kErrInvalid;
    }
    if (count <= 0) {
        return 0;
    }
    void *base = nullptr;
    if (!ReadAt(context, kTrackBase, base) || base == nullptr) {
        return kErrInvalid;
    }
    for (int32_t i = 0; i < count; ++i) {
        void *record = nullptr;
        std::int32_t recordUser = 0;
        if (!ElementAddress(base, i, kRecordStride, kRecordStride, record) ||
            !ReadAt(record, kRecordUserId, recordUser)) {
            return kErrInvalid;
        }
        if (recordUser == userId) {
            const int32_t neg = -1;
            if (!WriteAt(record, kRecordUserId, neg)) {
                return kErrInvalid;
            }
        }
    }
    return 0;
}

int APS5_VABI sceFaceTrackerAddUserFeature() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetUserFeature(int userId, void **feature, int *featureSize, void *context) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    if (userId <= 0 || !feature || !featureSize) {
        return kErrInvalid;
    }
    void *probeFeature = nullptr;
    int probeSize = 0;
    if (!CopyIn(&probeFeature, feature, sizeof(probeFeature)) ||
        !CopyIn(&probeSize, featureSize, sizeof(probeSize))) {
        return kErrInvalid;
    }
    int32_t count = 0;
    if (!ReadAt(context, kUserDb + kUserDbCount, count)) {
        return kErrInvalid;
    }
    if (count <= 0) {
        return kErrNotFound;
    }
    void *entries = nullptr;
    if (!ReadAt(context, kUserDb + kUserDbEntries, entries) || entries == nullptr) {
        return kErrInvalid;
    }
    for (int32_t i = 0; i < count; ++i) {
        void *entry = nullptr;
        std::uint8_t used = 0;
        std::int32_t existingUserId = 0;
        if (!ElementAddress(entries, i, kUserEntryStride, kUserEntryStride, entry) ||
            !ReadAt(entry, 0, used) || !ReadAt(entry, 8, existingUserId)) {
            return kErrInvalid;
        }
        if (used == 0 || existingUserId != userId) {
            continue;
        }
        void *features = nullptr;
        void *block = nullptr;
        if (!ReadAt(context, kUserDb + kUserDbFeatures, features) || features == nullptr ||
            !ElementAddress(features, i, kFeatureStride, kFeatureStride, block)) {
            return kErrInvalid;
        }
        const int32_t size = kMaxFeatureSize;
        if (!WritableSpan(feature, sizeof(block)) || !WritableSpan(featureSize, sizeof(size)) ||
            !CopyOut(feature, &block, sizeof(block)) || !CopyOut(featureSize, &size, sizeof(size))) {
            return kErrInvalid;
        }
        return 0;
    }
    return kErrNotFound;
}

int APS5_VABI sceFaceTrackerGetUserIdByTrackId(int trackId, void *context) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    if (trackId < 0) {
        return kErrInvalid;
    }
    return lookupTrackField(context, kRecordTrackId, trackId, kRecordUserId);
}

int APS5_VABI sceFaceTrackerGetTrackIdByUserId(int userId, void *context) {
    if (int error = 0; !CheckContext(context, error)) {
        return error;
    }
    if (userId <= 0) {
        return kErrInvalid;
    }
    return lookupTrackField(context, kRecordUserId, userId, kRecordTrackId);
}

int APS5_VABI sceFaceTrackerGetDefaultConfig(void *config) {
    if (config == nullptr) {
        return kErrInvalid;
    }
    std::uint32_t param[0x38 / 4] = {};
    std::array<std::uint8_t, 0x7e> localConfig{};
    auto *out = reinterpret_cast<std::uint32_t *>(localConfig.data());
    if (sceFaceDetectionGetDefaultParam(param) == 0) {
        out[0x00 / 4] = param[0x08 / 4];
        out[0x04 / 4] = param[0x10 / 4];
        out[0x08 / 4] = param[0x0c / 4];
        out[0x0c / 4] = 0;
        out[0x10 / 4] = 0;
        out[0x14 / 4] = param[0x1c / 4];
        out[0x18 / 4] = param[0x20 / 4];
        out[0x1c / 4] = 0;
        out[0x20 / 4] = 0;
        out[0x24 / 4] = param[0x2c / 4];
        out[0x28 / 4] = param[0x30 / 4];
        out[0x2c / 4] = 0;
        out[0x30 / 4] = 0x20;
        out[0x34 / 4] = 4;
    }
    out[0x38 / 4] = 0x3f574bc7;
    out[0x3c / 4] = 0x40000000;
    out[0x40 / 4] = 0x40000000;
    out[0x44 / 4] = 2;
    out[0x48 / 4] = 2;
    out[0x4c / 4] = 0x3e99999a;
    out[0x50 / 4] = 4;
    out[0x54 / 4] = 0x42b40000;
    out[0x58 / 4] = 5;
    out[0x5c / 4] = 0x42480000;
    out[0x60 / 4] = 0xa;
    localConfig[0x64] = 1;
    out[0x68 / 4] = 0xf;
    out[0x6c / 4] = 0x42500000;
    out[0x70 / 4] = 0x1e;
    out[0x74 / 4] = 0xf;
    out[0x78 / 4] = 5;
    const std::uint16_t tail = 0x101;
    std::memcpy(localConfig.data() + 0x7c, &tail, sizeof(tail));
    return CopyOut(config, localConfig.data(), localConfig.size()) ? 0 : kErrInvalid;
}

int APS5_VABI sceFaceTrackerGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerSearchTarget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
