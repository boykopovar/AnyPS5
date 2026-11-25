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

UBiDi* Checked(Icu::BiDi* bidi) {
    if (!bidi) APS5_INVALID_ARG_EX;
    return Native(bidi);
}

const UBiDi* Checked(const Icu::BiDi* bidi) {
    if (!bidi) APS5_INVALID_ARG_EX;
    return Native(bidi);
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

void ValidateParagraphOutputs(std::int32_t* start, std::int32_t* limit, std::uint8_t* level, const char* caller) {
    if (start && !GuestWritable_nid_no_patch(start, sizeof(*start))) throw std::runtime_error(caller);
    if (limit && !GuestWritable_nid_no_patch(limit, sizeof(*limit))) throw std::runtime_error(caller);
    if (level && !GuestWritable_nid_no_patch(level, sizeof(*level))) throw std::runtime_error(caller);
}

void CommitParagraph(std::int32_t* start, std::int32_t hostStart, std::int32_t* limit, std::int32_t hostLimit,
                     std::uint8_t* level, std::uint8_t hostLevel, const char* caller) {
    if (start && !GuestCopyOut_nid_no_patch(start, &hostStart, sizeof(hostStart))) throw std::runtime_error(caller);
    if (limit && !GuestCopyOut_nid_no_patch(limit, &hostLimit, sizeof(hostLimit))) throw std::runtime_error(caller);
    if (level && !GuestCopyOut_nid_no_patch(level, &hostLevel, sizeof(hostLevel))) throw std::runtime_error(caller);
}

}

extern "C" {

Icu::BiDi* APS5_VABI ubidi_open_nid_postfix() {
    return reinterpret_cast<Icu::BiDi*>(ubidi_open());
}

Icu::BiDi* APS5_VABI ubidi_openSized_nid_postfix(std::int32_t maxLength, std::int32_t maxRunCount,
                                                 std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return nullptr;
    auto* bidi = ubidi_openSized(maxLength, maxRunCount, &status);
    LeaveStatus(error, status, previous, __func__);
    return reinterpret_cast<Icu::BiDi*>(bidi);
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

std::int32_t APS5_VABI ubidi_getDirection_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getDirection(Checked(bidi));
}

std::int32_t APS5_VABI ubidi_getLength_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getLength(Checked(bidi));
}

std::int32_t APS5_VABI ubidi_getProcessedLength_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getProcessedLength(Checked(bidi));
}

std::int32_t APS5_VABI ubidi_getResultLength_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getResultLength(Checked(bidi));
}

std::uint8_t APS5_VABI ubidi_getLevelAt_nid_postfix(const Icu::BiDi* bidi, std::int32_t index) {
    return ubidi_getLevelAt(Checked(bidi), index);
}

std::int32_t APS5_VABI ubidi_getCustomizedClass_nid_postfix(Icu::BiDi* bidi, std::int32_t character) {
    return ubidi_getCustomizedClass(Checked(bidi), character);
}

std::int32_t APS5_VABI ubidi_getReorderingMode_nid_postfix(Icu::BiDi* bidi) {
    return ubidi_getReorderingMode(Checked(bidi));
}

std::uint32_t APS5_VABI ubidi_getReorderingOptions_nid_postfix(Icu::BiDi* bidi) {
    return ubidi_getReorderingOptions(Checked(bidi));
}

std::int8_t APS5_VABI ubidi_isInverse_nid_postfix(Icu::BiDi* bidi) {
    return ubidi_isInverse(Checked(bidi));
}

std::int8_t APS5_VABI ubidi_isOrderParagraphsLTR_nid_postfix(Icu::BiDi* bidi) {
    return ubidi_isOrderParagraphsLTR(Checked(bidi));
}

std::int32_t APS5_VABI ubidi_countParagraphs_nid_postfix(Icu::BiDi* bidi) {
    return ubidi_countParagraphs(Checked(bidi));
}

void APS5_VABI ubidi_orderParagraphsLTR_nid_postfix(Icu::BiDi* bidi, std::int8_t enabled) {
    ubidi_orderParagraphsLTR(Checked(bidi), enabled);
}

void APS5_VABI ubidi_setInverse_nid_postfix(Icu::BiDi* bidi, std::int8_t enabled) {
    ubidi_setInverse(Checked(bidi), enabled);
}

void APS5_VABI ubidi_setReorderingMode_nid_postfix(Icu::BiDi* bidi, std::int32_t mode) {
    ubidi_setReorderingMode(Checked(bidi), static_cast<UBiDiReorderingMode>(mode));
}

void APS5_VABI ubidi_setReorderingOptions_nid_postfix(Icu::BiDi* bidi, std::uint32_t options) {
    ubidi_setReorderingOptions(Checked(bidi), options);
}

void APS5_VABI ubidi_setContext_nid_postfix(Icu::BiDi* bidi, const char16_t* prologue, std::int32_t prologueLength,
                                            const char16_t* epilogue, std::int32_t epilogueLength,
                                            std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    ubidi_setContext(Native(bidi), prologue, prologueLength, epilogue, epilogueLength, &status);
    LeaveStatus(error, status, previous, __func__);
}

