#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceTextToSpeech/include/InventoryExports.hpp"
#include "prx/libSceTextToSpeech/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID = static_cast<int>(0x814900FF);
constexpr int SCE_TEXT_TO_SPEECH_ERROR_NOT_INITIALIZED = static_cast<int>(0x81490020);

int checkId(std::uint32_t id) {
    return id > 2 ? SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID : SCE_TEXT_TO_SPEECH_ERROR_NOT_INITIALIZED;
}

int checkPlayingId(std::uint32_t id) {
    return id - 3 < 0xFFFFFFFEu ? SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID : SCE_TEXT_TO_SPEECH_ERROR_NOT_INITIALIZED;
}

}

extern "C" {

int APS5_VABI sceTextToSpeechCancelImpl(std::uint32_t id) {
    return checkId(id);
}

int APS5_VABI sceTextToSpeechCloseImpl(std::uint32_t id) {
    return checkId(id);
}

int APS5_VABI sceTextToSpeechGetStatusImpl(std::uint32_t id, void* status) {
    if (id > 2) return SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID;
    if (status == nullptr) return static_cast<int>(0x81490014);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTextToSpeechInitImpl(std::uint32_t id, const void* parameter) {
    if (id > 2) return SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID;
    if (parameter == nullptr) return static_cast<int>(0x81490010);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTextToSpeechOpenImpl(std::uint32_t id) {
    return checkId(id);
}

int APS5_VABI sceTextToSpeechPauseImpl(std::uint32_t id) {
    return checkPlayingId(id);
}

int APS5_VABI sceTextToSpeechResumeImpl(std::uint32_t id) {
    return checkPlayingId(id);
}

int APS5_VABI sceTextToSpeechSpeakContinuousImpl(int id) {
    return id <= 0 ? SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID : SCE_TEXT_TO_SPEECH_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTextToSpeechSpeakImpl(std::uint32_t id) {
    return checkId(id);
}

int APS5_VABI sceTextToSpeechTermImpl(std::uint32_t id) {
    return checkId(id);
}

}

extern "C" {

int APS5_VABI libSceTextToSpeechUnknown_6ZLFpZLuqEk_nid_no_patch(std::uint32_t id) {
    return checkPlayingId(id);
}
APS5_EXPORT("6ZLFpZLuqEk", libSceTextToSpeechUnknown_6ZLFpZLuqEk_nid_no_patch);

int APS5_VABI libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(std::uint32_t id, void* status) {
    if (id - 3 < 0xFFFFFFFEu) return SCE_TEXT_TO_SPEECH_ERROR_INVALID_ID;
    if (status == nullptr) return static_cast<int>(0x8149001A);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("L-pHC7qtEug", libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch);

}
