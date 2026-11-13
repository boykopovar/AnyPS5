#pragma once

#include "prx/libSceIme/include/ImeFilterText.hpp"
#include "SceTypes.hpp"

struct ImeDictionaryWord;

struct ImeVshContext;

struct ImePreeditMetaInfo;

struct ImeVshCaretGeometry {
    std::int32_t x;
    std::int32_t y;
    std::uint32_t height;
    std::uint32_t index;
};

struct ImeVshParameter;

struct ImeVshTextGeometry {
    std::int32_t x;
    std::int32_t y;
    std::uint32_t width;
    std::uint32_t height;
};

extern "C" {

int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended);
int APS5_VABI sceImeClose_nid_postfix(void);
int APS5_VABI sceImeGetPanelPositionAndForm(PositionAndForm* form);
int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height);
int APS5_VABI sceImeKeyboardClose(int32_t userId);
int APS5_VABI sceImeKeyboardGetInfo(uint32_t resourceId, KeyboardInfo* info);
int APS5_VABI sceImeKeyboardGetResourceId(int32_t userId, KeyboardResourceIdArray* resourceIds);
int APS5_VABI sceImeKeyboardOpen(int32_t userId, const KeyboardParam* param);
int APS5_VABI sceImeKeyboardSetMode(int32_t userId, uint32_t mode);
int APS5_VABI sceImeFilterText(char16_t* dst, int32_t* outLength, uint32_t capacity, const ime_filter::FilterParam* param, const char16_t* src, uint32_t srcLength);
int APS5_VABI sceImeCheckFilterText(const void* param);
void APS5_VABI sceImeParamInit(Param* param);
int APS5_VABI sceImeSetCaret(const Caret* caret);
int APS5_VABI sceImeSetConsecutiveClauseConversionEnabled(std::uint8_t enabled);
int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length);
int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry);
int APS5_VABI sceImeUpdate(EventHandler handler);
int APS5_VABI sceImeVshOpen(ImeVshContext** context, const ImeVshParameter* parameter, const char16_t* text);
int APS5_VABI sceImeKeyboardUpdate(EventHandler handler);
int APS5_VABI sceImeVshUpdate(ImeVshContext* context);
int APS5_VABI sceImeVshUpdateContext(ImeVshContext* context, const char16_t* text, std::uint32_t caretIndex);
int APS5_VABI sceImeVshClearPreedit(ImeVshContext* context);
int APS5_VABI sceImeVshNotifySetText(ImeVshContext* context, const char16_t* text, std::uint32_t length);
int APS5_VABI sceImeVshSetCaretGeometry(ImeVshContext* context, const ImeVshCaretGeometry* geometry);
int APS5_VABI sceImeVshConfirmPreedit(ImeVshContext* context);
int APS5_VABI sceImeVshSetCaretIndexInPreedit(ImeVshContext* context, std::int32_t index);
int APS5_VABI sceImeVshSetPreeditGeometry(ImeVshContext* context, const ImeVshTextGeometry* geometry);
int APS5_VABI sceImeVshSetSelectGeometry(ImeVshContext* context, const ImeVshTextGeometry* geometry);
int APS5_VABI sceImeVshClose(ImeVshContext* context);
int APS5_VABI sceImeVshGetPanelPositionAndForm(ImeVshContext* context, PositionAndForm* form);
int APS5_VABI sceImeSetCandidateIndex(std::int32_t index);
int APS5_VABI sceImeConfirmCandidate(std::int32_t index);
int APS5_VABI sceImeDisableController(void);
int APS5_VABI sceImeSetVoiceDictationEnabled(bool enabled);
int APS5_VABI sceImeSetEditEventWithRemotePlayEnabled(bool enabled);
int APS5_VABI sceImeForTestFunction(void);
int APS5_VABI sceImeVshInformConfirmdString2(ImeVshContext* context, const char16_t* text, std::int32_t firstParameter, std::int32_t secondParameter, std::int32_t thirdParameter);
int APS5_VABI sceImeVshSetSelectionText(ImeVshContext* context, const char16_t* text, std::uint32_t length);
int APS5_VABI sceImeCheckUpdateTextInfo(ImeEditText* info, std::uint8_t repair);
int APS5_VABI sceImeCheckRemoteEventParam(ImeEvent* event, std::uint8_t repair);
int APS5_VABI sceImeKeyboardOpenInternal(std::int32_t userId, EventHandler handler, const KeyboardParam* param);
int APS5_VABI sceImeVshSetParam(ImeVshContext* context, const ImeVshParameter* parameter);
int APS5_VABI sceImeVshUpdateContext2(ImeVshContext* context, const char16_t* text, std::uint32_t caretIndex);
int APS5_VABI sceImeVshSendTextInfo(ImeVshContext* context, const void* info, std::uint32_t size);
int APS5_VABI sceImeVshInformConfirmdString(ImeVshContext* context, const char16_t* text, std::int32_t parameter);
int APS5_VABI sceImeVshNotifyCurrentTextInfo(ImeVshContext* context, const char16_t* text, std::int32_t caretIndex, std::int32_t preeditFirst, std::int32_t preeditSecond, std::int32_t selectionFirst, std::int32_t selectionSecond);
int APS5_VABI sceImeVshNotifyCurrentTextInfoForReconvertText(ImeVshContext* context, const char16_t* text, std::int32_t caretIndex, std::int32_t preeditFirst, std::int32_t preeditSecond, std::int32_t selectionFirst, std::int32_t selectionSecond, std::int32_t option);
int APS5_VABI sceImeVshNotifyDeleteText(ImeVshContext* context, const char16_t* text, std::int32_t length, std::int32_t parameter);
int APS5_VABI sceImeVshNotifyUpdateCaretText(ImeVshContext* context, const char16_t* text, std::int32_t length, std::int32_t parameter);
int APS5_VABI sceImeVshCheckAutoReConvertingPreeditText(ImeVshContext* context, const char16_t* text);
int APS5_VABI sceImeVshSetPanelPosition(ImeVshContext* context, float x, float y, std::int32_t horizontalAlignment, std::int32_t verticalAlignment);
int APS5_VABI sceImeVshNotifyPreeditMetaInfo(ImeVshContext* context, const ImePreeditMetaInfo* info);
int APS5_VABI sceImeConfigSet(std::uint64_t unusedArgument, std::uint64_t configuration, std::uint32_t key, std::uint32_t value);
int APS5_VABI sceImeConfigGet(std::uint64_t unusedArgument, std::uint64_t configuration, std::uint32_t key);
int APS5_VABI sceImeDicGetWords(std::int32_t userId, std::uint64_t dictionary, ImeDictionaryWord* words);
int APS5_VABI sceImeDicAddWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* word);
int APS5_VABI sceImeDicDeleteWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* word);
int APS5_VABI sceImeDicReplaceWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* oldWord, const ImeDictionaryWord* newWord);
int APS5_VABI sceImeDicDeleteLearnDics(std::int32_t userId);
int APS5_VABI sceImeDicDeleteUserDics(std::int32_t userId);
int APS5_VABI FinalizeImeModule_nid_postfix(void);
int APS5_VABI InitializeImeModule_nid_postfix(void);

}
