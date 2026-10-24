#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceVideoOut/include/ImeTextInput.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended);
int APS5_VABI sceImeClose_nid_postfix(void);
int APS5_VABI sceImeUpdate(EventHandler handler);
int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length);
int APS5_VABI sceImeSetCaret(const Caret* caret);
int APS5_VABI sceImeGetPanelPositionAndForm(PositionAndForm* form);
void APS5_VABI sceImeParamInit(Param* param);
}

namespace {

constexpr int Busy = static_cast<int>(0x80bc0001u);
constexpr int NotOpened = static_cast<int>(0x80bc0002u);
constexpr int InvalidText = static_cast<int>(0x80bc0006u);
constexpr int InvalidUserId = static_cast<int>(0x80bc0010u);
constexpr int InvalidType = static_cast<int>(0x80bc0011u);
constexpr int InvalidEnterLabel = static_cast<int>(0x80bc0013u);
constexpr int InvalidInputMethod = static_cast<int>(0x80bc0014u);
constexpr int InvalidOption = static_cast<int>(0x80bc0015u);
constexpr int InvalidMaxTextLength = static_cast<int>(0x80bc0016u);
constexpr int InvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int InvalidPosX = static_cast<int>(0x80bc0018u);
constexpr int InvalidPosY = static_cast<int>(0x80bc0019u);
constexpr int InvalidHorizontalAlignment = static_cast<int>(0x80bc001au);
constexpr int InvalidVerticalAlignment = static_cast<int>(0x80bc001bu);
constexpr int InvalidExtended = static_cast<int>(0x80bc001cu);
constexpr int InvalidWork = static_cast<int>(0x80bc0020u);
constexpr int InvalidHandler = static_cast<int>(0x80bc0022u);
constexpr int InvalidParam = static_cast<int>(0x80bc0030u);
constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
constexpr int InvalidReserved = static_cast<int>(0x80bc0032u);

constexpr uint32_t EventOpen = 0;
constexpr uint32_t EventUpdateText = 1;
constexpr uint32_t EventUpdateCaret = 2;
constexpr uint32_t EventPressClose = 4;
constexpr uint32_t EventPressEnter = 5;

struct Recorded {
    uint32_t id;
    void* arg;
    ImeEventParam param;
    std::u16string text;
};

std::vector<Recorded> g_events;
bool g_closeOnPressClose = false;
int g_reentrantClose = 1;

void APS5_VABI Record(void* arg, const ImeEvent* event) {
    Recorded recorded{event->id, arg, event->param, {}};
    if (event->id == EventUpdateText || event->id == EventPressEnter || event->id == EventPressClose) {
        recorded.text = event->param.text.str;
    }
    g_events.push_back(recorded);
    if (g_closeOnPressClose && event->id == EventPressClose) g_reentrantClose = sceImeClose_nid_postfix();
}

std::int32_t APS5_VABI TextFilter(char16_t*, std::uint32_t*, const char16_t*, std::uint32_t) {
    std::abort();
}

void APS5_VABI Other(void*, const ImeEvent*) {
    std::abort();
}

int32_t APS5_VABI UpperFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        const char16_t unit = sourceText[i];
        outText[i] = unit >= u'a' && unit <= u'z' ? static_cast<char16_t>(unit - u'a' + u'A') : unit;
    }
    *outTextLength = sourceTextLength;
    return 0;
}

int32_t APS5_VABI FailingFilter(char16_t*, uint32_t*, const char16_t*, uint32_t) {
    return 1;
}

int32_t APS5_VABI StripXFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    uint32_t length = 0;
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        if (sourceText[i] != u'x') outText[length++] = sourceText[i];
    }
    *outTextLength = length;
    return 0;
}

int32_t APS5_VABI InvalidOutputFilter(char16_t* outText, uint32_t* outTextLength, const char16_t*, uint32_t) {
    outText[0] = 0xdc00;
    *outTextLength = 1;
    return 0;
}

int32_t APS5_VABI TooLongFilter(char16_t*, uint32_t* outTextLength, const char16_t*, uint32_t) {
    *outTextLength = 9;
    return 0;
}

