#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    *__error_nid_postfix() = 0;
    Require(execl_nid_postfix("/nonexistent/anyps5-exec-check", "anyps5-exec-check", "one", nullptr) == -1);
    Require(*__error_nid_postfix() == 2);
    char* const environment[] = {nullptr};
    *__error_nid_postfix() = 0;
    Require(execle_nid_postfix("/nonexistent/anyps5-exec-check", "anyps5-exec-check", "one", nullptr, environment) == -1);
    Require(*__error_nid_postfix() == 2);
    *__error_nid_postfix() = 0;
    Require(execlp_nid_postfix("anyps5-exec-check-missing", "anyps5-exec-check-missing", nullptr) == -1);
    Require(*__error_nid_postfix() == 2);
    return 0;
}
