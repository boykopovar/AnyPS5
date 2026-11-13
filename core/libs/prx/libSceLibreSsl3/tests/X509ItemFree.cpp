#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 item free check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireNoException(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (...) {
        Require(false, location);
    }
}

template<typename TAction>
void RequireNotImplemented(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

void TestNull() {
    RequireNoException([] { AUTHORITY_KEYID_free_nid_postfix(nullptr); });
    RequireNoException([] { X509_CRL_free_nid_postfix(nullptr); });
}

void TestNonNull() {
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    RequireNotImplemented([&] { AUTHORITY_KEYID_free_nid_postfix(unreadable); });
    RequireNotImplemented([&] { X509_CRL_free_nid_postfix(unreadable); });
}

}

int main() {
    TestNull();
    TestNonNull();
    std::puts("LibreSSL3 X509 item free tests passed");
}
