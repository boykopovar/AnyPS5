#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    Require(_ZN7WebCore7Process10identifierEv() == 1);
    Require(_ZN7WebCore7Process10identifierEv() == 1);
    _ZN7WebCore7Process13setIdentifierEN3WTF23ObjectIdentifierGenericINS_21ProcessIdentifierTypeENS1_38ObjectIdentifierMainThreadAccessTraitsEEE(42);
    Require(_ZN7WebCore7Process10identifierEv() == 42);
    Require(_ZN7WebCore7Process10identifierEv() == 42);
    std::printf("WebCore::Process: the first query takes the first object identifier, keeps it, and a later set replaces it\n");
}
