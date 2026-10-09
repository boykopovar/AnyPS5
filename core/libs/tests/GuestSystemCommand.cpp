#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
extern "C" {
int APS5_VABI system_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    *__error_nid_postfix() = 13;
    Require(system_nid_postfix(nullptr) == 1);
    const int status = system_nid_postfix("clrxdisasm --version > /dev/null 2>&1");
    Require((status & 0x7f) == 0);
    Require(((status >> 8) & 0xff) == 127);
    Require(system_nid_postfix("exit 0") == status);
    Require(system_nid_postfix("") == status);
    Require(*__error_nid_postfix() == 13);
}
