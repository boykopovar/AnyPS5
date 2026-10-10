#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
extern "C" {
int APS5_VABI seteuid_nid_postfix(std::uint32_t);
int APS5_VABI setegid_nid_postfix(std::uint32_t);
int* APS5_VABI __error_nid_postfix();
}
using SetId = int (APS5_VABI *)(std::uint32_t);
static void Require(bool value) { if (!value) std::abort(); }
static bool Throws(SetId function, std::uint32_t id) {
    try {
        function(id);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
int main() {
    *__error_nid_postfix() = 13;
    Require(seteuid_nid_postfix(0) == 0);
    Require(setegid_nid_postfix(0) == 0);
    Require(*__error_nid_postfix() == 13);
    Require(Throws(seteuid_nid_postfix, 1000));
    Require(Throws(setegid_nid_postfix, 1000));
    Require(Throws(seteuid_nid_postfix, 0xFFFFFFFFu));
    Require(Throws(setegid_nid_postfix, 0xFFFFFFFFu));
    Require(seteuid_nid_postfix(0) == 0);
    Require(setegid_nid_postfix(0) == 0);
}
