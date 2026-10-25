#include "prx/libSceIme/include/Api.hpp"
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
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeFilterText.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

static_assert(sizeof(ImeEditText) == 64);
static_assert(offsetof(ImeEvent, param) == 8);
static_assert(sizeof(ImeVshCaretGeometry) == 16);
static_assert(sizeof(ImeVshTextGeometry) == 16);

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
constexpr int ErrorInternal = static_cast<int>(0x80bc00ffu);
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
    ImeExtKeyboardFilter extKeyboardFilter = nullptr;
    std::u16string text;
    std::uint32_t caret = 0;
    std::deque<ImeEvent> events;
    std::deque<ImeHostInput> hostInputs;
    std::uint64_t id = 0;
    std::uint64_t revision = 0;
};

struct DeferredFilter {
    std::u16string oldText;
};

std::mutex g_keyboardMutex;
std::set<int32_t> g_openKeyboards;
std::mutex g_imeMutex;
std::optional<Session> g_session;
std::atomic<ImeHostTextSink> g_textSink{nullptr};
std::uint64_t g_nextSessionId = 0;

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

struct EditDelta {
    std::uint32_t index = 0;
    std::int32_t length = 0;
};

EditDelta ComputeEditDelta(const std::u16string& before, const std::u16string& after) {
    std::size_t prefix = 0;
    while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix]) ++prefix;
    std::size_t beforeTail = before.size();
    std::size_t afterTail = after.size();
    while (beforeTail > prefix && afterTail > prefix && before[beforeTail - 1] == after[afterTail - 1]) {
        --beforeTail;
        --afterTail;
    }
    return {static_cast<std::uint32_t>(prefix),
            static_cast<std::int32_t>(afterTail - prefix) - static_cast<std::int32_t>(beforeTail - prefix)};
}

void NormalizeCaret(Session& session) {
    if (session.caret > session.text.size()) session.caret = static_cast<std::uint32_t>(session.text.size());
    if (session.caret > 0 && session.caret < session.text.size() &&
        IsHighSurrogate(session.text[session.caret - 1]) && IsLowSurrogate(session.text[session.caret])) {
        ++session.caret;
    }
}

bool RunTextFilter(ImeTextFilter filter, const std::u16string& text, std::uint32_t maxLength, bool multiline,
                   std::u16string* filtered) {
    std::vector<char16_t> buffer(maxLength + 1, u'\0');
    std::uint32_t length = maxLength;
    if (filter(buffer.data(), &length, text.c_str(), static_cast<std::uint32_t>(text.size())) != 0) {
        return false;
    }
    if (length > maxLength || !ValidText(buffer.data(), length, multiline)) {
        return false;
    }
    filtered->assign(buffer.data(), buffer.data() + length);
    return true;
}

void CommitFilteredEdit(Session& session, const std::u16string* filtered, const std::u16string& oldText) {
    if (filtered) {
        session.text = *filtered;
        NormalizeCaret(session);
    }
    if (session.text == oldText) return;
    SyncBuffers(session);
    const EditDelta edit = ComputeEditDelta(oldText, session.text);
    QueueText(session, EventUpdateText, edit.index, edit.length);
}

std::optional<DeferredFilter> CommitEdit(Session& session, const std::u16string& oldText, std::uint32_t plainIndex,
                                         std::int32_t plainDelta) {
    if (session.param.filter) {
        return DeferredFilter{oldText};
    }
    SyncBuffers(session);
    QueueText(session, EventUpdateText, plainIndex, plainDelta);
    return std::nullopt;
}

