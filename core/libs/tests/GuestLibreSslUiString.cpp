#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestUi {
    const void* meth;
    void* strings;
    void* userData;
    void* exData;
    int flags;
};

struct TestUiString {
    int type;
    const char* outString;
    int inputFlags;
    char* resultBuf;
    unsigned char data[24];
    int flags;
};

static_assert(sizeof(TestUi) == 40 && offsetof(TestUi, userData) == 16);
static_assert(sizeof(TestUiString) == 64 && offsetof(TestUiString, inputFlags) == 16);

static const LibreSslUi* Ui(const TestUi* ui) {
    return reinterpret_cast<const LibreSslUi*>(ui);
}

static const LibreSslUiString* UiString(const TestUiString* string) {
    return reinterpret_cast<const LibreSslUiString*>(string);
}

static void TestUserData() {
    int data = 0;
    int strings = 0;
    TestUi ui {};
    ExpectErr([] { UI_get0_user_data_nid_postfix(nullptr); });
    Require(UI_get0_user_data_nid_postfix(Ui(&ui)) == nullptr);
    ui.strings = &strings;
    ui.userData = &data;
    ui.flags = 0x0101;
    Require(UI_get0_user_data_nid_postfix(Ui(&ui)) == &data);
}

static void TestStringType() {
    TestUiString string {};
    Require(UI_get_string_type_nid_postfix(nullptr) == 0);
    Require(UI_get_string_type_nid_postfix(UiString(&string)) == 0);
    string.inputFlags = 1;
    string.flags = 1;
    for (int type = 1; type <= 5; ++type) {
        string.type = type;
        Require(UI_get_string_type_nid_postfix(UiString(&string)) == type);
    }
}

static void TestInputFlags() {
    TestUiString string {};
    Require(UI_get_input_flags_nid_postfix(nullptr) == 0);
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0);
    string.type = 2;
    string.flags = 1;
    string.inputFlags = 0x01;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0x01);
    string.inputFlags = 0x8001;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0x8001);
    string.inputFlags = -1;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == -1);
}

static void TestGuestPointers() {
    errno = 71;
    ExpectErr([] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(0x10)); });
    const auto* const unmapped = reinterpret_cast<const LibreSslUiString*>(0x10);
    Require(UI_get_string_type_nid_postfix(unmapped) == 0 && UI_get_input_flags_nid_postfix(unmapped) == 0);
    Require(errno == 71);
}

int main() {
    TestUserData();
    TestStringType();
    TestInputFlags();
    TestGuestPointers();
    return 0;
}
