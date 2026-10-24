#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
const char* APS5_VABI getprogname_nid_postfix();
const char* APS5_VABI _getprogname_nid_postfix();
extern const char* __progname_nid_postfix;
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const char* const original = __progname_nid_postfix;
    *__error_nid_postfix() = 45;
    Require(getprogname_nid_postfix() == original);
    Require(_getprogname_nid_postfix() == original);
    Require(std::strcmp(getprogname_nid_postfix(), "eboot.bin") == 0);
    Require(*__error_nid_postfix() == 45);

    char renamed[] = "quake2";
    __progname_nid_postfix = renamed;
    Require(getprogname_nid_postfix() == renamed);
    Require(_getprogname_nid_postfix() == renamed);
    renamed[0] = 'Q';
    Require(std::strcmp(getprogname_nid_postfix(), "Quake2") == 0);

    __progname_nid_postfix = "";
    Require(getprogname_nid_postfix()[0] == '\0');

    __progname_nid_postfix = original;
    Require(getprogname_nid_postfix() == original);
    Require(*__error_nid_postfix() == 45);
}
