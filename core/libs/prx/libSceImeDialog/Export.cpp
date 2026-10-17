#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

namespace {

constexpr int StatusNone = 0;
constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusUserCanceled = 1;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr std::uint32_t MaxTextLength = 2048;
constexpr int ErrorBusy = static_cast<int>(0x80bc0001u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);
constexpr int ErrorInvalidMaxTextLength = static_cast<int>(0x80bc0016u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);

struct Dialog {
    int status = StatusNone;
    std::uint32_t endStatus = EndStatusOk;
    char16_t* buffer = nullptr;
    std::uint32_t maxLength = 0;
    std::u16string text;
    std::size_t caret = 0;
};

std::mutex g_mutex;
Dialog g_dialog;

bool IsHighSurrogate(char16_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool IsLowSurrogate(char16_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

std::size_t UnitsBefore(const Dialog& dialog) {
    if (dialog.caret == 0) return 0;
    return dialog.caret >= 2 && IsLowSurrogate(dialog.text[dialog.caret - 1]) && IsHighSurrogate(dialog.text[dialog.caret - 2]) ? 2 : 1;
}

std::size_t UnitsAfter(const Dialog& dialog) {
    if (dialog.caret >= dialog.text.size()) return 0;
    return dialog.caret + 1 < dialog.text.size() && IsHighSurrogate(dialog.text[dialog.caret]) && IsLowSurrogate(dialog.text[dialog.caret + 1]) ? 2 : 1;
}

void Finish(std::uint32_t endStatus) {
    g_dialog.endStatus = endStatus;
    g_dialog.status = StatusFinished;
    ImeSetHostTextSink_nid_no_patch(nullptr);
}

void Insert(Dialog& dialog, const char16_t* units, std::size_t length) {
    std::u16string accepted;
    for (std::size_t i = 0; i < length; ++i) {
        if (units[i] != u'\n' && units[i] != u'\r') accepted.push_back(units[i]);
    }
    std::size_t count = std::min<std::size_t>(accepted.size(), dialog.maxLength - dialog.text.size());
    if (count > 0 && count < accepted.size() && IsHighSurrogate(accepted[count - 1])) --count;
    dialog.text.insert(dialog.caret, accepted, 0, count);
    dialog.caret += count;
}

void Receive(ImeHostInputKind kind, const char16_t* units, std::size_t length) {
    std::lock_guard lock(g_mutex);
    Dialog& dialog = g_dialog;
    if (dialog.status != StatusRunning) return;
    switch (kind) {
        case ImeHostInputKind::Text:
            Insert(dialog, units, length);
            return;
        case ImeHostInputKind::Backspace: {
            const std::size_t count = UnitsBefore(dialog);
            dialog.caret -= count;
            dialog.text.erase(dialog.caret, count);
            return;
        }
        case ImeHostInputKind::Delete:
            dialog.text.erase(dialog.caret, UnitsAfter(dialog));
            return;
        case ImeHostInputKind::Left:
            dialog.caret -= UnitsBefore(dialog);
            return;
        case ImeHostInputKind::Right:
            dialog.caret += UnitsAfter(dialog);
            return;
        case ImeHostInputKind::Home:
            dialog.caret = 0;
            return;
        case ImeHostInputKind::End:
            dialog.caret = dialog.text.size();
            return;
        case ImeHostInputKind::Enter:
            std::copy(dialog.text.begin(), dialog.text.end(), dialog.buffer);
            dialog.buffer[dialog.text.size()] = u'\0';
            Finish(EndStatusOk);
            return;
        case ImeHostInputKind::Escape:
            Finish(EndStatusUserCanceled);
            return;
    }
}

}

extern "C" {

int APS5_VABI sceImeDialogAbort(void) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status != StatusRunning) return ErrorNotOpened;
    Finish(EndStatusAborted);
    return 0;
}

int APS5_VABI sceImeDialogGetPanelPositionAndForm(PositionAndForm* form) {
    if (form == nullptr) return ErrorNotOpened;
    *form = PositionAndForm{};
    form->width = 1920;
    form->height = 1080;
    return 0;
}

int APS5_VABI sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
    (void)param;
    if (width != nullptr) *width = 1920;
    if (height != nullptr) *height = 1080;
    return 0;
}

int APS5_VABI sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height) {
    (void)extended;
    return sceImeDialogGetPanelSize(param, width, height);
}

int APS5_VABI sceImeDialogGetResult(Result* result) {
    std::lock_guard lock(g_mutex);
    if (result == nullptr || g_dialog.status != StatusFinished) return ErrorNotOpened;
    *result = Result{};
    result->endstatus = g_dialog.endStatus;
    return 0;
}

int APS5_VABI sceImeDialogGetStatus(void) {
    std::lock_guard lock(g_mutex);
    return g_dialog.status;
}

int APS5_VABI sceImeDialogInit(const Param* param, const ExtendedParam* extended) {
    (void)extended;
    std::lock_guard lock(g_mutex);
    if (g_dialog.status != StatusNone) return ErrorBusy;
    if (param == nullptr) return ErrorInvalidAddress;
    if (param->max_text_length == 0 || param->max_text_length > MaxTextLength) return ErrorInvalidMaxTextLength;
    if (param->input_text_buffer == nullptr) return ErrorInvalidInputTextBuffer;
    g_dialog = Dialog{};
    g_dialog.buffer = param->input_text_buffer;
    g_dialog.maxLength = param->max_text_length;
    std::size_t length = 0;
    while (length < param->max_text_length && param->input_text_buffer[length] != u'\0') ++length;
    g_dialog.text.assign(param->input_text_buffer, length);
    g_dialog.caret = length;
    g_dialog.status = StatusRunning;
    ImeSetHostTextSink_nid_no_patch(&Receive);
    return 0;
}

int APS5_VABI sceImeDialogTerm(void) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status == StatusNone) return ErrorNotOpened;
    ImeSetHostTextSink_nid_no_patch(nullptr);
    g_dialog = Dialog{};
    return 0;
}

}
