#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <initializer_list>
extern "C" {
int APS5_VABI waitpid_nid_postfix(int, int*, int);
int APS5_VABI getpid_nid_postfix(void);
int* APS5_VABI __error_nid_postfix();
}
constexpr int noChild = 10;
constexpr int invalidArgument = 22;
constexpr int untouched = 0x5a5a5a5a;
static void Require(bool value) { if (!value) std::abort(); }
static void RequireFailure(int pid, int options, int error) {
    int status = untouched;
    *__error_nid_postfix() = 0;
    Require(waitpid_nid_postfix(pid, &status, options) == -1);
    Require(*__error_nid_postfix() == error);
    Require(status == untouched);
    *__error_nid_postfix() = 0;
    Require(waitpid_nid_postfix(pid, nullptr, options) == -1);
    Require(*__error_nid_postfix() == error);
}
int main() {
    const int self = getpid_nid_postfix();
    for (int pid : {-1, 0, 1, self, self + 1, -self}) {
        for (int options : {0, 1, 2, 3, 4, 8, 16, 32, 0x2c, 0x3f, static_cast<int>(0x80000000u), static_cast<int>(0x80000001u), static_cast<int>(0x8000003fu)})
            RequireFailure(pid, options, noChild);
        for (int options : {0x40, 0x41, 0x80, 0x100, 0x40000000, static_cast<int>(0x80000040u), static_cast<int>(0xffffffffu)})
            RequireFailure(pid, options, invalidArgument);
    }
}