int32_t APS5_VABI ShortenFilter(char16_t* outText, uint32_t* outTextLength, const char16_t*, uint32_t) {
    outText[0] = u'a';
    *outTextLength = 1;
    return 0;
}

int32_t APS5_VABI PairAtCaretFilter(char16_t* outText, uint32_t* outTextLength, const char16_t*, uint32_t) {
    outText[0] = u'a';
    outText[1] = u'b';
    outText[2] = 0xd83d;
    outText[3] = 0xde00;
    *outTextLength = 4;
    return 0;
}

int APS5_VABI ExtNoop(const ImeKeycode*, std::uint16_t*, std::uint32_t*, void*) {
    return 0;
}

bool g_sawCaptureInsideFilter = false;
int g_setInsideRc = -1;

int32_t APS5_VABI LockingFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    g_sawCaptureInsideFilter = ImeCapturesKeyboard_nid_postfix();
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        outText[i] = sourceText[i];
    }
    *outTextLength = sourceTextLength;
    return 0;
}

int32_t APS5_VABI SetInsideFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    g_setInsideRc = sceImeSetText(u"zz", 2);
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        outText[i] = sourceText[i];
    }
    *outTextLength = sourceTextLength;
    return 0;
}

bool g_caretInsideFirstCall = true;
int g_caretInsideRc = -1;

int32_t APS5_VABI SetCaretInsideFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    if (g_caretInsideFirstCall) {
        g_caretInsideFirstCall = false;
        const Caret caret{0.0f, 0.0f, 0, 0};
        g_caretInsideRc = sceImeSetCaret(&caret);
    }
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        outText[i] = sourceText[i];
    }
    *outTextLength = sourceTextLength;
    return 0;
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

struct Buffers {
    char16_t text[2049] = {};
    alignas(16) unsigned char work[0x5000] = {};
    int arg = 0;
};

Param MakeParam(Buffers& buffers, uint32_t maxLength) {
    Param param;
    sceImeParamInit(&param);
    param.user_id = 1;
    param.max_text_length = maxLength;
    param.input_text_buffer = buffers.text;
    param.posx = 100.0f;
    param.posy = 200.0f;
    param.work = buffers.work;
    param.arg = &buffers.arg;
    param.handler = Record;
    return param;
}

std::vector<Recorded> Drain() {
    g_events.clear();
    Require(sceImeUpdate(Record) == 0, "update failed");
    return g_events;
}

void Publish(ImeHostInputKind kind, const char* text = "") {
    ImeHostInput input{};
    input.kind = kind;
    std::strncpy(input.text, text, sizeof(input.text) - 1);
    ImePublishInput_nid_postfix(input);
}

void RequireText(const Recorded& event, uint32_t id, const std::u16string& text, uint32_t caret, uint32_t index, int32_t delta) {
    Require(event.id == id, "event id");
    Require(event.text == text, "event text");
    Require(event.param.text.caret_index == caret, "event caret");
    Require(event.param.text.area_num == 1, "event area count");
    Require(event.param.text.text_area[0].mode == TextAreaMode::Edit, "event area mode");
    Require(event.param.text.text_area[0].index == index, "event edit index");
    Require(event.param.text.text_area[0].length == delta, "event edit length");
}

void RequireCaretSteps(const std::vector<Recorded>& events, uint32_t direction, std::size_t count) {
    Require(events.size() == count, "caret step count");
    for (const auto& event : events) {
        Require(event.id == EventUpdateCaret && event.param.caret_move == direction, "caret step");
    }
}

