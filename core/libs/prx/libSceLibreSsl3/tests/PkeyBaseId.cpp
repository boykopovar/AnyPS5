#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY_base_id check failed at line %u\n", location.line());
    std::abort();
}

int BaseId(std::int32_t type) {
    LibreSsl3EvpPkey key{};
    key.type = type;
    key.references = 1;
    return EVP_PKEY_base_id_nid_postfix(&key);
}

}

int main() {
    Require(BaseId(6) == 6);
    Require(BaseId(19) == 6);
    Require(BaseId(912) == 912);
    Require(BaseId(116) == 116);
    Require(BaseId(67) == 116);
    Require(BaseId(66) == 116);
    Require(BaseId(113) == 116);
    Require(BaseId(70) == 116);
    Require(BaseId(28) == 28);
    Require(BaseId(408) == 408);
    Require(BaseId(855) == 855);
    Require(BaseId(894) == 894);
    Require(BaseId(950) == 950);
    Require(BaseId(952) == 952);
    Require(BaseId(0) == 0);
    Require(BaseId(811) == 0);
    Require(BaseId(-1) == 0);
}
