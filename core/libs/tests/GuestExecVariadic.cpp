#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <string>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    const std::string huge(300000, 'X');
    const std::string hugeEntry = "ANYPS5_BIG=" + huge;
    char name[] = "anyps5-exec-check-missing";
    char oversized[] = "ANYPS5_BIG";
    std::string hugeArgument = huge;

    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, "one", nullptr) == -1 && *__error_nid_postfix() == 2);
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, hugeArgument.c_str(), nullptr) == -1 && *__error_nid_postfix() == 7);
    *__error_nid_postfix() = 0;
    Require(execlp_nid_postfix(name, name, hugeArgument.c_str(), nullptr) == -1 && *__error_nid_postfix() == 7);

    char* const smallEnvironment[] = {oversized, nullptr};
    *__error_nid_postfix() = 0;
    Require(execle_nid_postfix(name, name, "one", nullptr, smallEnvironment) == -1 && *__error_nid_postfix() == 2);
    char* const hugeEnvironment[] = {const_cast<char*>(hugeEntry.c_str()), nullptr};
    *__error_nid_postfix() = 0;
    Require(execle_nid_postfix(name, name, "one", nullptr, hugeEnvironment) == -1 && *__error_nid_postfix() == 7);

    char small[] = "s";
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, small, small, small, small, small, small, nullptr) == -1 && *__error_nid_postfix() == 2);
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, small, small, small, hugeArgument.c_str(), nullptr) == -1 && *__error_nid_postfix() == 7);
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, small, small, small, small, hugeArgument.c_str(), nullptr) == -1 && *__error_nid_postfix() == 7);
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix(name, name, small, small, small, small, small, hugeArgument.c_str(), nullptr) == -1 && *__error_nid_postfix() == 7);
    *__error_nid_postfix() = 0;
    Require(execle_nid_postfix(name, name, small, small, small, small, nullptr, hugeEnvironment) == -1 && *__error_nid_postfix() == 7);
    return 0;
}