void CheckValidation() {
    Buffers buffers;
    Require(sceImeOpen_nid_postfix(nullptr, nullptr) == InvalidAddress, "null param");
    auto expect = [&](auto change, int error, const char* message) {
        Param param = MakeParam(buffers, 16);
        change(param);
        Require(sceImeOpen_nid_postfix(&param, nullptr) == error, message);
    };
    expect([](Param& p) { p.user_id = -1; }, InvalidUserId, "invalid user");
    expect([](Param& p) { p.type = 5; }, InvalidType, "invalid type");
    expect([](Param& p) { p.enter_label = 4; }, InvalidEnterLabel, "invalid enter label");
    expect([](Param& p) { p.input_method = 1; }, InvalidInputMethod, "invalid input method");
    expect([](Param& p) { p.option = 0x80000000u; }, InvalidOption, "invalid option");
    expect([](Param& p) { p.max_text_length = 0; }, InvalidMaxTextLength, "zero length");
    expect([](Param& p) { p.max_text_length = 2049; }, InvalidMaxTextLength, "long length");
    expect([](Param& p) { p.input_text_buffer = nullptr; }, InvalidInputTextBuffer, "null buffer");
    expect([](Param& p) { p.posx = -1.0f; }, InvalidPosX, "negative x");
    expect([](Param& p) { p.posx = 1920.0f; }, InvalidPosX, "x past 1080p");
    expect([](Param& p) { p.posy = 1080.0f; }, InvalidPosY, "y past 1080p");
    expect([](Param& p) { p.horizontal_alignment = 3; }, InvalidHorizontalAlignment, "horizontal alignment");
    expect([](Param& p) { p.vertical_alignment = 3; }, InvalidVerticalAlignment, "vertical alignment");
    expect([](Param& p) { p.work = nullptr; }, InvalidWork, "null work");
    expect([&](Param& p) { p.work = buffers.work + 2; }, InvalidWork, "misaligned work");
    expect([](Param& p) { p.handler = nullptr; }, InvalidHandler, "null handler");
    expect([](Param& p) { p.reserved[7] = 1; }, InvalidReserved, "reserved bytes");
    buffers.text[0] = 0xdc00;
    expect([](Param&) {}, InvalidText, "lone low surrogate");
    buffers.text[0] = u'\n';
    expect([](Param&) {}, InvalidText, "newline without multiline");
    buffers.text[0] = 0;

    Param param = MakeParam(buffers, 16);
    ExtendedParam extended{};
    extended.priority = 4;
    Require(sceImeOpen_nid_postfix(&param, &extended) == InvalidExtended, "extended priority");
    extended.priority = 0;
    extended.reserved[59] = 1;
    Require(sceImeOpen_nid_postfix(&param, &extended) == InvalidExtended, "extended reserved");

    param.posx = 3000.0f;
    param.option = 0x4000;
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "over-2K coordinates");
    Require(sceImeClose_nid_postfix() == 0, "close after over-2K open");

    param = MakeParam(buffers, 16);
    param.filter = reinterpret_cast<ImeTextFilter>(&UpperFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a text filter");
    Require(ImeCapturesKeyboard_nid_postfix(), "text filter session not open");
    Require(sceImeClose_nid_postfix() == 0, "close the text filter session");

    param = MakeParam(buffers, 16);
    ExtendedParam extFilters{};
    extFilters.ext_keyboard_filter = reinterpret_cast<ImeExtKeyboardFilter>(&ExtNoop);
    Require(sceImeOpen_nid_postfix(&param, &extFilters) == 0, "open with an external keyboard filter");
    Require(ImeCapturesKeyboard_nid_postfix(), "external keyboard filter session not open");
    Require(sceImeClose_nid_postfix() == 0, "close the external keyboard filter session");
}

