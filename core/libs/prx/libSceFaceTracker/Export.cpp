#include "prx/libSceFaceTracker/include/Api.hpp"
#include "prx/libSceFace/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
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

uint8_t *ctxByte(void *context, std::size_t off) {
    return static_cast<uint8_t *>(context) + off;
}

int32_t ctxI32(void *context, std::size_t off) {
    int32_t v;
    std::memcpy(&v, ctxByte(context, off), sizeof(v));
    return v;
}

void ctxStore(void *context, std::size_t off, void *value) {
    std::memcpy(ctxByte(context, off), &value, sizeof(value));
}

uint8_t *userDb(void *context) {
    return ctxByte(context, kUserDb);
}

int32_t userDbCount(void *context) {
    int32_t v;
    std::memcpy(&v, userDb(context) + kUserDbCount, sizeof(v));
    return v;
}

uint8_t *userDbEntries(void *context) {
    uint8_t *p;
    std::memcpy(&p, userDb(context) + kUserDbEntries, sizeof(p));
    return p;
}

uint8_t *userDbFeatures(void *context) {
    uint8_t *p;
    std::memcpy(&p, userDb(context) + kUserDbFeatures, sizeof(p));
    return p;
}

bool entryUsed(uint8_t *entry) {
    return entry[0] != 0;
}

int32_t entryUserId(uint8_t *entry) {
    int32_t v;
    std::memcpy(&v, entry + 8, sizeof(v));
    return v;
}

int32_t trackCount(void *context) {
    return ctxI32(context, kTrackCount);
}

uint8_t *trackRecord(void *context, int32_t index) {
    uint8_t *base;
    std::memcpy(&base, ctxByte(context, kTrackBase), sizeof(base));
    return base + static_cast<std::size_t>(index) * kRecordStride;
}

int32_t recordI32(uint8_t *record, std::size_t off) {
    int32_t v;
    std::memcpy(&v, record + off, sizeof(v));
    return v;
}

int registerCallback(void *callback, void *userArg, void *context,
                     std::size_t cbOff, std::size_t argOff) {
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    ctxStore(context, cbOff, callback);
    ctxStore(context, argOff, userArg);
    return 0;
}

int userDbRegister(void *context, int userId, const void *feature, int featureSize) {
    if (userId <= 0 || feature == nullptr || featureSize > kMaxFeatureSize ||
        featureSize <= 0) {
        return kErrInvalid;
    }
    const int32_t count = userDbCount(context);
    if (count <= 0) {
        return kErrFull;
    }
    uint8_t *entries = userDbEntries(context);
    int32_t freeIndex = -1;
    bool haveFree = false;
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *entry = entries + static_cast<std::size_t>(i) * kUserEntryStride;
        if (!haveFree && !entryUsed(entry)) {
            freeIndex = i;
            haveFree = true;
        }
        if (entryUserId(entry) == userId) {
            return kErrInvalid;
        }
    }
    if (freeIndex < 0) {
        return kErrFull;
    }
    std::memcpy(userDbFeatures(context) + static_cast<std::size_t>(freeIndex) * kFeatureStride,
                feature, static_cast<std::size_t>(featureSize));
    uint8_t *entry = entries + static_cast<std::size_t>(freeIndex) * kUserEntryStride;
    entry[0] = 1;
    std::memcpy(entry + 8, &userId, sizeof(userId));
    return 0;
}

void wipeFeature(uint8_t *block) {
    const uint64_t sentinel = 0x1ffffffff;
    const uint64_t zero = 0;
    for (int i = 0; i < kFeatureElementCount; ++i) {
        uint8_t *element = block + static_cast<std::size_t>(i) * kFeatureElementStride;
        std::memset(element, 0, kFeatureElementStride);
        std::memcpy(element + 0x400, &sentinel, sizeof(sentinel));
        std::memcpy(element + 0x408, &zero, sizeof(zero));
    }
}

