#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO field check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    LibreSsl::Bio bio{};
    int payload = 0;
    bio.data = &payload;
    Require(BIO_get_data_nid_postfix(&bio) == &payload);
    BIO_set_data_nid_postfix(&bio, nullptr);
    Require(bio.data == nullptr);
    BIO_set_data_nid_postfix(&bio, &payload);
    Require(bio.data == &payload);
}
