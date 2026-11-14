#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <atomic>

namespace {

void DefaultUnexpectedHandler() {
    _ZSt9terminatev_nid_postfix();
}

std::atomic<LibcUnexpectedHandler> unexpectedHandler{&DefaultUnexpectedHandler};

}

extern "C" {

LibcUnexpectedHandler APS5_VABI _ZSt14set_unexpectedPFvvE_nid_postfix(LibcUnexpectedHandler handler) {
    return unexpectedHandler.exchange(handler ? handler : &DefaultUnexpectedHandler);
}

LibcUnexpectedHandler APS5_VABI _ZSt14get_unexpectedv_nid_postfix() {
    return unexpectedHandler.load();
}

[[noreturn]] void APS5_VABI _ZSt10unexpectedv_nid_postfix() {
    LibcUnexpectedHandler handler = unexpectedHandler.load();
    if (handler) {
        handler();
    }
    _ZSt9terminatev_nid_postfix();
}

}
