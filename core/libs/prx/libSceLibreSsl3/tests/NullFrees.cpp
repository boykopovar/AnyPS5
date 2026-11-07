#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 null frees check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireNotImplemented(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false);
}

void TestNullFrees() {
    try {
        EC_POINT_clear_free_nid_postfix(nullptr);
        EVP_PKEY_CTX_free_nid_postfix(nullptr);
    } catch (...) {
        Require(false);
    }
}

}

int main() {
    TestNullFrees();
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    RequireNotImplemented([&] { EC_POINT_clear_free_nid_postfix(unreadable); });
    RequireNotImplemented([&] { EVP_PKEY_CTX_free_nid_postfix(unreadable); });
}