void APS5_VABI ubidi_setLine_nid_postfix(const Icu::BiDi* paragraph, std::int32_t start, std::int32_t limit,
                                         Icu::BiDi* line, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    ubidi_setLine(Native(paragraph), start, limit, Native(line), &status);
    LeaveStatus(error, status, previous, __func__);
}

std::int32_t APS5_VABI ubidi_getParagraph_nid_postfix(const Icu::BiDi* bidi, std::int32_t index,
                                                      std::int32_t* start, std::int32_t* limit, std::uint8_t* level,
                                                      std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return -1;
    ValidateParagraphOutputs(start, limit, level, __func__);
    std::int32_t hostStart = 0;
    std::int32_t hostLimit = 0;
    std::uint8_t hostLevel = 0;
    const auto paragraph = ubidi_getParagraph(Native(bidi), index, &hostStart, &hostLimit, &hostLevel, &status);
    if (U_SUCCESS(status)) CommitParagraph(start, hostStart, limit, hostLimit, level, hostLevel, __func__);
    LeaveStatus(error, status, previous, __func__);
    return paragraph;
}

void APS5_VABI ubidi_getParagraphByIndex_nid_postfix(const Icu::BiDi* bidi, std::int32_t paragraph,
                                                     std::int32_t* start, std::int32_t* limit, std::uint8_t* level,
                                                     std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    ValidateParagraphOutputs(start, limit, level, __func__);
    std::int32_t hostStart = 0;
    std::int32_t hostLimit = 0;
    std::uint8_t hostLevel = 0;
    ubidi_getParagraphByIndex(Native(bidi), paragraph, &hostStart, &hostLimit, &hostLevel, &status);
    if (U_SUCCESS(status)) CommitParagraph(start, hostStart, limit, hostLimit, level, hostLevel, __func__);
    LeaveStatus(error, status, previous, __func__);
}

void APS5_VABI ubidi_getClassCallback_nid_postfix(Icu::BiDi* bidi, Icu::BiDiClassCallback* function,
                                                  const void** context) {
    if (!bidi) APS5_INVALID_ARG_EX;
    if (function && !GuestWritable_nid_no_patch(function, sizeof(*function))) throw std::runtime_error(__func__);
    if (context && !GuestWritable_nid_no_patch(context, sizeof(*context))) throw std::runtime_error(__func__);
    Icu::BiDiClassCallback hostFunction = nullptr;
    const void* hostContext = nullptr;
    {
        std::lock_guard lock(classCallbackMutex);
        const auto entry = classCallbacks.find(bidi);
        if (entry != classCallbacks.end()) {
            hostFunction = entry->second->function;
            hostContext = entry->second->context;
        }
    }
    if (function && !GuestCopyOut_nid_no_patch(function, &hostFunction, sizeof(hostFunction))) {
        throw std::runtime_error(__func__);
    }
    if (context && !GuestCopyOut_nid_no_patch(context, &hostContext, sizeof(hostContext))) {
        throw std::runtime_error(__func__);
    }
}

const char16_t* APS5_VABI ubidi_getText_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getText(Checked(bidi));
}

const std::uint8_t* APS5_VABI ubidi_getLevels_nid_postfix(Icu::BiDi* bidi, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return nullptr;
    const auto* levels = ubidi_getLevels(Native(bidi), &status);
    LeaveStatus(error, status, previous, __func__);
    return levels;
}

std::int32_t APS5_VABI ubidi_getLogicalIndex_nid_postfix(Icu::BiDi* bidi, std::int32_t index, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return -1;
    const auto logical = ubidi_getLogicalIndex(Native(bidi), index, &status);
    LeaveStatus(error, status, previous, __func__);
    return logical;
}

void APS5_VABI ubidi_getLogicalMap_nid_postfix(Icu::BiDi* bidi, std::int32_t* indexMap, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    const auto length = ubidi_getProcessedLength(Native(bidi));
    if (length > 0 && !GuestWritable_nid_no_patch(indexMap, static_cast<std::size_t>(length) * sizeof(*indexMap))) {
        throw std::runtime_error(__func__);
    }
    std::vector<std::int32_t> hostMap(static_cast<std::size_t>(std::max(length, 0)));
    ubidi_getLogicalMap(Native(bidi), hostMap.data(), &status);
    if (U_SUCCESS(status) && length > 0 &&
        !GuestCopyOut_nid_no_patch(indexMap, hostMap.data(), hostMap.size() * sizeof(hostMap[0]))) {
        throw std::runtime_error(__func__);
    }
    LeaveStatus(error, status, previous, __func__);
}

std::int32_t APS5_VABI ubidi_getVisualIndex_nid_postfix(Icu::BiDi* bidi, std::int32_t index, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return -1;
    const auto visual = ubidi_getVisualIndex(Native(bidi), index, &status);
    LeaveStatus(error, status, previous, __func__);
    return visual;
}

