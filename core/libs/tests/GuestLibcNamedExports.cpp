#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

#ifndef _WIN32
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {
extern unsigned char _ZSt10adopt_lock_nid_postfix;
extern unsigned char _ZSt10defer_lock_nid_postfix;
extern unsigned char _ZSt11try_to_lock_nid_postfix;
extern unsigned char _ZSt6ignore_nid_postfix;
extern unsigned char _ZNSt12placeholders2_1E_nid_postfix;
extern unsigned char _ZNSt12placeholders3_20E_nid_postfix;
extern void (*_ZSt14_Raise_handler_nid_postfix)();
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int success = 0;

int main() {
    _Lock_shared_ptr_spin_lock_nid_postfix();
    _Unlock_shared_ptr_spin_lock_nid_postfix();
    _Lock_shared_ptr_spin_lock_nid_postfix();
    _Unlock_shared_ptr_spin_lock_nid_postfix();

    void* condition = nullptr;
    Require(_Cnd_init_with_name_nid_postfix(&condition, "NamedCondition") == success);
    Require(condition != nullptr);
    _Cnd_destroy_nid_postfix(&condition);
    Require(condition == nullptr);

    RequireThrows<std::invalid_argument>([] {
        _Cnd_init_with_name_nid_postfix(nullptr, "NullHandle");
    });

    Require(&_ZSt10adopt_lock_nid_postfix != &_ZSt10defer_lock_nid_postfix);
    Require(&_ZSt10adopt_lock_nid_postfix != &_ZSt11try_to_lock_nid_postfix);
    Require(&_ZSt10adopt_lock_nid_postfix != &_ZSt6ignore_nid_postfix);
    Require(&_ZNSt12placeholders2_1E_nid_postfix != &_ZNSt12placeholders3_20E_nid_postfix);
    Require(_ZSt14_Raise_handler_nid_postfix == nullptr);
    Require(&__cxa_call_unexpected_nid_postfix != nullptr);

#ifndef _WIN32
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        __cxa_call_unexpected_nid_postfix(nullptr);
        std::_Exit(1);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#endif
    return 0;
}
