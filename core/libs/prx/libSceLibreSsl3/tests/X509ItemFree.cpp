#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

template<typename TAction>
void RequireNoException(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (...) {
        Require(false, location);
    }
}

void TestNull() {
    RequireNoException([] { AUTHORITY_KEYID_free_nid_postfix(nullptr); });
    RequireNoException([] { X509_CRL_free_nid_postfix(nullptr); });
    RequireNoException([] { PKCS7_free_nid_postfix(nullptr); });
}

void TestNonNull() {
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    ExpectErr<std::runtime_error>([&] { AUTHORITY_KEYID_free_nid_postfix(unreadable); });
    ExpectErr<std::runtime_error>([&] { X509_CRL_free_nid_postfix(unreadable); });
    ExpectErr<std::runtime_error>([&] { PKCS7_free_nid_postfix(unreadable); });
}

}

int main() {
    TestNull();
    TestNonNull();
    std::puts("LibreSSL3 X509 item free tests passed");
}
