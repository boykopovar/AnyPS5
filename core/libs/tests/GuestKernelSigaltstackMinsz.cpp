#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static void TestSigaltstackMinsz() {
    alignas(16) char stackBuffer[128];
    GuestStack st{};
    st.ss_sp = stackBuffer;
    st.ss_size = 64;
    st.ss_flags = 0;

    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&st, nullptr) == -1);
    Require(*__error_nid_postfix() == 12);
}

static void TestSigaltstackInvalidFlags() {
    alignas(16) char stackBuffer[8192];
    GuestStack st{};
    st.ss_sp = stackBuffer;
    st.ss_size = sizeof(stackBuffer);
    st.ss_flags = 0xFFFF;

    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&st, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
}

static void TestSigaltstackNullPointerRejection() {
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
}

int main() {
    TestSigaltstackMinsz();
    TestSigaltstackInvalidFlags();
    TestSigaltstackNullPointerRejection();
    return 0;
}