std::optional<DeferredFilter> Insert(Session& session, std::u16string units) {
    const bool multiline = (session.param.option & OptionMultiline) != 0;
    std::u16string accepted;
    for (const char16_t unit : units) {
        if (!multiline && (unit == u'\n' || unit == u'\r')) continue;
        accepted.push_back(unit);
    }
    const std::size_t room = session.param.max_text_length - session.text.size();
    std::size_t count = std::min(accepted.size(), room);
    if (count > 0 && count < accepted.size() && IsHighSurrogate(accepted[count - 1])) --count;
    if (count == 0) return std::nullopt;
    const uint32_t index = session.caret;
    const std::u16string oldText = session.text;
    session.text.insert(index, accepted, 0, count);
    session.caret += static_cast<uint32_t>(count);
    return CommitEdit(session, oldText, index, static_cast<int32_t>(count));
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

std::optional<DeferredFilter> Apply(Session& session, const ImeHostInput& input) {
    switch (input.kind) {
        case ImeHostInputKind::Text: {
            std::size_t size = 0;
            while (size < sizeof(input.text) && input.text[size] != '\0') ++size;
            return Insert(session, DecodeUtf8(input.text, size));
        }
        case ImeHostInputKind::Backspace: {
            const std::size_t units = UnitsBefore(session);
            if (units == 0) return std::nullopt;
            const std::u16string oldText = session.text;
            session.caret -= static_cast<uint32_t>(units);
            session.text.erase(session.caret, units);
            return CommitEdit(session, oldText, session.caret, -static_cast<int32_t>(units));
        }
        case ImeHostInputKind::Delete: {
            const std::size_t units = UnitsAfter(session);
            if (units == 0) return std::nullopt;
            const std::u16string oldText = session.text;
            session.text.erase(session.caret, units);
            return CommitEdit(session, oldText, session.caret, -static_cast<int32_t>(units));
        }
        case ImeHostInputKind::Left: {
            const std::size_t units = UnitsBefore(session);
            session.caret -= static_cast<uint32_t>(units);
            QueueCaretSteps(session, CaretMoveLeft, units);
            return std::nullopt;
        }
        case ImeHostInputKind::Right: {
            const std::size_t units = UnitsAfter(session);
            session.caret += static_cast<uint32_t>(units);
            QueueCaretSteps(session, CaretMoveRight, units);
            return std::nullopt;
        }
        case ImeHostInputKind::Home: {
            const std::size_t units = session.caret;
            session.caret = 0;
            QueueCaretSteps(session, CaretMoveLeft, units);
            return std::nullopt;
        }
        case ImeHostInputKind::End: {
            const std::size_t units = session.text.size() - session.caret;
            session.caret = static_cast<uint32_t>(session.text.size());
            QueueCaretSteps(session, CaretMoveRight, units);
            return std::nullopt;
        }
        case ImeHostInputKind::Enter:
            if ((session.param.option & OptionMultiline) != 0) {
                return Insert(session, u"\n");
            }
            QueueText(session, EventPressEnter, session.caret, 0);
            return std::nullopt;
        case ImeHostInputKind::Escape:
            QueueText(session, EventPressClose, session.caret, 0);
            return std::nullopt;
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
            g_session->hostInputs.push_back(input);
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
    Session session;
    session.id = ++g_nextSessionId;
    session.param = *param;
    session.extKeyboardFilter = extended ? extended->ext_keyboard_filter : nullptr;
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

int APS5_VABI sceImeFilterText(char16_t* dst, int32_t* out_length, uint32_t capacity,
                               const ime_filter::FilterParam* param, const char16_t* src, uint32_t src_length) {
    return static_cast<int>(ime_filter::FilterText(dst, out_length, capacity, param, src, src_length));
}

int APS5_VABI sceImeCheckFilterText(const void* param) {
    (void)param;
    NotImplemented_nid_no_patch(__func__);
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
    ++g_session->revision;
    return 0;
}

int APS5_VABI sceImeSetConsecutiveClauseConversionEnabled(std::uint8_t enabled) {
    (void)enabled;
    NotImplemented_nid_no_patch(__func__);
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
    ++session.revision;
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
        bool deliver = false;
        bool runFilter = false;
        ImeTextFilter filter = nullptr;
        std::u16string filterText;
        std::u16string filterOldText;
        std::uint32_t filterMaxLength = 0;
        bool filterMultiline = false;
        std::uint64_t sessionId = 0;
        std::uint64_t sessionRevision = 0;
        {
            std::lock_guard lock(g_imeMutex);
            if (!g_session || g_session->param.handler != handler) return 0;
            if (!g_session->hostInputs.empty()) {
                const ImeHostInput input = g_session->hostInputs.front();
                g_session->hostInputs.pop_front();
                if (const std::optional<DeferredFilter> deferred = Apply(*g_session, input)) {
                    runFilter = true;
                    filter = g_session->param.filter;
                    filterText = g_session->text;
                    filterOldText = deferred->oldText;
                    filterMaxLength = g_session->param.max_text_length;
                    filterMultiline = (g_session->param.option & OptionMultiline) != 0;
                    sessionId = g_session->id;
                    sessionRevision = g_session->revision;
                }
            } else if (!g_session->events.empty()) {
                event = g_session->events.front();
                g_session->events.pop_front();
                arg = g_session->param.arg;
                deliver = true;
            } else {
                return 0;
            }
        }
        if (runFilter) {
            std::u16string filtered;
            const bool accepted = RunTextFilter(filter, filterText, filterMaxLength, filterMultiline, &filtered);
            std::lock_guard lock(g_imeMutex);
            if (g_session && g_session->id == sessionId) {
                if (g_session->revision == sessionRevision) {
                    CommitFilteredEdit(*g_session, accepted ? &filtered : nullptr, filterOldText);
                } else if (g_session->text == filterText) {
                    g_session->text = filterOldText;
                    NormalizeCaret(*g_session);
                }
            }
            continue;
        }
        if (!deliver) continue;
        handler(arg, &event);
    }
}

int APS5_VABI sceImeVshOpen(ImeVshContext** context, const ImeVshParameter* parameter, const char16_t* text) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeKeyboardUpdate(EventHandler handler) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshUpdate(ImeVshContext* context) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshUpdateContext(ImeVshContext* context, const char16_t* text, std::uint32_t caretIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshClearPreedit(ImeVshContext* context) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifySetText(ImeVshContext* context, const char16_t* text, std::uint32_t length) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetCaretGeometry(ImeVshContext* context, const ImeVshCaretGeometry* geometry) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshConfirmPreedit(ImeVshContext* context) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetCaretIndexInPreedit(ImeVshContext* context, std::int32_t index) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetPreeditGeometry(ImeVshContext* context, const ImeVshTextGeometry* geometry) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetSelectGeometry(ImeVshContext* context, const ImeVshTextGeometry* geometry) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshClose(ImeVshContext* context) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshGetPanelPositionAndForm(ImeVshContext* context, PositionAndForm* form) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeSetCandidateIndex(std::int32_t index) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeConfirmCandidate(std::int32_t index) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDisableController(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeSetVoiceDictationEnabled(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeSetEditEventWithRemotePlayEnabled(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeForTestFunction(void) {
    return ErrorInternal;
}

int APS5_VABI sceImeVshInformConfirmdString2(ImeVshContext* context, const char16_t* text, std::int32_t firstParameter, std::int32_t secondParameter, std::int32_t thirdParameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetSelectionText(ImeVshContext* context, const char16_t* text, std::uint32_t length) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeCheckUpdateTextInfo(ImeEditText* info, std::uint8_t repair) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeCheckRemoteEventParam(ImeEvent* event, std::uint8_t repair) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeKeyboardOpenInternal(std::int32_t userId, EventHandler handler, const KeyboardParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetParam(ImeVshContext* context, const ImeVshParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshUpdateContext2(ImeVshContext* context, const char16_t* text, std::uint32_t caretIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSendTextInfo(ImeVshContext* context, const void* info, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshInformConfirmdString(ImeVshContext* context, const char16_t* text, std::int32_t parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifyCurrentTextInfo(ImeVshContext* context, const char16_t* text, std::int32_t caretIndex, std::int32_t preeditFirst, std::int32_t preeditSecond, std::int32_t selectionFirst, std::int32_t selectionSecond) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifyCurrentTextInfoForReconvertText(ImeVshContext* context, const char16_t* text, std::int32_t caretIndex, std::int32_t preeditFirst, std::int32_t preeditSecond, std::int32_t selectionFirst, std::int32_t selectionSecond, std::int32_t option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifyDeleteText(ImeVshContext* context, const char16_t* text, std::int32_t length, std::int32_t parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifyUpdateCaretText(ImeVshContext* context, const char16_t* text, std::int32_t length, std::int32_t parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshCheckAutoReConvertingPreeditText(ImeVshContext* context, const char16_t* text) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshSetPanelPosition(ImeVshContext* context, float x, float y, std::int32_t horizontalAlignment, std::int32_t verticalAlignment) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeVshNotifyPreeditMetaInfo(ImeVshContext* context, const ImePreeditMetaInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeConfigSet(std::uint64_t unusedArgument, std::uint64_t configuration, std::uint32_t key, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeConfigGet(std::uint64_t unusedArgument, std::uint64_t configuration, std::uint32_t key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicGetWords(std::int32_t userId, std::uint64_t dictionary, ImeDictionaryWord* words) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicAddWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* word) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicDeleteWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* word) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicReplaceWord(std::int32_t userId, std::uint64_t dictionary, const ImeDictionaryWord* oldWord, const ImeDictionaryWord* newWord) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicDeleteLearnDics(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceImeDicDeleteUserDics(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI FinalizeImeModule_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI InitializeImeModule_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
