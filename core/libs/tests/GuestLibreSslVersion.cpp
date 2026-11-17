#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    Require(SSLeay_nid_postfix() == std::uint64_t{0x20000000});
    Require(SSLeay_nid_postfix() == SSLeay_nid_postfix());
    Require(SSL_library_init_nid_postfix() == 1);
    Require(SSL_library_init_nid_postfix() == 1);
    return 0;
}
