#include "prx/libSceIcu/Bidi.hpp"
#include "prx/libc/include/General.hpp"
#include "ubidi_props.h"
#include "unicode/ubidi.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

bool CopyInPreservingErrno(void* dst, const void* src, std::size_t bytes) {
    PreservedErrno guard;
    return GuestCopyIn_nid_no_patch(dst, src, bytes);
}

bool CopyOutPreservingErrno(void* dst, const void* src, std::size_t bytes) {
    PreservedErrno guard;
    return GuestCopyOut_nid_no_patch(dst, src, bytes);
}

bool WritablePreservingErrno(void* dst, std::size_t bytes) {
    PreservedErrno guard;
    return GuestWritable_nid_no_patch(dst, bytes);
}

struct ClassCallback {
    Icu::BiDiClassCallback function = nullptr;
    const void* context = nullptr;
};

std::mutex classCallbackMutex;
std::unordered_map<const void*, std::unique_ptr<ClassCallback>> classCallbacks;

UCharDirection U_CALLCONV ForwardClass(const void* context, UChar32 character) {
    const auto& callback = *static_cast<const ClassCallback*>(context);
    return static_cast<UCharDirection>(callback.function(callback.context, character));
}

UBiDi* Native(Icu::BiDi* bidi) {
    return reinterpret_cast<UBiDi*>(bidi);
}

const UBiDi* Native(const Icu::BiDi* bidi) {
    return reinterpret_cast<const UBiDi*>(bidi);
}

