#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

namespace {

constexpr int ErrorBusy = static_cast<int>(0x80bc0001u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);
constexpr int ErrorConnectionFailed = static_cast<int>(0x80bc0004u);
constexpr int ErrorInvalidText = static_cast<int>(0x80bc0006u);
constexpr int ErrorInvalidUserId = static_cast<int>(0x80bc0010u);
constexpr int ErrorInvalidType = static_cast<int>(0x80bc0011u);
constexpr int ErrorInvalidEnterLabel = static_cast<int>(0x80bc0013u);
constexpr int ErrorInvalidInputMethod = static_cast<int>(0x80bc0014u);
constexpr int ErrorInvalidOption = static_cast<int>(0x80bc0015u);
constexpr int ErrorInvalidMaxTextLength = static_cast<int>(0x80bc0016u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int ErrorInvalidPosX = static_cast<int>(0x80bc0018u);
constexpr int ErrorInvalidPosY = static_cast<int>(0x80bc0019u);
constexpr int ErrorInvalidHorizontalAlignment = static_cast<int>(0x80bc001au);
constexpr int ErrorInvalidVerticalAlignment = static_cast<int>(0x80bc001bu);
constexpr int ErrorInvalidExtended = static_cast<int>(0x80bc001cu);
constexpr int ErrorInvalidWork = static_cast<int>(0x80bc0020u);
constexpr int ErrorInvalidHandler = static_cast<int>(0x80bc0022u);
constexpr int ErrorNoResourceId = static_cast<int>(0x80bc0023u);
constexpr int ErrorInvalidMode = static_cast<int>(0x80bc0024u);
constexpr int ErrorInvalidParam = static_cast<int>(0x80bc0030u);
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);
constexpr int ErrorInvalidReserved = static_cast<int>(0x80bc0032u);
constexpr uint32_t ValidKeyboardModes = 0x0000007f;
constexpr uint32_t TypeNumber = 4;
constexpr uint32_t ValidOptions = 0x00007bff;
constexpr uint32_t OptionMultiline = 0x00000001;
constexpr uint32_t OptionUseOver2K = 0x00004000;
constexpr uint32_t EnterLabelLast = 3;
constexpr uint32_t AlignmentLast = 2;
constexpr uint32_t PanelPriorityLast = 3;
constexpr uint32_t MaxTextLength = 2048;
constexpr int32_t UserIdInvalid = -1;

constexpr uint32_t EventOpen = 0;
constexpr uint32_t EventUpdateText = 1;
constexpr uint32_t EventUpdateCaret = 2;
constexpr uint32_t EventPressClose = 4;
constexpr uint32_t EventPressEnter = 5;
constexpr uint32_t CaretMoveLeft = 1;
constexpr uint32_t CaretMoveRight = 2;

struct Session {
    Param param{};
    std::u16string text;
    std::uint32_t caret = 0;
    std::deque<ImeEvent> events;
};

std::mutex g_keyboardMutex;
std::set<int32_t> g_openKeyboards;
std::mutex g_imeMutex;
std::optional<Session> g_session;
std::atomic<ImeHostTextSink> g_textSink{nullptr};

void PanelSize(const Param& param, uint32_t* width, uint32_t* height) {
    const uint32_t scale = (param.option & OptionUseOver2K) != 0 ? 2 : 1;
    *width = (param.type == TypeNumber ? 370u : 793u) * scale;
    *height = (param.type == TypeNumber ? 402u : 408u) * scale;
}

bool IsHighSurrogate(char16_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool IsLowSurrogate(char16_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

bool ValidText(const char16_t* text, std::size_t length, bool multiline) {
    for (std::size_t i = 0; i < length; ++i) {
        const char16_t unit = text[i];
        if (!multiline && (unit == u'\n' || unit == u'\r')) return false;
        if (IsLowSurrogate(unit)) return false;
        if (IsHighSurrogate(unit)) {
            if (i + 1 >= length || !IsLowSurrogate(text[i + 1])) return false;
            ++i;
        }
    }
    return true;
}

std::size_t TextLength(const char16_t* text, std::size_t limit) {
    std::size_t length = 0;
    while (length < limit && text[length] != u'\0') ++length;
    return length;
}

std::u16string DecodeUtf8(const char* text, std::size_t size) {
    std::u16string units;
    for (std::size_t i = 0; i < size;) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::uint32_t codePoint = 0;
        std::size_t extra = 0;
        if (lead < 0x80) codePoint = lead;
        else if ((lead & 0xe0) == 0xc0) { codePoint = lead & 0x1f; extra = 1; }
        else if ((lead & 0xf0) == 0xe0) { codePoint = lead & 0x0f; extra = 2; }
        else if ((lead & 0xf8) == 0xf0) { codePoint = lead & 0x07; extra = 3; }
        else throw std::invalid_argument("IME host input: invalid UTF-8 lead byte");
        if (extra != 0 && i + extra >= size) throw std::invalid_argument("IME host input: truncated UTF-8 sequence");
        for (std::size_t k = 1; k <= extra; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("IME host input: invalid UTF-8 continuation byte");
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        i += extra + 1;
        if (codePoint >= 0x10000) {
            codePoint -= 0x10000;
            units.push_back(static_cast<char16_t>(0xd800 + (codePoint >> 10)));
            units.push_back(static_cast<char16_t>(0xdc00 + (codePoint & 0x3ff)));
        } else {
            units.push_back(static_cast<char16_t>(codePoint));
        }
    }
    return units;
}

void SyncBuffers(Session& session) {
    const std::size_t bytes = session.text.size() * sizeof(char16_t);
    std::memcpy(session.param.input_text_buffer, session.text.data(), bytes);
    session.param.input_text_buffer[session.text.size()] = u'\0';
    auto* work = static_cast<char16_t*>(session.param.work);
    std::memcpy(work, session.text.data(), bytes);
    work[session.text.size()] = u'\0';
}

void QueueText(Session& session, uint32_t id, uint32_t index, int32_t delta) {
    ImeEvent event{};
    event.id = id;
    event.param.text.str = static_cast<char16_t*>(session.param.work);
    event.param.text.caret_index = session.caret;
    event.param.text.area_num = 1;
    event.param.text.text_area[0].mode = TextAreaMode::Edit;
    event.param.text.text_area[0].index = index;
    event.param.text.text_area[0].length = delta;
    session.events.push_back(event);
}

void QueueCaretSteps(Session& session, uint32_t direction, std::size_t steps) {
    for (std::size_t i = 0; i < steps; ++i) {
        ImeEvent event{};
        event.id = EventUpdateCaret;
        event.param.caret_move = direction;
        session.events.push_back(event);
    }
}

void Insert(Session& session, std::u16string units) {
    const bool multiline = (session.param.option & OptionMultiline) != 0;
    std::u16string accepted;
    for (const char16_t unit : units) {
        if (!multiline && (unit == u'\n' || unit == u'\r')) continue;
        accepted.push_back(unit);
    }
    const std::size_t room = session.param.max_text_length - session.text.size();
    std::size_t count = std::min(accepted.size(), room);
    if (count > 0 && count < accepted.size() && IsHighSurrogate(accepted[count - 1])) --count;
    if (count == 0) return;
    const uint32_t index = session.caret;
    session.text.insert(index, accepted, 0, count);
    session.caret += static_cast<uint32_t>(count);
    SyncBuffers(session);
    QueueText(session, EventUpdateText, index, static_cast<int32_t>(count));
}

std::size_t UnitsBefore(const Session& session) {
    const uint32_t caret = session.caret;
    if (caret == 0) return 0;
    return caret >= 2 && IsLowSurrogate(session.text[caret - 1]) && IsHighSurrogate(session.text[caret - 2]) ? 2 : 1;
}

std::size_t UnitsAfter(const Session& session) {
    const uint32_t caret = session.caret;
    if (caret >= session.text.size()) return 0;
    return caret + 1 < session.text.size() && IsHighSurrogate(session.text[caret]) && IsLowSurrogate(session.text[caret + 1]) ? 2 : 1;
}

void Apply(Session& session, const ImeHostInput& input) {
    switch (input.kind) {
        case ImeHostInputKind::Text: {
            std::size_t size = 0;
            while (size < sizeof(input.text) && input.text[size] != '\0') ++size;
            Insert(session, DecodeUtf8(input.text, size));
            return;
        }
        case ImeHostInputKind::Backspace: {
            const std::size_t units = UnitsBefore(session);
            if (units == 0) return;
            session.caret -= static_cast<uint32_t>(units);
            session.text.erase(session.caret, units);
            SyncBuffers(session);
            QueueText(session, EventUpdateText, session.caret, -static_cast<int32_t>(units));
            return;
        }
        case ImeHostInputKind::Delete: {
            const std::size_t units = UnitsAfter(session);
            if (units == 0) return;
            session.text.erase(session.caret, units);
            SyncBuffers(session);
            QueueText(session, EventUpdateText, session.caret, -static_cast<int32_t>(units));
            return;
        }
        case ImeHostInputKind::Left: {
            const std::size_t units = UnitsBefore(session);
            session.caret -= static_cast<uint32_t>(units);
            QueueCaretSteps(session, CaretMoveLeft, units);
            return;
        }
        case ImeHostInputKind::Right: {
            const std::size_t units = UnitsAfter(session);
            session.caret += static_cast<uint32_t>(units);
            QueueCaretSteps(session, CaretMoveRight, units);
            return;
        }
        case ImeHostInputKind::Home: {
            const std::size_t units = session.caret;
            session.caret = 0;
            QueueCaretSteps(session, CaretMoveLeft, units);
            return;
        }
        case ImeHostInputKind::End: {
            const std::size_t units = session.text.size() - session.caret;
            session.caret = static_cast<uint32_t>(session.text.size());
            QueueCaretSteps(session, CaretMoveRight, units);
            return;
        }
        case ImeHostInputKind::Enter:
            if ((session.param.option & OptionMultiline) != 0) {
                Insert(session, u"\n");
            } else {
                QueueText(session, EventPressEnter, session.caret, 0);
            }
            return;
        case ImeHostInputKind::Escape:
            QueueText(session, EventPressClose, session.caret, 0);
            return;
    }
    throw std::invalid_argument("IME host input: unknown input kind");
}

int ValidateOpen(const Param* param, const ExtendedParam* extended) {
    if (!param) return ErrorInvalidAddress;
    if (param->user_id == UserIdInvalid) return ErrorInvalidUserId;
    if (param->type > TypeNumber) return ErrorInvalidType;
    if (param->enter_label > EnterLabelLast) return ErrorInvalidEnterLabel;
    if (param->input_method != 0) return ErrorInvalidInputMethod;
    if ((param->option & ~ValidOptions) != 0) return ErrorInvalidOption;
    if (param->max_text_length == 0 || param->max_text_length > MaxTextLength) return ErrorInvalidMaxTextLength;
    if (!param->input_text_buffer) return ErrorInvalidInputTextBuffer;
    const float maxX = (param->option & OptionUseOver2K) != 0 ? 3840.0f : 1920.0f;
    const float maxY = (param->option & OptionUseOver2K) != 0 ? 2160.0f : 1080.0f;
    if (!(param->posx >= 0.0f && param->posx < maxX)) return ErrorInvalidPosX;
    if (!(param->posy >= 0.0f && param->posy < maxY)) return ErrorInvalidPosY;
    if (param->horizontal_alignment > AlignmentLast) return ErrorInvalidHorizontalAlignment;
    if (param->vertical_alignment > AlignmentLast) return ErrorInvalidVerticalAlignment;
    if (extended) {
        if (extended->priority > PanelPriorityLast) return ErrorInvalidExtended;
        for (const std::int8_t byte : extended->reserved) {
            if (byte != 0) return ErrorInvalidExtended;
        }
    }
    if (!param->work || (reinterpret_cast<std::uintptr_t>(param->work) & 3u) != 0) return ErrorInvalidWork;
    if (!param->handler) return ErrorInvalidHandler;
    for (const std::int8_t byte : param->reserved) {
        if (byte != 0) return ErrorInvalidReserved;
    }
    const std::size_t length = TextLength(param->input_text_buffer, param->max_text_length);
    if (!ValidText(param->input_text_buffer, length, (param->option & OptionMultiline) != 0)) return ErrorInvalidText;
    return 0;
}

}

extern "C" {

void ImePublishInput_nid_postfix(const ImeHostInput& input) {
    {
        std::lock_guard lock(g_imeMutex);
        if (g_session) {
            Apply(*g_session, input);
            return;
        }
    }
    const ImeHostTextSink sink = g_textSink.load();
    if (sink == nullptr) return;
    std::u16string units;
    if (input.kind == ImeHostInputKind::Text) {
        std::size_t size = 0;
        while (size < sizeof(input.text) && input.text[size] != '\0') ++size;
        units = DecodeUtf8(input.text, size);
    }
    sink(input.kind, units.data(), units.size());
}

bool ImeCapturesKeyboard_nid_postfix() {
    std::lock_guard lock(g_imeMutex);
    return g_session.has_value() || g_textSink.load() != nullptr;
}

void ImeSetHostTextSink_nid_no_patch(ImeHostTextSink sink) {
    g_textSink = sink;
}

int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended) {
    std::lock_guard lock(g_imeMutex);
    if (g_session) return ErrorBusy;
    if (const int error = ValidateOpen(param, extended)) return error;
    if (param->filter) NotImplemented_nid_no_patch("sceImeOpen: text filter");
    if (extended && extended->ext_keyboard_filter) NotImplemented_nid_no_patch("sceImeOpen: external keyboard filter");
    Session session;
    session.param = *param;
    session.text.assign(param->input_text_buffer, TextLength(param->input_text_buffer, param->max_text_length));
    session.caret = static_cast<uint32_t>(session.text.size());
    SyncBuffers(session);
    ImeEvent open{};
    open.id = EventOpen;
    open.param.rect.x = param->posx;
    open.param.rect.y = param->posy;
    PanelSize(*param, &open.param.rect.width, &open.param.rect.height);
    session.events.push_back(open);
    g_session = std::move(session);
    return 0;
}

int APS5_VABI sceImeClose_nid_postfix(void) {
    std::lock_guard lock(g_imeMutex);
    if (!g_session) return ErrorNotOpened;
    g_session.reset();
    return 0;
}

int APS5_VABI sceImeGetPanelPositionAndForm(PositionAndForm* form) {
    std::lock_guard lock(g_imeMutex);
    if (!g_session) return ErrorNotOpened;
    if (!form) return ErrorInvalidAddress;
    const Param& param = g_session->param;
    *form = {};
    form->type = param.type;
    form->posx = param.posx;
    form->posy = param.posy;
    form->horizontal_alignment = param.horizontal_alignment;
    form->vertical_alignment = param.vertical_alignment;
    PanelSize(param, &form->width, &form->height);
    return 0;
}

int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 if (!param || !width || !height) return ErrorInvalidAddress;
 if (param->type > TypeNumber) return ErrorInvalidType;
 if ((param->option & ~ValidOptions) != 0) return ErrorInvalidOption;
 PanelSize(*param, width, height);
 return 0;
}

int APS5_VABI sceImeKeyboardClose(int32_t user_id) {
 std::lock_guard lock(g_keyboardMutex);
 if (g_openKeyboards.erase(user_id) == 0) return ErrorNotOpened;
 return 0;
}

int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
 (void)resource_id;
 if (!info) return ErrorInvalidAddress;
 std::lock_guard lock(g_keyboardMutex);
 return g_openKeyboards.empty() ? ErrorNotOpened : ErrorNoResourceId;
}

