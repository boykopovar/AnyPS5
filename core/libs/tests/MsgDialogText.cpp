#include "prx/libSceMsgDialog/include/MsgDialogText.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "msg_dialog_text: %s\n", what);
        std::abort();
    }
}

}

int main() {
    Require(MsgDialog::DecodeUtf8("") == u"", "an empty string is not empty");
    Require(MsgDialog::DecodeUtf8("Save?\nYes") == u"Save?\nYes", "ASCII was changed");
    Require(MsgDialog::DecodeUtf8("\xC3\xA9") == u"é", "a two-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xE2\x82\xAC") == u"€", "a three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x9F\x98\x80") == u"\U0001F600", "a four-byte sequence is not a surrogate pair");
    Require(MsgDialog::DecodeUtf8("\xFF") == u"�", "an invalid lead byte");
    Require(MsgDialog::DecodeUtf8("\x80") == u"�", "a stray continuation byte");
    Require(MsgDialog::DecodeUtf8("\xC0\x80") == u"��", "an overlong two-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xE0\x80\x80") == u"���", "an overlong three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xED\xA0\x80") == u"���", "an encoded surrogate");
    Require(MsgDialog::DecodeUtf8("\xF4\x90\x80\x80") == u"����", "a code point past U+10FFFF");
    Require(MsgDialog::DecodeUtf8("a\xE2\x82") == u"a�", "a truncated sequence");
    Require(MsgDialog::DecodeUtf8("\xE2\x28\xA1") == u"�(�", "a broken continuation byte");
    Require(MsgDialog::DecodeUtf8("\xE3\x81" "A") == u"\uFFFDA", "an interrupted three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x9F") == u"\uFFFD", "a truncated two-byte prefix of a four-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x9F\x98") == u"\uFFFD", "a truncated three-byte prefix of a four-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x9F\x98" "Z") == u"\uFFFDZ", "an interrupted four-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xE0\xA0") == u"\uFFFD", "a truncated E0 sequence with a valid second byte");
    Require(MsgDialog::DecodeUtf8("\xE3\x81\xE3\x81\x82") == u"\uFFFD\u3042", "a truncated three-byte sequence followed by a valid sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x9F\x98" "\xE3\x81\x82") == u"\uFFFD\u3042", "a truncated four-byte sequence followed by a valid sequence");
    Require(MsgDialog::DecodeUtf8("\xE0\x9F\x80") == u"\uFFFD\uFFFD\uFFFD", "an overlong E0 sequence below 0xA0");
    Require(MsgDialog::DecodeUtf8("\xF0\x8F\x80\x80") == u"\uFFFD\uFFFD\uFFFD\uFFFD", "an overlong F0 sequence below 0x90");
    Require(MsgDialog::DecodeUtf8("\xC2\x80") == u"\u0080", "minimum valid two-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xDF\xBF") == u"\u07FF", "maximum valid two-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xE0\xA0\x80") == u"\u0800", "minimum valid three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xED\x9F\xBF") == u"\uD7FF", "maximum pre-surrogate three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xEE\x80\x80") == u"\uE000", "minimum post-surrogate three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xEF\xBF\xBF") == u"\uFFFF", "maximum valid three-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF0\x90\x80\x80") == u"\U00010000", "minimum valid four-byte sequence");
    Require(MsgDialog::DecodeUtf8("\xF4\x8F\xBF\xBF") == u"\U0010FFFF", "maximum valid Unicode scalar U+10FFFF");
}