void CheckEditing() {
    Buffers buffers;
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    Param param = MakeParam(buffers, 8);
    Require(!ImeCapturesKeyboard_nid_postfix(), "keyboard captured before open");
    Require(sceImeUpdate(Record) == 0 && g_events.empty(), "update before open");
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open");
    Require(sceImeOpen_nid_postfix(&param, nullptr) == Busy, "second open");
    Require(ImeCapturesKeyboard_nid_postfix(), "keyboard not captured");
    Require(sceImeUpdate(Other) == 0, "update with another handler");

    auto events = Drain();
    Require(events.size() == 1 && events[0].id == EventOpen, "open event");
    Require(events[0].arg == &buffers.arg, "handler argument");
    Require(events[0].param.rect.x == 100.0f && events[0].param.rect.y == 200.0f, "open position");
    Require(events[0].param.rect.width == 793 && events[0].param.rect.height == 408, "open size");
    Require(Drain().empty(), "events delivered twice");

    PositionAndForm form{};
    Require(sceImeGetPanelPositionAndForm(nullptr) == InvalidAddress, "null panel form");
    Require(sceImeGetPanelPositionAndForm(&form) == 0, "panel form");
    Require(form.posx == 100.0f && form.posy == 200.0f && form.width == 793 && form.height == 408, "panel form values");

    Publish(ImeHostInputKind::Text, "c");
    events = Drain();
    Require(events.size() == 1, "one text event");
    RequireText(events[0], EventUpdateText, u"abc", 3, 2, 1);
    Require(std::u16string(buffers.text) == u"abc", "input buffer");
    Require(std::u16string(reinterpret_cast<const char16_t*>(buffers.work)) == u"abc", "work copy");

    Publish(ImeHostInputKind::Text, "\xc3\xa9\xf0\x9f\x98\x80");
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abcé\U0001F600", 6, 3, 3);

    Publish(ImeHostInputKind::Left);
    RequireCaretSteps(Drain(), 1, 2);
    Publish(ImeHostInputKind::Backspace);
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abc\U0001F600", 3, 3, -1);
    Publish(ImeHostInputKind::Delete);
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abc", 3, 3, -2);
    Publish(ImeHostInputKind::Delete);
    Require(Drain().empty(), "delete at the end");
    Publish(ImeHostInputKind::Home);
    RequireCaretSteps(Drain(), 1, 3);
    Publish(ImeHostInputKind::Left);
    Require(Drain().empty(), "left at the start");
    Publish(ImeHostInputKind::Backspace);
    Require(Drain().empty(), "backspace at the start");
    Publish(ImeHostInputKind::Right);
    RequireCaretSteps(Drain(), 2, 1);
    Publish(ImeHostInputKind::End);
    RequireCaretSteps(Drain(), 2, 2);

    Publish(ImeHostInputKind::Text, "x\ny");
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abcxy", 5, 3, 2);
    Publish(ImeHostInputKind::Text, "12\xf0\x9f\x98\x80");
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abcxy12", 7, 5, 2);
    Publish(ImeHostInputKind::Text, "\xf0\x9f\x98\x80");
    Require(Drain().empty(), "a pair past the limit must not be split");
    Publish(ImeHostInputKind::Text, "z");
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"abcxy12z", 8, 7, 1);
    Publish(ImeHostInputKind::Text, "q");
    Require(Drain().empty(), "text past the limit");

    Require(sceImeSetText(nullptr, 1) == InvalidAddress, "null text");
    Require(sceImeSetText(u"a\nb", 3) == InvalidText, "newline in single-line text");
    Require(sceImeSetText(u"hello", 5) == 0, "set text");
    Require(std::u16string(buffers.text) == u"hello", "set text buffer");
    Require(Drain().empty(), "set text queued an event");
    Caret caret{};
    caret.index = 6;
    Require(sceImeSetCaret(nullptr) == InvalidAddress, "null caret");
    Require(sceImeSetCaret(&caret) == InvalidParam, "caret past the text");
    caret.index = 1;
    Require(sceImeSetCaret(&caret) == 0, "set caret");
    Publish(ImeHostInputKind::Text, "X");
    events = Drain();
    RequireText(events.at(0), EventUpdateText, u"hXello", 2, 1, 1);

    Publish(ImeHostInputKind::Enter);
    events = Drain();
    RequireText(events.at(0), EventPressEnter, u"hXello", 2, 2, 0);
    g_closeOnPressClose = true;
    Publish(ImeHostInputKind::Escape);
    Publish(ImeHostInputKind::Enter);
    events = Drain();
    g_closeOnPressClose = false;
    Require(events.size() == 1, "events after a close inside the handler");
    RequireText(events[0], EventPressClose, u"hXello", 2, 2, 0);
    Require(g_reentrantClose == 0, "close inside the handler");
    Require(!ImeCapturesKeyboard_nid_postfix(), "keyboard captured after close");
    Require(sceImeClose_nid_postfix() == NotOpened, "second close");
    Publish(ImeHostInputKind::Text, "ignored");
    Require(std::u16string(buffers.text) == u"hXello", "input after close");
    Require(sceImeSetText(u"a", 1) == NotOpened, "set text after close");
}

