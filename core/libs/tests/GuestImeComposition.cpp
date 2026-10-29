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
constexpr uint32_t EventUpdateCaret = 2;
constexpr uint32_t EventPressClose = 4;
constexpr uint32_t EventPressEnter = 5;

struct Recorded {
    uint32_t id;
    std::u16string text;
};

std::vector<Recorded> g_events;
int g_filterCalls = 0;

void APS5_VABI Record(void*, const ImeEvent* event) {
    Recorded recorded{event->id, {}};
    if (event->id == EventUpdateText || event->id == EventPressEnter || event->id == EventPressClose) recorded.text = event->param.text.str;
    g_events.push_back(recorded);
}

int32_t APS5_VABI CountingFilter(char16_t* outText, uint32_t* outTextLength, const char16_t* sourceText, uint32_t sourceTextLength) {
    ++g_filterCalls;
    for (uint32_t i = 0; i < sourceTextLength; ++i) outText[i] = sourceText[i];
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

void Publish(ImeHostInputKind kind, const char* text = "", std::int32_t start = 0, std::int32_t length = 0) {
    ImeHostInput input{};
    input.kind = kind;
    std::strncpy(input.text, text, sizeof(input.text) - 1);
    input.start = start;
    input.length = length;
    ImePublishInput_nid_postfix(input);
}

const char16_t* Work(const Buffers& buffers) {
    return reinterpret_cast<const char16_t*>(buffers.work);
}

void CheckLifecycle() {
    Buffers buffers;
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    Param param = MakeParam(buffers, 16);
    param.filter = reinterpret_cast<ImeTextFilter>(&CountingFilter);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open");
    Drain();

    Publish(ImeHostInputKind::Preedit, "\xe3\x81\x93", 1, 0);
    Require(Drain().empty(), "a preedit queued an event");
    Publish(ImeHostInputKind::Preedit, "\xe3\x81\x93\xe3\x82\x93", 2, 0);
    Require(Drain().empty(), "a second preedit queued an event");
    Require(std::u16string(buffers.text) == u"ab" && std::u16string(Work(buffers)) == u"ab", "a preedit reached the guest buffers");
    Require(g_filterCalls == 0, "a preedit reached the text filter");

    Publish(ImeHostInputKind::Preedit, "");
    Publish(ImeHostInputKind::Text, "\xe3\x81\x93\xe3\x82\x93");
    const auto committed = Drain();
    Require(committed.size() == 1 && committed[0].id == EventUpdateText && committed[0].text == u"ab\u3053\u3093", "the commit is one text update");
    Require(std::u16string(buffers.text) == u"ab\u3053\u3093", "the commit reached the input buffer");
    Require(g_filterCalls == 1, "the commit reached the text filter once");

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Text, "\xe4\xbb\x8a");
    Publish(ImeHostInputKind::Backspace);
    const auto afterCommit = Drain();
    Require(afterCommit.size() == 2 && afterCommit[1].text == u"ab\u3053\u3093", "a commit ends the composition so Backspace edits again");
    Require(sceImeClose_nid_postfix() == 0, "close");
}

void CheckCancel() {
    Buffers buffers;
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open for cancel");
    Drain();

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Escape);
    Require(Drain().empty(), "Escape while composing closed the panel");
    Publish(ImeHostInputKind::Escape);
    const auto closed = Drain();
    Require(closed.size() == 1 && closed[0].id == EventPressClose, "the next Escape reaches the title");

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Enter);
    Require(Drain().empty(), "Enter while composing accepted the text");
    Publish(ImeHostInputKind::Enter);
    const auto entered = Drain();
    Require(entered.size() == 1 && entered[0].id == EventPressEnter, "the next Enter reaches the title");

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Preedit, "");
    Publish(ImeHostInputKind::Escape);
    const auto emptied = Drain();
    Require(emptied.size() == 1 && emptied[0].id == EventPressClose, "an emptied composition no longer swallows Escape");
    Require(std::u16string(buffers.text) == u"ab", "a discarded composition changed the buffer");
    Require(sceImeClose_nid_postfix() == 0, "close after cancel");
}

void CheckEditKeysBelongToTheInputMethod() {
    Buffers buffers;
    std::memcpy(buffers.text, u"abc", sizeof(u"abc"));
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open for edit keys");
    Drain();

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    for (const auto kind : {ImeHostInputKind::Backspace, ImeHostInputKind::Delete, ImeHostInputKind::Left, ImeHostInputKind::Right, ImeHostInputKind::Home, ImeHostInputKind::End}) Publish(kind);
    Require(Drain().empty() && std::u16string(buffers.text) == u"abc", "an editing key acted on the text while composing");

    Publish(ImeHostInputKind::Preedit, "");
    Publish(ImeHostInputKind::Left);
    const auto moved = Drain();
    Require(moved.size() == 1 && moved[0].id == EventUpdateCaret, "Left works once the composition is gone");
    Publish(ImeHostInputKind::Backspace);
    const auto erased = Drain();
    Require(erased.size() == 1 && erased[0].text == u"ac", "Backspace works once the composition is gone");
    Require(sceImeClose_nid_postfix() == 0, "close after edit keys");
}

void CheckCloseDiscardsTheComposition() {
    Buffers buffers;
    std::memcpy(buffers.text, u"ab", sizeof(u"ab"));
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "first open");
    Drain();
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Drain();
    Require(sceImeClose_nid_postfix() == 0, "close with a composition");
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "second open");
    Drain();
    Publish(ImeHostInputKind::Backspace);
    const auto erased = Drain();
    Require(erased.size() == 1 && erased[0].text == u"a", "a composition survived the close");
    Require(sceImeClose_nid_postfix() == 0, "close the second session");
}

void CheckMalformedPreedit() {
    Buffers buffers;
    Param param = MakeParam(buffers, 16);
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open for malformed");
    Drain();
    char cut[sizeof(ImeHostInput::text)] = {};
    std::memset(cut, 'a', sizeof(cut));
    cut[30] = '\xe3';
    cut[31] = '\x81';
    ImeHostInput input{};
    input.kind = ImeHostInputKind::Preedit;
    std::memcpy(input.text, cut, sizeof(cut));
    ImePublishInput_nid_postfix(input);
    Publish(ImeHostInputKind::Preedit, "\xff\xfe\x80", 0, 0);
    Require(Drain().empty() && buffers.text[0] == u'\0', "a malformed preedit changed the session");
    Publish(ImeHostInputKind::Preedit, "");
    Publish(ImeHostInputKind::Text, "z");
    const auto typed = Drain();
    Require(typed.size() == 1 && typed[0].text == u"z", "typing still works after a malformed preedit");
    Require(sceImeClose_nid_postfix() == 0, "close after malformed");
}

}

int main() {
    CheckLifecycle();
    CheckCancel();
    CheckEditKeysBelongToTheInputMethod();
    CheckCloseDiscardsTheComposition();
    CheckMalformedPreedit();
}
