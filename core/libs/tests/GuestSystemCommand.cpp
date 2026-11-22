#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template<class TCall>
static bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    *__error_nid_postfix() = 13;
    Require(system_nid_postfix(nullptr) == 1);
    const int status = system_nid_postfix("clrxdisasm --version > /dev/null 2>&1");
    Require((status & 0x7f) == 0);
    Require(((status >> 8) & 0xff) == 127);
    Require(system_nid_postfix("exit 0") == status);
    Require(system_nid_postfix("") == status);
    Require(NotImplemented([] { popen_nid_postfix("true", "r"); }));
    Require(NotImplemented([] { pclose_nid_postfix(nullptr); }));
    Require(*__error_nid_postfix() == 13);
}
