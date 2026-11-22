#include "prx/libSceImeBackend/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kNotInitialized = static_cast<int>(0x80bc1002);
constexpr int kAlreadyInitialized = static_cast<int>(0x80bc1001);
constexpr int kInvalidMode = static_cast<int>(0x80bc1010);
constexpr int kInvalidLength = static_cast<int>(0x80bc1011);
constexpr int kInvalidType = static_cast<int>(0x80bc1012);
constexpr int kInvalidReservedArgument = static_cast<int>(0x80bc1014);
constexpr int kInvalidStringSelector = static_cast<int>(0x80bc1030);
constexpr int kNullArgument = static_cast<int>(0x80bc1031);
constexpr int kInvalidReservedField = static_cast<int>(0x80bc1032);
constexpr int kInitialStringTooLong = static_cast<int>(0x80bc1034);
constexpr int kInvalidField18 = static_cast<int>(0x80bc1035);

bool TextEquals(const char16_t* text, const char16_t* expected) {
    return std::char_traits<char16_t>::compare(text, expected, std::char_traits<char16_t>::length(expected) + 1) == 0;
}

SceImeBackendParam MakeParam() {
    SceImeBackendParam param;
    sceImeBackendParamInit(&param);
    param.mode = 0;
    param.maxLength = 8;
    return param;
}

void CheckValidation() {
    SceImeBackendParam param = MakeParam();
    Require(sceImeBackendOpen(nullptr, nullptr) == kNullArgument);
    Require(sceImeBackendOpen(&param, &param) == kInvalidReservedArgument);

    param.mode = -1;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidMode);
    param = MakeParam();
    param.maxLength = 0;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidLength);
    param = MakeParam();
    param.type = 4;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidType);
    param = MakeParam();
    param.field18 = 2;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidField18);
    param = MakeParam();
    param.reserved[0] = 1;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidReservedField);

    const std::u16string longText(0x400, u'A');
    param = MakeParam();
    param.type = 2;
    param.initialString = longText.c_str();
    Require(sceImeBackendOpen(&param, nullptr) == kInitialStringTooLong);
}

void CheckLifecycle() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Hi";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);
    Require(sceImeBackendOpen(&param, nullptr) == kAlreadyInitialized);
    Require(sceImeBackendGetStatus() == 1);
    Require(sceImeBackendGetCaretIndex() == 2);

    char16_t text[0x20] = {};
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(0, text, 0x20) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(1, text, 0x20) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(2, text, 0x20) == kInvalidStringSelector);
    Require(sceImeBackendGetString(0, nullptr, 0x20) == kNullArgument);

    int mode = -1;
    Require(sceImeBackendGetConvertMode(&mode) == 0 && mode == 0);
    Require(sceImeBackendSetConvertMode(1) == 0);
    Require(sceImeBackendGetConvertMode(&mode) == 0 && mode == 1);

    const std::uint16_t noCandidates = 1;
    Require(sceImeBackendGetCandidateListSize() == 0);
    std::uint16_t selected = noCandidates;
    Require(sceImeBackendGetCandidateList(&selected, text) == 0 && selected == 0 && text[0] == 0);
    Require(sceImeBackendGetCandidateSelect(&selected) == 0 && selected == 0);

    Require(sceImeBackendClose() == 0);
    Require(sceImeBackendGetStatus() == 0);
    Require(sceImeBackendClose() == kNotInitialized);
    Require(sceImeBackendGetCaretIndex() == kNotInitialized);
}

void CheckEditing() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Hi";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);

    char16_t text[0x20] = {};
    Require(sceImeBackendEnterCharacter(u'A', text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetString(0, text, 0x20) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetCaretIndex() == 3);

    Require(sceImeBackendModeCaretPrevious() == 0);
    Require(sceImeBackendGetCaretIndex() == 2);
    Require(sceImeBackendEnterCharacter(u'B', text) == 0 && TextEquals(text, u"HiBA"));
    Require(sceImeBackendBackspaceCharacter() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendModeCaretPrevious() == 0);
    Require(sceImeBackendDeleteCharacter() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HA"));

    Require(sceImeBackendAllDeleteConvertString() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u""));
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u""));

    Require(sceImeBackendReset() == 0);
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u""));
    Require(sceImeBackendGetCaretIndex() == 0);

    Require(sceImeBackendClose() == 0);
}

void CheckStringCapacity() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Abcd";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);

    char16_t text[0x20];
    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetString(0, text, 4) == 0);
    Require(text[0] == u'A' && text[1] == u'b' && text[2] == u'c' && text[3] == u'd' && text[4] == 0 && text[5] == u'~');

    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetString(0, text, 2) == 0);
    Require(text[0] == u'A' && text[1] == u'b' && text[2] == 0 && text[3] == u'~');

    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetConvertString(text) == 0 && text[4] == 0 && text[5] == u'~');
    Require(sceImeBackendGetConfirmString(text) == 0 && text[4] == 0 && text[5] == u'~');

    Require(sceImeBackendClose() == 0);
}

}

int main() {
    CheckValidation();
    CheckLifecycle();
    CheckEditing();
    CheckStringCapacity();
    return 0;
}
