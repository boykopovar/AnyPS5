#include "prx/libSceIcu/Bidi.hpp"
#include "prx/libc/include/General.hpp"
#include "ubidi_props.h"
#include "unicode/ubidi.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

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

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(error);
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
    ubidi_setPara(Native(bidi), text, length, level, embeddingLevels, Status(error));
}

std::uint8_t APS5_VABI ubidi_getParaLevel_nid_postfix(const Icu::BiDi* bidi) {
    return ubidi_getParaLevel(Native(bidi));
}

std::int32_t APS5_VABI ubidi_countRuns_nid_postfix(Icu::BiDi* bidi, std::int32_t* error) {
    return ubidi_countRuns(Native(bidi), Status(error));
}

std::int32_t APS5_VABI ubidi_getVisualRun_nid_postfix(Icu::BiDi* bidi, std::int32_t run, std::int32_t* logicalStart,
                                                      std::int32_t* length) {
    return ubidi_getVisualRun(Native(bidi), run, logicalStart, length);
}

void APS5_VABI ubidi_getLogicalRun_nid_postfix(const Icu::BiDi* bidi, std::int32_t position, std::int32_t* logicalLimit,
                                               std::uint8_t* level) {
    if (!bidi) APS5_INVALID_ARG_EX;
    ubidi_getLogicalRun(Native(bidi), position, logicalLimit, level);
}

std::int32_t APS5_VABI ubidi_getBaseDirection_nid_postfix(const char16_t* text, std::int32_t length) {
    return ubidi_getBaseDirection(text, length);
}

void APS5_VABI ubidi_reorderVisual_nid_postfix(const std::uint8_t* levels, std::int32_t length, std::int32_t* indexMap) {
    ubidi_reorderVisual(levels, length, indexMap);
}

void APS5_VABI ubidi_setClassCallback_nid_postfix(Icu::BiDi* bidi, Icu::BiDiClassCallback function, const void* context,
                                                  Icu::BiDiClassCallback* oldFunction, const void** oldContext,
                                                  std::int32_t* error) {
    if (!error || U_FAILURE(*Status(error))) return;
    if (!bidi) {
        *error = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    std::lock_guard lock(classCallbackMutex);
    auto& callback = classCallbacks[bidi];
    if (!callback) callback = std::make_unique<ClassCallback>();
    if (oldFunction) *oldFunction = callback->function;
    if (oldContext) *oldContext = callback->context;
    callback->function = function;
    callback->context = context;
    ubidi_setClassCallback(Native(bidi), function ? ForwardClass : nullptr, callback.get(), nullptr, nullptr, Status(error));
}

std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t character) {
    return ubidi_getClass(character);
}

}
