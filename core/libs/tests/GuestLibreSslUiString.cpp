#include "prx/libc/include/general/VabiMacros.hpp"
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

extern "C" {
void* APS5_VABI UI_get0_user_data_nid_postfix(const TestUi*);
int APS5_VABI UI_get_string_type_nid_postfix(const TestUiString*);
int APS5_VABI UI_get_input_flags_nid_postfix(const TestUiString*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL UI string check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestUserData() {
    int data = 0;
    int strings = 0;
    TestUi ui {};
    RequireThrows([] { UI_get0_user_data_nid_postfix(nullptr); });
    Require(UI_get0_user_data_nid_postfix(&ui) == nullptr);
    ui.strings = &strings;
    ui.userData = &data;
    ui.flags = 0x0101;
    Require(UI_get0_user_data_nid_postfix(&ui) == &data);
}

static void TestStringType() {
    TestUiString string {};
    Require(UI_get_string_type_nid_postfix(nullptr) == 0);
    Require(UI_get_string_type_nid_postfix(&string) == 0);
    string.inputFlags = 1;
    string.flags = 1;
    for (int type = 1; type <= 5; ++type) {
        string.type = type;
        Require(UI_get_string_type_nid_postfix(&string) == type);
    }
}

static void TestInputFlags() {
    TestUiString string {};
    Require(UI_get_input_flags_nid_postfix(nullptr) == 0);
    Require(UI_get_input_flags_nid_postfix(&string) == 0);
    string.type = 2;
    string.flags = 1;
    string.inputFlags = 0x01;
    Require(UI_get_input_flags_nid_postfix(&string) == 0x01);
    string.inputFlags = 0x8001;
    Require(UI_get_input_flags_nid_postfix(&string) == 0x8001);
    string.inputFlags = -1;
    Require(UI_get_input_flags_nid_postfix(&string) == -1);
}

int main() {
    TestUserData();
    TestStringType();
    TestInputFlags();
    return 0;
}
