#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
[[noreturn]] void APS5_VABI _ZSt14_Atomic_assertPKcS0__nid_postfix(const char*, const char*);
}
static void Require(bool value) { if (!value) std::abort(); }
static void RequireMessage(const char* expression, const char* file, const char* message) {
    try { _ZSt14_Atomic_assertPKcS0__nid_postfix(expression, file); }
    catch (const std::runtime_error& error) { Require(std::strcmp(error.what(), message) == 0); return; }
    std::abort();
}
int main() {
    RequireMessage("expression", "file.cpp", "std::_Atomic_assert: expression, file.cpp");
    RequireMessage(nullptr, nullptr, "std::_Atomic_assert: (null), (null)");
}