int userDbUnregister(void *context, int userId) {
    if (userId <= 0) {
        return kErrInvalid;
    }
    const int32_t count = userDbCount(context);
    if (count <= 0) {
        return kErrNotFound;
    }
    uint8_t *entries = userDbEntries(context);
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *entry = entries + static_cast<std::size_t>(i) * kUserEntryStride;
        if (!entryUsed(entry) || entryUserId(entry) != userId) {
            continue;
        }
        const int32_t neg = -1;
        std::memcpy(entry + 4, &neg, sizeof(neg));
        std::memcpy(entry + 8, &neg, sizeof(neg));
        entry[0] = 0;
        wipeFeature(userDbFeatures(context) + static_cast<std::size_t>(i) * kFeatureStride);
        return 0;
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
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    return userDbRegister(context, userId, feature, featureSize);
}

int APS5_VABI sceFaceTrackerUnregisterUser(int userId, void *context) {
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    const int ret = userDbUnregister(context, userId);
    if (ret != 0) {
        return ret;
    }
    const int32_t count = trackCount(context);
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *record = trackRecord(context, i);
        if (recordI32(record, kRecordUserId) == userId) {
            const int32_t neg = -1;
            std::memcpy(record + kRecordUserId, &neg, sizeof(neg));
        }
    }
    return 0;
}

int APS5_VABI sceFaceTrackerAddUserFeature() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetUserFeature(int userId, void **feature, int *featureSize, void *context) {
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    if (userId <= 0) {
        return kErrInvalid;
    }
    const int32_t count = userDbCount(context);
    if (count <= 0) {
        return kErrNotFound;
    }
    uint8_t *entries = userDbEntries(context);
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *entry = entries + static_cast<std::size_t>(i) * kUserEntryStride;
        if (!entryUsed(entry) || entryUserId(entry) != userId) {
            continue;
        }
        uint8_t *block = userDbFeatures(context) + static_cast<std::size_t>(i) * kFeatureStride;
        std::memcpy(feature, &block, sizeof(block));
        const int32_t size = kMaxFeatureSize;
        std::memcpy(featureSize, &size, sizeof(size));
        return 0;
    }
    return kErrNotFound;
}

int APS5_VABI sceFaceTrackerGetUserIdByTrackId(int trackId, void *context) {
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    if (trackId < 0) {
        return kErrInvalid;
    }
    const int32_t count = trackCount(context);
    if (count <= 0) {
        return kErrNotFound;
    }
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *record = trackRecord(context, i);
        if (recordI32(record, kRecordTrackId) == trackId) {
            return recordI32(record, kRecordUserId);
        }
    }
    return kErrNotFound;
}

int APS5_VABI sceFaceTrackerGetTrackIdByUserId(int userId, void *context) {
    if (context == nullptr) {
        return kErrNullContext;
    }
    if (ctxByte(context, kInitFlag)[0] == 0) {
        return kErrNotInitialized;
    }
    if (userId <= 0) {
        return kErrInvalid;
    }
    const int32_t count = trackCount(context);
    if (count <= 0) {
        return kErrNotFound;
    }
    for (int32_t i = 0; i < count; ++i) {
        uint8_t *record = trackRecord(context, i);
        if (recordI32(record, kRecordUserId) == userId) {
            return recordI32(record, kRecordTrackId);
        }
    }
    return kErrNotFound;
}

int APS5_VABI sceFaceTrackerGetDefaultConfig(void *config) {
    if (config == nullptr) {
        return kErrInvalid;
    }
    std::uint32_t param[0x38 / 4] = {};
    auto *out = static_cast<std::uint32_t *>(config);
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
    static_cast<std::uint8_t *>(config)[0x64] = 1;
    out[0x68 / 4] = 0xf;
    out[0x6c / 4] = 0x42500000;
    out[0x70 / 4] = 0x1e;
    out[0x74 / 4] = 0xf;
    out[0x78 / 4] = 5;
    static_cast<std::uint16_t *>(config)[0x7c / 2] = 0x101;
    return 0;
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
