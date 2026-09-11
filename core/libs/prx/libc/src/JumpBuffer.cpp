#include <csetjmp>

#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI setjmp_nid_postfix(std::jmp_buf env) {
    return setjmp(env);
}

[[noreturn]] void longjmp_nid_postfix(std::jmp_buf env, int val) {
    std::longjmp(env, val);
}

}