void CheckMultiline() {
    Buffers buffers;
    Param param = MakeParam(buffers, 16);
    param.option = 1;
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "multiline open");
    Drain();
    Publish(ImeHostInputKind::Text, "a");
    Publish(ImeHostInputKind::Enter);
    Publish(ImeHostInputKind::Text, "b\nc");
    const auto events = Drain();
    Require(events.size() == 3, "multiline events");
    RequireText(events[1], EventUpdateText, u"a\nb\nc", 2, 1, 1);
    RequireText(events[2], EventUpdateText, u"a\nb\nc", 5, 2, 3);
    Require(sceImeSetText(u"x\ny", 3) == 0, "multiline set text");
    Require(sceImeClose_nid_postfix() == 0, "multiline close");
}

void CheckTextFilter() {
    Buffers buffers;
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    Param param = MakeParam(buffers, 8);
    param.filter = reinterpret_cast<ImeTextFilter>(&UpperFilter);
    ExtendedParam extFilters{};
    extFilters.ext_keyboard_filter = reinterpret_cast<ImeExtKeyboardFilter>(&ExtNoop);
    Require(sceImeOpen_nid_postfix(&param, &extFilters) == 0, "open with both filters");
    Drain();

    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"ABC", 3, 0, 1);
    Require(std::u16string(buffers.text) == u"ABC", "filtered input buffer");
    Require(std::u16string(reinterpret_cast<const char16_t*>(buffers.work)) == u"ABC", "filtered work copy");

    Publish(ImeHostInputKind::Backspace);
    RequireText(Drain().at(0), EventUpdateText, u"AB", 2, 2, -1);

    Publish(ImeHostInputKind::Left);
    RequireCaretSteps(Drain(), 1, 1);
    Publish(ImeHostInputKind::Delete);
    RequireText(Drain().at(0), EventUpdateText, u"A", 1, 1, -1);

    Require(sceImeSetText(u"xy", 2) == 0, "set text with a filter");
    Require(std::u16string(buffers.text) == u"xy", "set text is not filtered");
    Require(Drain().empty(), "set text with a filter queued an event");
    Require(sceImeClose_nid_postfix() == 0, "close the filtered session");

    buffers = Buffers{};
    param = MakeParam(buffers, 8);
    param.filter = reinterpret_cast<ImeTextFilter>(&StripXFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a stripping filter");
    Drain();
    Publish(ImeHostInputKind::Text, "x");
    Require(Drain().empty(), "a stripped edit sends no event");
    Require(std::u16string(buffers.text).empty(), "a stripped edit leaves the buffers");
    Publish(ImeHostInputKind::Text, "b");
    RequireText(Drain().at(0), EventUpdateText, u"b", 1, 0, 1);
    Publish(ImeHostInputKind::Text, "x");
    Require(Drain().empty(), "a stripped edit sends no event again");
    Require(std::u16string(buffers.text) == u"b", "buffers after a stripped edit");
    Require(sceImeClose_nid_postfix() == 0, "close the stripping session");

    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&FailingFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a failing filter");
    Drain();
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"abc", 3, 2, 1);
    Require(std::u16string(buffers.text) == u"abc", "a failed filter keeps the edit");
    Require(sceImeClose_nid_postfix() == 0, "close the failing session");

    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&InvalidOutputFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with an invalid-output filter");
    Drain();
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"abc", 3, 2, 1);
    Require(std::u16string(buffers.text) == u"abc", "an invalid filter output keeps the edit");
    Require(sceImeClose_nid_postfix() == 0, "close the invalid-output session");

    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&TooLongFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a too-long filter");
    Drain();
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"abc", 3, 2, 1);
    Require(std::u16string(buffers.text) == u"abc", "an overlong filter output keeps the edit");
    Require(sceImeClose_nid_postfix() == 0, "close the too-long session");

    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&ShortenFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a shortening filter");
    Drain();
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"a", 1, 1, -1);
    Require(sceImeClose_nid_postfix() == 0, "close the shortening session");

    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&PairAtCaretFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a pair filter");
    Drain();
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"ab\U0001F600", 4, 2, 2);
    Require(sceImeClose_nid_postfix() == 0, "close the pair session");

    buffers = Buffers{};
    param = MakeParam(buffers, 8);
    param.filter = reinterpret_cast<ImeTextFilter>(&LockingFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a filter that calls sceIme*");
    Drain();
    g_sawCaptureInsideFilter = false;
    Publish(ImeHostInputKind::Text, "c");
    RequireText(Drain().at(0), EventUpdateText, u"c", 1, 0, 1);
    Require(g_sawCaptureInsideFilter, "the filter can call back into sceIme*");
    Require(sceImeClose_nid_postfix() == 0, "close the locking session");

    buffers = Buffers{};
    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&SetInsideFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a filter that sets the text");
    Drain();
    g_setInsideRc = -1;
    Publish(ImeHostInputKind::Text, "c");
    Require(Drain().empty(), "an edit whose filter set the text sends no event");
    Require(g_setInsideRc == 0, "sceImeSetText inside the filter returns 0");
    Require(std::u16string(buffers.text) == u"zz", "the setText inside the filter wins");
    Require(std::u16string(reinterpret_cast<const char16_t*>(buffers.work)) == u"zz", "the setText inside the filter wins in the work copy");
    Require(sceImeClose_nid_postfix() == 0, "close the set-inside session");

    buffers = Buffers{};
    param = MakeParam(buffers, 8);
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    param.filter = reinterpret_cast<ImeTextFilter>(&SetCaretInsideFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open with a filter that moves the caret");
    Drain();
    g_caretInsideFirstCall = true;
    g_caretInsideRc = -1;
    Publish(ImeHostInputKind::Text, "c");
    Require(Drain().empty(), "an edit whose filter moved the caret sends no event");
    Require(g_caretInsideRc == 0, "sceImeSetCaret inside the filter returns 0");
    Require(std::u16string(buffers.text) == u"ab", "the discarded edit is not in the buffer");
    Publish(ImeHostInputKind::Text, "d");
    RequireText(Drain().at(0), EventUpdateText, u"dab", 1, 0, 1);
    Require(std::u16string(buffers.text) == u"dab", "the edit after the discard builds on the old text");
    Require(std::u16string(reinterpret_cast<const char16_t*>(buffers.work)) == u"dab", "the edit after the discard reaches the work copy");
    Require(sceImeClose_nid_postfix() == 0, "close the caret-inside session");
}

void CheckHostEvents() {
    Buffers buffers;
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "host events open");
    Drain();
    ImeTextInput input;
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    event.text.windowID = 7;
    std::strcpy(event.text.text, "hi");
    input.HandleEvent(event, 7);
    event.text.windowID = 8;
    input.HandleEvent(event, 7);
    event = {};
    event.type = SDL_KEYDOWN;
    event.key.windowID = 7;
    event.key.keysym.scancode = SDL_SCANCODE_BACKSPACE;
    event.key.repeat = 1;
    input.HandleEvent(event, 7);
    event.key.keysym.scancode = SDL_SCANCODE_RETURN;
    input.HandleEvent(event, 7);
    event.key.repeat = 0;
    input.HandleEvent(event, 7);
    event.key.keysym.scancode = SDL_SCANCODE_A;
    input.HandleEvent(event, 7);
    event.type = SDL_KEYUP;
    event.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    input.HandleEvent(event, 7);
    const auto events = Drain();
    Require(events.size() == 3, "host event count");
    RequireText(events[0], EventUpdateText, u"h", 2, 0, 2);
    RequireText(events[1], EventUpdateText, u"h", 1, 1, -1);
    Require(events[2].id == EventPressEnter, "host enter");
    Require(sceImeClose_nid_postfix() == 0, "host events close");
}

}

int main() {
    CheckValidation();
    CheckEditing();
    CheckMultiline();
    CheckTextFilter();
    CheckHostEvents();
}
