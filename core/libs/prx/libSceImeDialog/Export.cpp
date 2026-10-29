#include "prx/libSceImeDialog/include/Api.hpp"
#include "prx/libSceImeDialog/include/PadKeyboard.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"

namespace {

constexpr int StatusNone = 0;
constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusUserCanceled = 1;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr std::uint32_t MaxTextLength = 2048;
constexpr int ErrorBusy = static_cast<int>(0x80bc0001u);
constexpr int ErrorSuspending = static_cast<int>(0x80bc0009u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);
constexpr int ErrorNotInUse = static_cast<int>(0x80bc0107u);
constexpr int ErrorInvalidMaxTextLength = static_cast<int>(0x80bc0016u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);
constexpr int ErrorInternal = static_cast<int>(0x80bc00ffu);
constexpr std::uint32_t OptionMultiline = 0x00000001;
constexpr std::uint32_t ExtendedSetPriority = 0x00000002;
constexpr std::uint32_t ExtendedPriorityShift = 0x00000004;
constexpr std::uint32_t PrioritySymbol = 2;
constexpr std::uint32_t PriorityAccent = 3;
constexpr std::uint32_t DisableController = 0x00000001;

struct Dialog {
    int status = StatusNone;
    std::uint32_t endStatus = EndStatusOk;
    char16_t* buffer = nullptr;
    std::uint32_t maxLength = 0;
    std::u16string text;
    std::size_t caret = 0;
    ImeDialog::PadKeyboardState keyboard;
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

void ReleaseInput() {
    ImeSetHostTextSink_nid_no_patch(nullptr);
    ImeSetHostPadSink_nid_no_patch(nullptr);
    ImeSetKeyboardViewProvider_nid_no_patch(nullptr);
}

void Finish(std::uint32_t endStatus) {
    g_dialog.endStatus = endStatus;
    g_dialog.status = StatusFinished;
    ReleaseInput();
}

std::size_t Insert(Dialog& dialog, const char16_t* units, std::size_t length) {
    std::u16string accepted;
    for (std::size_t i = 0; i < length; ++i) {
        if (units[i] != u'\n' && units[i] != u'\r') accepted.push_back(units[i]);
    }
    std::size_t count = std::min<std::size_t>(accepted.size(), dialog.maxLength - dialog.text.size());
    if (count > 0 && count < accepted.size() && IsHighSurrogate(accepted[count - 1])) --count;
    dialog.text.insert(dialog.caret, accepted, 0, count);
    dialog.caret += count;
    return count;
}

void Apply(Dialog& dialog, ImeHostInputKind kind, const char16_t* units, std::size_t length) {
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

void Receive(ImeHostInputKind kind, const char16_t* units, std::size_t length) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status != StatusRunning) return;
    Apply(g_dialog, kind, units, length);
}

void ReceivePad(const ImeHostPad& pad) {
    std::lock_guard lock(g_mutex);
    Dialog& dialog = g_dialog;
    if (dialog.status != StatusRunning) return;
    const auto result = ImeDialog::PadKeyboardStep(dialog.keyboard, {pad.buttons, pad.leftX, pad.leftY, pad.l2}, ImeDialog::PadKeyboardState::Clock::now());
    switch (result.edit) {
        case ImeDialog::PadEdit::None:
            break;
        case ImeDialog::PadEdit::Insert:
            ImeDialog::PadKeyboardNoteInsert(dialog.keyboard, Insert(dialog, result.text.data(), result.text.size()) != 0);
            break;
        case ImeDialog::PadEdit::Backspace:
            Apply(dialog, ImeHostInputKind::Backspace, nullptr, 0);
            break;
        case ImeDialog::PadEdit::Clear:
            dialog.text.clear();
            dialog.caret = 0;
            break;
        case ImeDialog::PadEdit::CaretLeft:
            Apply(dialog, ImeHostInputKind::Left, nullptr, 0);
            break;
        case ImeDialog::PadEdit::CaretRight:
            Apply(dialog, ImeHostInputKind::Right, nullptr, 0);
            break;
        case ImeDialog::PadEdit::CaretHome:
            Apply(dialog, ImeHostInputKind::Home, nullptr, 0);
            break;
        case ImeDialog::PadEdit::CaretEnd:
            Apply(dialog, ImeHostInputKind::End, nullptr, 0);
            break;
    }
    if (result.finish == ImeDialog::PadFinish::Done) Apply(dialog, ImeHostInputKind::Enter, nullptr, 0);
    else if (result.finish == ImeDialog::PadFinish::Cancel) Apply(dialog, ImeHostInputKind::Escape, nullptr, 0);
}

ImeKeySymbol Symbol(const ImeDialog::PadKeyboardState& keyboard, ImeDialog::PadKeyGlyph glyph) {
    switch (glyph) {
        case ImeDialog::PadKeyGlyph::Shift:
            return keyboard.caseState == ImeDialog::PadKeyboardCase::Upper ? ImeKeySymbol::ShiftFilled : keyboard.caseState == ImeDialog::PadKeyboardCase::CapsLock ? ImeKeySymbol::CapsLock : ImeKeySymbol::ShiftOutline;
        case ImeDialog::PadKeyGlyph::Backspace: return ImeKeySymbol::Backspace;
        case ImeDialog::PadKeyGlyph::ArrowLeft: return ImeKeySymbol::ArrowLeft;
        case ImeDialog::PadKeyGlyph::ArrowRight: return ImeKeySymbol::ArrowRight;
        case ImeDialog::PadKeyGlyph::ArrowUp: return ImeKeySymbol::ArrowUp;
        case ImeDialog::PadKeyGlyph::ArrowDown: return ImeKeySymbol::ArrowDown;
        case ImeDialog::PadKeyGlyph::None: break;
    }
    return ImeKeySymbol::None;
}

bool ProvideView(ImeKeyboardView& view) {
    std::lock_guard lock(g_mutex);
    const Dialog& dialog = g_dialog;
    if (dialog.status != StatusRunning) return false;
    view.text = dialog.text;
    view.caret = dialog.caret;
    view.rows = ImeDialog::PadKeyboardRows(dialog.keyboard);
    view.columns = ImeDialog::PadKeyboardColumns();
    view.keys.clear();
    const ImeDialog::PadKey* selected = ImeDialog::PadKeyboardSelected(dialog.keyboard);
    for (const auto& key : ImeDialog::PadKeyboardKeys(dialog.keyboard)) {
        view.keys.push_back({key.row, key.column, key.columnSpan, key.rowSpan, ImeDialog::PadKeyboardLabel(dialog.keyboard, key), Symbol(dialog.keyboard, key.glyph), key.badge != nullptr ? std::u16string(key.badge) : std::u16string(), &key == selected});
    }
    return true;
}

ImeDialog::PadKeyboardOptions KeyboardOptions(const Param& param, const ExtendedParam* extended) {
    ImeDialog::PadKeyboardOptions options;
    options.controller = extended == nullptr || (extended->disable_device & DisableController) == 0;
    options.multiline = (param.option & OptionMultiline) != 0;
    options.currency = ImeDialog::PadKeyboardCurrencyFor(param.supported_languages);
    options.enterLabel = param.enter_label;
    if (extended != nullptr && (extended->option & ExtendedSetPriority) != 0) {
        options.family = extended->priority == PrioritySymbol ? ImeDialog::PadKeyboardFamily::Symbols : extended->priority == PriorityAccent ? ImeDialog::PadKeyboardFamily::Specials : ImeDialog::PadKeyboardFamily::Latin;
        if ((extended->option & ExtendedPriorityShift) != 0 && options.family != ImeDialog::PadKeyboardFamily::Symbols) options.caseState = ImeDialog::PadKeyboardCase::CapsLock;
    }
    return options;
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
    g_dialog.keyboard = ImeDialog::PadKeyboardOpen(KeyboardOptions(*param, extended));
    g_dialog.status = StatusRunning;
    ImeSetHostTextSink_nid_no_patch(&Receive);
    ImeSetHostPadSink_nid_no_patch(&ReceivePad);
    ImeSetKeyboardViewProvider_nid_no_patch(&ProvideView);
    return 0;
}

int APS5_VABI sceImeDialogTerm(void) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status == StatusNone) return ErrorNotOpened;
    ReleaseInput();
    g_dialog = Dialog{};
    return 0;
}

int APS5_VABI sceImeDialogForTestFunction() {
    return ErrorInternal;
}

int APS5_VABI sceImeDialogForceClose(void) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status == StatusNone) return ErrorNotInUse;
    ReleaseInput();
    g_dialog = Dialog{};
    return 0;
}

int APS5_VABI sceImeDialogGetCurrentStarState(int64_t state) {
    std::lock_guard lock(g_mutex);
    if (g_dialog.status == StatusNone) return ErrorNotInUse;
    if (state == 0) return ErrorInvalidAddress;
    *reinterpret_cast<std::uint32_t*>(state) = 0;
    return 0;
}

int APS5_VABI sceImeDialogInitInternal(const Param* param, const ExtendedParam* extended) {
    return sceImeDialogInit(param, extended);
}

int APS5_VABI sceImeDialogSetPanelPosition(int32_t, int32_t) {
    std::lock_guard lock(g_mutex);
    return g_dialog.status == StatusNone ? ErrorNotInUse : ErrorSuspending;
}

int APS5_VABI sceImeDialogInitInternalWithSubInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
