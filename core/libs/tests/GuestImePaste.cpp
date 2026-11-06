#include "prx/libSceIme/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t EventUpdateText = 1;

struct Recorded {
    uint32_t id;
    std::u16string text;
    uint32_t index;
    int32_t length;
};

std::vector<Recorded> g_events;
int g_filterCalls = 0;

void APS5_VABI Record(void*, const ImeEvent* event) {
    Recorded recorded{event->id, {}, 0, 0};
    if (event->id == EventUpdateText) {
        recorded.text = event->param.text.str;
        recorded.index = event->param.text.text_area[0].index;
        recorded.length = event->param.text.text_area[0].length;
    }
    g_events.push_back(recorded);
}

int32_t APS5_VABI DigitsOnlyFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    ++g_filterCalls;
    uint32_t length = 0;
    for (uint32_t i = 0; i < sourceTextLength; ++i) {
        if (sourceText[i] >= u'0' && sourceText[i] <= u'9') outText[length++] = sourceText[i];
    }
    *outTextLength = length;
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

Param MakeParam(Buffers& buffers, uint32_t maxLength, uint32_t option = 0) {
    Param param;
    sceImeParamInit(&param);
    param.user_id = 1;
    param.max_text_length = maxLength;
    param.input_text_buffer = buffers.text;
    param.option = option;
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

void Paste(const std::string& utf8) {
    ImePublishPaste_nid_postfix(utf8.data(), utf8.size());
}

void Publish(ImeHostInputKind kind, const char* text = "", std::int32_t start = 0, std::int32_t length = 0) {
    ImeHostInput input{};
    input.kind = kind;
    std::strncpy(input.text, text, sizeof(input.text) - 1);
    input.start = start;
    input.length = length;
    ImePublishInput_nid_postfix(input);
}

struct Session {
    Buffers buffers;
    explicit Session(const char16_t* initial, uint32_t maxLength, uint32_t option = 0, ImeTextFilter filter = nullptr) {
        std::memcpy(buffers.text, initial, (std::char_traits<char16_t>::length(initial) + 1) * sizeof(char16_t));
        Param param = MakeParam(buffers, maxLength, option);
        param.filter = filter;
        Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open");
        Drain();
    }
    ~Session() { sceImeClose_nid_postfix(); }
    std::u16string Text() const { return buffers.text; }
};

void CheckInsertAtCaret() {
    Session session(u"ab", 16);
    Publish(ImeHostInputKind::Left);
    Drain();
    Paste("XY");
    const auto events = Drain();
    Require(events.size() == 1 && events[0].id == EventUpdateText, "a paste is one text update");
    Require(events[0].text == u"aXYb" && events[0].index == 1 && events[0].length == 2, "the paste lands at the caret");
    Require(session.Text() == u"aXYb", "the paste reached the input buffer");
    Publish(ImeHostInputKind::Text, "z");
    Require(Drain().at(0).text == u"aXYzb", "the caret sits after the pasted text");
}

void CheckSingleLineAndMultiline() {
    {
        Session session(u"", 32);
        Paste("a\r\nb\nc\rd");
        Drain();
        Require(session.Text() == u"abcd", "a single-line session drops line breaks");
    }
    {
        Session session(u"", 32, 0x00000001);
        Paste("a\r\nb\nc\rd");
        Drain();
        Require(session.Text() == u"a\nb\nc\nd", "a multi-line session keeps line breaks as LF");
    }
}

void CheckControlCharacters() {
    Session session(u"", 32);
    Paste(std::string("a\tb\x01" "c\x1b" "d\x7f" "e\0f", 11));
    Drain();
    Require(session.Text() == u"abcdef", "controls are stripped from a paste");
}

void CheckMaxLength() {
    {
        Session session(u"abc", 5);
        Paste("defgh");
        const auto events = Drain();
        Require(session.Text() == u"abcde" && events.size() == 1 && events[0].length == 2, "a paste stops at the maximum length");
        Paste("z");
        Require(Drain().empty() && session.Text() == u"abcde", "a paste into a full field does nothing");
    }
    {
        Session session(u"abc", 5);
        Paste("d\xf0\x9f\x98\x80");
        Drain();
        Require(session.Text() == u"abcd", "a cut that would split a surrogate pair drops the whole character");
    }
    {
        Session session(u"abc", 6);
        Paste("d\xf0\x9f\x98\x80");
        Drain();
        Require(session.Text() == u"abcd\U0001F600", "a pair that fits is kept whole");
    }
    {
        Session session(u"", 1);
        Paste("\xf0\x9f\x98\x80");
        Require(Drain().empty() && session.Text().empty(), "a pair that cannot fit is not half inserted");
    }
}

void CheckFilter() {
    g_filterCalls = 0;
    Session session(u"", 16, 0, reinterpret_cast<ImeTextFilter>(&DigitsOnlyFilter));
    Paste("1a2b3\xe3\x81\x82" "4");
    const auto events = Drain();
    Require(g_filterCalls == 1, "a paste runs the text filter once");
    Require(session.Text() == u"1234" && events.size() == 1 && events[0].text == u"1234", "the filter shapes the pasted text");
}

void CheckHugeClipboard() {
    Session session(u"", 2048);
    const std::string huge(8u * 1024u * 1024u, 'x');
    Paste(huge);
    Drain();
    Require(session.Text().size() == 2048, "a huge clipboard fills the field and no more");
}

void CheckInvalidUtf8() {
    Session session(u"", 16);
    Paste("a\xff" "b\xe3\x81");
    Drain();
    Require(session.Text() == u"a\uFFFDb\uFFFD", "an invalid clipboard is replaced and never throws");
}

void CheckNothingCaptured() {
    Require(!ImeCapturesKeyboard_nid_postfix(), "nothing should be capturing");
    Paste("ignored");
    ImePublishPaste_nid_postfix(nullptr, 5);
    Buffers buffers;
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open after an ignored paste");
    const auto opened = Drain();
    Require(opened.size() == 1 && opened[0].id == 0 && buffers.text[0] == u'\0', "a paste with no session was kept for later");
    Paste("");
    Require(Drain().empty(), "an empty paste queued an update");
    ImePublishPaste_nid_postfix(nullptr, 5);
    Require(Drain().empty(), "a null clipboard queued an update");
    Require(sceImeClose_nid_postfix() == 0, "close");
}

void CheckPasteWhileComposing() {
    Session session(u"ab", 16);
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Paste("XY");
    Require(Drain().empty() && session.Text() == u"ab", "a paste went in while the input method held a composition");
    Publish(ImeHostInputKind::Preedit, "");
    Paste("XY");
    Drain();
    Require(session.Text() == u"abXY", "a paste works once the composition ends");
}

}

int main() {
    CheckNothingCaptured();
    CheckInsertAtCaret();
    CheckSingleLineAndMultiline();
    CheckControlCharacters();
    CheckMaxLength();
    CheckFilter();
    CheckHugeClipboard();
    CheckInvalidUtf8();
    CheckPasteWhileComposing();
}
