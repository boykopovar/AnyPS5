#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

struct SceImeBackendParam {
    std::int32_t mode;
    std::int32_t maxLength;
    std::int32_t type;
    const char16_t* initialString;
    std::int32_t field18;
    std::uint8_t reserved[0xc];
};

static_assert(sizeof(SceImeBackendParam) == 0x28);
static_assert(offsetof(SceImeBackendParam, initialString) == 0x10);
static_assert(offsetof(SceImeBackendParam, field18) == 0x18);
static_assert(offsetof(SceImeBackendParam, reserved) == 0x1c);

void APS5_VABI sceImeBackendParamInit(void* param);
int APS5_VABI sceImeBackendOpen(const SceImeBackendParam* param, void* reserved);
int APS5_VABI sceImeBackendClose(void);
int APS5_VABI sceImeBackendReset(void);
int APS5_VABI sceImeBackendGetStatus(void);
int APS5_VABI sceImeBackendEnterCharacter(std::uint32_t character, char16_t* output);
int APS5_VABI sceImeBackendBackspaceCharacter(void);
int APS5_VABI sceImeBackendPartConfirm(void);
int APS5_VABI sceImeBackendAllConfirm(void);
int APS5_VABI sceImeBackendAllDeleteConvertString(void);
int APS5_VABI sceImeBackendConvertForward(void);
int APS5_VABI sceImeBackendConvertBackward(void);
int APS5_VABI sceImeBackendConvertCancel(void);
int APS5_VABI sceImeBackendGetString(int which, char16_t* output, std::uint16_t capacity);
int APS5_VABI sceImeBackendGetConvertString(char16_t* output);
int APS5_VABI sceImeBackendGetConfirmString(char16_t* output);
int APS5_VABI sceImeBackendGetCaretIndex(void);
int APS5_VABI sceImeBackendModeCaretNext(void);
int APS5_VABI sceImeBackendModeCaretPrevious(void);
int APS5_VABI sceImeBackendGetCandidateList(std::uint16_t* selected, char16_t* output);
int APS5_VABI sceImeBackendGetCandidateListSize(void);
int APS5_VABI sceImeBackendGetCandidateSelect(std::uint16_t* selected);
int APS5_VABI sceImeBackendGetConvertMode(int* mode);
int APS5_VABI sceImeBackendTemporalConfirm(void);
int APS5_VABI sceImeBackendSetConvertMode(int mode);
int APS5_VABI sceImeBackendForTestFunction(void);
int APS5_VABI sceImeBackendDeleteCharacter(void);

}