void APS5_VABI ubidi_getVisualMap_nid_postfix(Icu::BiDi* bidi, std::int32_t* indexMap, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    const auto length = ubidi_getResultLength(Native(bidi));
    if (length > 0 && !GuestWritable_nid_no_patch(indexMap, static_cast<std::size_t>(length) * sizeof(*indexMap))) {
        throw std::runtime_error(__func__);
    }
    std::vector<std::int32_t> hostMap(static_cast<std::size_t>(std::max(length, 0)));
    ubidi_getVisualMap(Native(bidi), hostMap.data(), &status);
    if (U_SUCCESS(status) && length > 0 &&
        !GuestCopyOut_nid_no_patch(indexMap, hostMap.data(), hostMap.size() * sizeof(hostMap[0]))) {
        throw std::runtime_error(__func__);
    }
    LeaveStatus(error, status, previous, __func__);
}

void APS5_VABI ubidi_invertMap_nid_postfix(const std::int32_t* source, std::int32_t* destination,
                                           std::int32_t length) {
    if (length <= 0) {
        ubidi_invertMap(source, destination, length);
        return;
    }
    const auto count = static_cast<std::size_t>(length);
    std::vector<std::int32_t> hostSource(count);
    if (!GuestCopyIn_nid_no_patch(hostSource.data(), source, count * sizeof(hostSource[0]))) {
        throw std::runtime_error(__func__);
    }
    if (!GuestWritable_nid_no_patch(destination, count * sizeof(*destination))) throw std::runtime_error(__func__);
    std::vector<std::int32_t> hostDestination(count);
    ubidi_invertMap(hostSource.data(), hostDestination.data(), length);
    if (!GuestCopyOut_nid_no_patch(destination, hostDestination.data(), count * sizeof(hostDestination[0]))) {
        throw std::runtime_error(__func__);
    }
}

void APS5_VABI ubidi_reorderLogical_nid_postfix(const std::uint8_t* levels, std::int32_t length,
                                                std::int32_t* indexMap) {
    if (length <= 0) {
        ubidi_reorderLogical(levels, length, indexMap);
        return;
    }
    const auto count = static_cast<std::size_t>(length);
    std::vector<std::uint8_t> hostLevels(count);
    if (!GuestCopyIn_nid_no_patch(hostLevels.data(), levels, count)) throw std::runtime_error(__func__);
    if (!GuestWritable_nid_no_patch(indexMap, count * sizeof(*indexMap))) throw std::runtime_error(__func__);
    std::vector<std::int32_t> hostMap(count, Unwritten);
    ubidi_reorderLogical(hostLevels.data(), length, hostMap.data());
    if (hostMap[0] != Unwritten &&
        !GuestCopyOut_nid_no_patch(indexMap, hostMap.data(), count * sizeof(hostMap[0]))) {
        throw std::runtime_error(__func__);
    }
}

std::int32_t APS5_VABI ubidi_writeReordered_nid_postfix(Icu::BiDi* bidi, char16_t* destination,
                                                        std::int32_t capacity, std::uint16_t options,
                                                        std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return 0;
    if (capacity < 0 || (!destination && capacity > 0)) {
        ubidi_writeReordered(Native(bidi), destination, capacity, options, &status);
        LeaveStatus(error, status, previous, __func__);
        return 0;
    }
    std::vector<char16_t> hostDestination(static_cast<std::size_t>(capacity));
    const auto length = ubidi_writeReordered(Native(bidi), hostDestination.data(), capacity, options, &status);
    const auto written = std::min<std::int32_t>(capacity, length < capacity ? length + 1 : length);
    if (written > 0 &&
        !GuestCopyOut_nid_no_patch(destination, hostDestination.data(), static_cast<std::size_t>(written) * sizeof(char16_t))) {
        throw std::runtime_error(__func__);
    }
    LeaveStatus(error, status, previous, __func__);
    return length;
}

std::int32_t APS5_VABI ubidi_writeReverse_nid_postfix(const char16_t* source, std::int32_t sourceLength,
                                                       char16_t* destination, std::int32_t capacity,
                                                       std::uint16_t options, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return 0;
    if (capacity < 0 || (!destination && capacity > 0)) {
        const auto length = ubidi_writeReverse(source, sourceLength, destination, capacity, options, &status);
        LeaveStatus(error, status, previous, __func__);
        return length;
    }
    std::vector<char16_t> hostDestination(static_cast<std::size_t>(capacity));
    const auto length = ubidi_writeReverse(source, sourceLength, hostDestination.data(), capacity, options, &status);
    const auto written = std::min<std::int32_t>(capacity, length < capacity ? length + 1 : length);
    if (written > 0 &&
        !GuestCopyOut_nid_no_patch(destination, hostDestination.data(), static_cast<std::size_t>(written) * sizeof(char16_t))) {
        throw std::runtime_error(__func__);
    }
    LeaveStatus(error, status, previous, __func__);
    return length;
}

}
