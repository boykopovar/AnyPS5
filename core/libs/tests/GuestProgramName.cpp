#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {

extern const char* __progname_nid_postfix;

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
    setprogname_nid_postfix("/app0/game.elf");
    Require(std::strcmp(getprogname_nid_postfix(), "game.elf") == 0);
    Require(std::strcmp(_getprogname_nid_postfix(), "game.elf") == 0);
    setprogname_nid_postfix("doom");
    Require(std::strcmp(getprogname_nid_postfix(), "doom") == 0);
    setprogname_nid_postfix("");
    Require(std::strcmp(getprogname_nid_postfix(), "") == 0);
    bool nullNameThrew = false;
    try {
        setprogname_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullNameThrew = true;
    }
    Require(nullNameThrew);
    setprogname_nid_postfix(original);
    Require(getprogname_nid_postfix() == original);
    Require(*__error_nid_postfix() == 45);
}