bool EnterStatus(std::int32_t* error, UErrorCode& status, std::int32_t& previous, const char* caller) {
    if (!CopyInPreservingErrno(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(caller);
    status = static_cast<UErrorCode>(previous);
    if (U_FAILURE(status)) return false;
    if (!WritablePreservingErrno(error, sizeof(*error))) NotImplemented_nid_no_patch(caller);
    return true;
}

void LeaveStatus(std::int32_t* error, UErrorCode status, std::int32_t previous, const char* caller) {
    const auto updated = static_cast<std::int32_t>(status);
    if (updated != previous && !CopyOutPreservingErrno(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(caller);
}

template <typename T>
T* PrepareOutput(T* guest, T& host, const char* caller) {
    if (!guest) return nullptr;
    if (!CopyInPreservingErrno(&host, guest, sizeof(host)) || !WritablePreservingErrno(guest, sizeof(*guest))) {
        NotImplemented_nid_no_patch(caller);
    }
    return &host;
}

template <typename T>
void CommitOutput(T* guest, const T& host, const char* caller) {
    if (guest && !CopyOutPreservingErrno(guest, &host, sizeof(host))) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

Icu::BiDi* APS5_VABI ubidi_open_nid_postfix() {
    return reinterpret_cast<Icu::BiDi*>(ubidi_open());
}

void APS5_VABI ubidi_close_nid_postfix(Icu::BiDi* bidi) {
    std::unique_ptr<ClassCallback> callback;
    {
        std::lock_guard lock(classCallbackMutex);
        const auto entry = classCallbacks.find(bidi);
        if (entry != classCallbacks.end()) {
            callback = std::move(entry->second);
            classCallbacks.erase(entry);
        }
    }
    ubidi_close(Native(bidi));
}

void APS5_VABI ubidi_setPara_nid_postfix(Icu::BiDi* bidi, const char16_t* text, std::int32_t length, std::uint8_t level,
                                         std::uint8_t* embeddingLevels, std::int32_t* error) {
    if (!error) {
        ubidi_setPara(Native(bidi), text, length, level, embeddingLevels, nullptr);
        return;
    }
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    ubidi_setPara(Native(bidi), text, length, level, embeddingLevels, &status);
    LeaveStatus(error, status, previous, __func__);
}

std::uint8_t APS5_VABI ubidi_getParaLevel_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getParaLevel(Native(bidi));
}

std::int32_t APS5_VABI ubidi_countRuns_nid_postfix(Icu::BiDi* bidi, std::int32_t* error) {
    if (!error) return ubidi_countRuns(Native(bidi), nullptr);
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return -1;
    const auto runs = ubidi_countRuns(Native(bidi), &status);
    LeaveStatus(error, status, previous, __func__);
    return runs;
}

std::int32_t APS5_VABI ubidi_getVisualRun_nid_postfix(Icu::BiDi* bidi, std::int32_t run, std::int32_t* logicalStart,
                                                      std::int32_t* length) {
    std::int32_t hostStart = 0;
    std::int32_t hostLength = 0;
    auto* outStart = PrepareOutput(logicalStart, hostStart, __func__);
    auto* outLength = PrepareOutput(length, hostLength, __func__);
    const auto direction = ubidi_getVisualRun(Native(bidi), run, outStart, outLength);
    CommitOutput(logicalStart, hostStart, __func__);
    CommitOutput(length, hostLength, __func__);
    return direction;
}

void APS5_VABI ubidi_getLogicalRun_nid_postfix(const Icu::BiDi* bidi, std::int32_t position, std::int32_t* logicalLimit,
                                               std::uint8_t* level) {
    if (!bidi) APS5_INVALID_ARG_EX;
    std::int32_t hostLimit = 0;
    std::uint8_t hostLevel = 0;
    auto* outLimit = PrepareOutput(logicalLimit, hostLimit, __func__);
    auto* outLevel = PrepareOutput(level, hostLevel, __func__);
    ubidi_getLogicalRun(Native(bidi), position, outLimit, outLevel);
    CommitOutput(logicalLimit, hostLimit, __func__);
    CommitOutput(level, hostLevel, __func__);
}

std::int32_t APS5_VABI ubidi_getBaseDirection_nid_postfix(const char16_t* text, std::int32_t length) {
    return ubidi_getBaseDirection(text, length);
}

void APS5_VABI ubidi_reorderVisual_nid_postfix(const std::uint8_t* levels, std::int32_t length, std::int32_t* indexMap) {
    if (!levels || length <= 0 || !indexMap) {
        ubidi_reorderVisual(levels, length, indexMap);
        return;
    }
    const auto count = static_cast<std::size_t>(length);
    std::vector<std::uint8_t> hostLevels(count);
    if (!CopyInPreservingErrno(hostLevels.data(), levels, hostLevels.size())) NotImplemented_nid_no_patch(__func__);
    const std::size_t mapBytes = count * sizeof(std::int32_t);
    if (!WritablePreservingErrno(indexMap, mapBytes)) NotImplemented_nid_no_patch(__func__);
    std::vector<std::int32_t> hostMap(count);
    ubidi_reorderVisual(hostLevels.data(), length, hostMap.data());
    if (!CopyOutPreservingErrno(indexMap, hostMap.data(), mapBytes)) NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ubidi_setClassCallback_nid_postfix(Icu::BiDi* bidi, Icu::BiDiClassCallback function, const void* context,
                                                  Icu::BiDiClassCallback* oldFunction, const void** oldContext,
                                                  std::int32_t* error) {
    if (!error) return;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    if (!bidi) {
        LeaveStatus(error, U_ILLEGAL_ARGUMENT_ERROR, previous, __func__);
        return;
    }
    if (oldFunction && !WritablePreservingErrno(oldFunction, sizeof(*oldFunction))) NotImplemented_nid_no_patch(__func__);
    if (oldContext && !WritablePreservingErrno(oldContext, sizeof(*oldContext))) NotImplemented_nid_no_patch(__func__);

    std::lock_guard lock(classCallbackMutex);
    auto& callback = classCallbacks[bidi];
    if (!callback) callback = std::make_unique<ClassCallback>();
    const auto previousFunction = callback->function;
    const auto* previousContext = callback->context;
    callback->function = function;
    callback->context = context;
    ubidi_setClassCallback(Native(bidi), function ? ForwardClass : nullptr, callback.get(), nullptr, nullptr, &status);
    CommitOutput(oldFunction, previousFunction, __func__);
    CommitOutput(oldContext, previousContext, __func__);
    LeaveStatus(error, status, previous, __func__);
}

std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t character) {
    return ubidi_getClass(character);
}

}
