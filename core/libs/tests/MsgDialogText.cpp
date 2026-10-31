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
    Require(MsgDialog::DecodeUtf8("a\xE2\x82") == u"a��", "a truncated sequence");
    Require(MsgDialog::DecodeUtf8("\xE2\x28\xA1") == u"�(�", "a broken continuation byte");
}
