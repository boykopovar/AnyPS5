#include "prx/libSceIcu/Bidi.hpp"
#include "prx/libc/include/General.hpp"
#include "ubidi_props.h"
#include "unicode/ubidi.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::int32_t Unwritten = -1;

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
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) throw std::runtime_error(caller);
    status = static_cast<UErrorCode>(previous);
    return !U_FAILURE(status);
}

void LeaveStatus(std::int32_t* error, UErrorCode status, std::int32_t previous, const char* caller) {
    const auto updated = static_cast<std::int32_t>(status);
    if (updated != previous && !GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) throw std::runtime_error(caller);
}

template <typename TFirst, typename TSecond>
void CommitOutputs(TFirst* first, const TFirst& hostFirst, TSecond* second, const TSecond& hostSecond,
                   const char* caller) {
    if (first && second && !GuestWritable_nid_no_patch(second, sizeof(*second))) throw std::runtime_error(caller);
    if (first && !GuestCopyOut_nid_no_patch(first, &hostFirst, sizeof(hostFirst))) throw std::runtime_error(caller);
    if (second && !GuestCopyOut_nid_no_patch(second, &hostSecond, sizeof(hostSecond))) throw std::runtime_error(caller);
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
    std::int32_t hostStart = Unwritten;
    std::int32_t hostLength = 0;
    const auto direction = ubidi_getVisualRun(Native(bidi), run, &hostStart, &hostLength);
    if (hostStart != Unwritten) CommitOutputs(logicalStart, hostStart, length, hostLength, __func__);
    return direction;
}

void APS5_VABI ubidi_getLogicalRun_nid_postfix(const Icu::BiDi* bidi, std::int32_t position, std::int32_t* logicalLimit,
                                               std::uint8_t* level) {
    if (!bidi) APS5_INVALID_ARG_EX;
    std::int32_t hostLimit = Unwritten;
    std::uint8_t hostLevel = 0;
    ubidi_getLogicalRun(Native(bidi), position, &hostLimit, &hostLevel);
    if (hostLimit != Unwritten) CommitOutputs(logicalLimit, hostLimit, level, hostLevel, __func__);
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
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(std::int32_t)) throw std::runtime_error(__func__);
    const std::size_t mapBytes = count * sizeof(std::int32_t);
    constexpr std::size_t ProbeChunk = 4096;
    if (count > ProbeChunk) {
        std::uint8_t probe[ProbeChunk];
        for (std::size_t offset = 0; offset < count; offset += ProbeChunk) {
            const std::size_t chunk = std::min(ProbeChunk, count - offset);
            if (!GuestCopyIn_nid_no_patch(probe, levels + offset, chunk)) throw std::runtime_error(__func__);
        }
    }
    std::vector<std::uint8_t> hostLevels(count);
    if (!GuestCopyIn_nid_no_patch(hostLevels.data(), levels, hostLevels.size())) throw std::runtime_error(__func__);
    std::vector<std::int32_t> hostMap(count, Unwritten);
    ubidi_reorderVisual(hostLevels.data(), length, hostMap.data());
    if (hostMap[0] == Unwritten) return;
    if (!GuestCopyOut_nid_no_patch(indexMap, hostMap.data(), mapBytes)) throw std::runtime_error(__func__);
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
    if (oldFunction && !GuestWritable_nid_no_patch(oldFunction, sizeof(*oldFunction))) throw std::runtime_error(__func__);
    if (oldContext && !GuestWritable_nid_no_patch(oldContext, sizeof(*oldContext))) throw std::runtime_error(__func__);

    std::lock_guard lock(classCallbackMutex);
    auto& callback = classCallbacks[bidi];
    if (!callback) callback = std::make_unique<ClassCallback>();
    const auto previousFunction = callback->function;
    const auto* previousContext = callback->context;
    callback->function = function;
    callback->context = context;
    ubidi_setClassCallback(Native(bidi), function ? ForwardClass : nullptr, callback.get(), nullptr, nullptr, &status);
    if (oldFunction && !GuestCopyOut_nid_no_patch(oldFunction, &previousFunction, sizeof(previousFunction))) {
        throw std::runtime_error(__func__);
    }
    if (oldContext && !GuestCopyOut_nid_no_patch(oldContext, &previousContext, sizeof(previousContext))) {
        throw std::runtime_error(__func__);
    }
    LeaveStatus(error, status, previous, __func__);
}

std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t character) {
    return ubidi_getClass(character);
}

}