int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
 if (!resource_ids) return ErrorInvalidAddress;
 if (user_id == UserIdInvalid) return ErrorInvalidUserId;
 *resource_ids = {};
 resource_ids->user_id = user_id;
 std::lock_guard lock(g_keyboardMutex);
 return g_openKeyboards.contains(user_id) ? ErrorConnectionFailed : ErrorNotOpened;
}

int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
 if (!param) APS5_INVALID_ARG_EX;
 std::lock_guard lock(g_keyboardMutex);
 if (!g_openKeyboards.insert(user_id).second) return ErrorBusy;
 return 0;
}

int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
 if (user_id == UserIdInvalid) return ErrorInvalidUserId;
 std::lock_guard lock(g_keyboardMutex);
 if (!g_openKeyboards.contains(user_id)) return ErrorNotOpened;
 return (mode & ~ValidKeyboardModes) == 0 ? 0 : ErrorInvalidMode;
}

void APS5_VABI sceImeParamInit(Param* param) {
 if (!param) return;
 std::memset(param, 0, sizeof(*param));
 param->user_id = -1;
}

int APS5_VABI sceImeSetCaret(const Caret* caret) {
    std::lock_guard lock(g_imeMutex);
    if (!g_session) return ErrorNotOpened;
    if (!caret) return ErrorInvalidAddress;
    if (caret->index > g_session->text.size()) return ErrorInvalidParam;
    g_session->caret = caret->index;
    return 0;
}

int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length) {
    std::lock_guard lock(g_imeMutex);
    if (!g_session) return ErrorNotOpened;
    if (!text) return ErrorInvalidAddress;
    Session& session = *g_session;
    const std::size_t count = TextLength(text, std::min(length, session.param.max_text_length));
    if (!ValidText(text, count, (session.param.option & OptionMultiline) != 0)) return ErrorInvalidText;
    session.text.assign(text, count);
    if (session.caret > session.text.size()) session.caret = static_cast<uint32_t>(session.text.size());
    SyncBuffers(session);
    return 0;
}

int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
    (void)mode;
    std::lock_guard lock(g_imeMutex);
    if (!g_session) return ErrorNotOpened;
    if (!geometry) return ErrorInvalidAddress;
    return 0;
}

int APS5_VABI sceImeUpdate(EventHandler handler) {
    if (!handler) APS5_INVALID_ARG_EX;
    for (;;) {
        ImeEvent event{};
        void* arg = nullptr;
        {
            std::lock_guard lock(g_imeMutex);
            if (!g_session || g_session->param.handler != handler || g_session->events.empty()) return 0;
            event = g_session->events.front();
            g_session->events.pop_front();
            arg = g_session->param.arg;
        }
        handler(arg, &event);
    }
}

}
