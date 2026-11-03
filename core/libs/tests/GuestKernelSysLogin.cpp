#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

extern "C" {
void LoginNameForTest_nid_no_patch(const char* name);
}

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

void Getlogin() {
    char expected[33]{};
    const int error = getlogin_r_nid_postfix(expected, static_cast<int>(sizeof(expected)));
    char* name = getlogin_nid_postfix();
    if (error != 0) {
        Require(name == nullptr && Error() == error);
        return;
    }
    if (expected[0] == '\0') {
        Require(name == nullptr);
        return;
    }
    Require(name != nullptr && std::strcmp(name, expected) == 0);
    Require(getlogin_nid_postfix() == name);
}

void GetloginPaths() {
    LoginNameForTest_nid_no_patch("player");
    char* name = getlogin_nid_postfix();
    Require(name != nullptr && std::strcmp(name, "player") == 0);
    char buffer[7];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(getlogin_r_nid_postfix(buffer, 6) == 34 && buffer[0] == 'x');
    Require(getlogin_r_nid_postfix(buffer, 7) == 0 && std::strcmp(buffer, "player") == 0);

    const std::string longName(40, 'n');
    LoginNameForTest_nid_no_patch(longName.c_str());
    *__error_nid_postfix() = 0;
    Require(getlogin_nid_postfix() == nullptr && Error() == 34);

    LoginNameForTest_nid_no_patch("");
    bool threw = false;
    try {
        getlogin_nid_postfix();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    LoginNameForTest_nid_no_patch(nullptr);
}

}

int main() {
    Getlogin();
    GetloginPaths();
    std::puts("getlogin passed");
    return 0;
}
