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

int main() {
    TestUserData();
    return 0;
}
