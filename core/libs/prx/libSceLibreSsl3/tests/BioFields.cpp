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
    BIO_set_init_nid_postfix(&bio, 1);
    Require(bio.initialized == 1);
    BIO_set_init_nid_postfix(&bio, 0);
    Require(bio.initialized == 0 && bio.data == &payload);
    bio.shutdown = 1;
    Require(BIO_get_shutdown_nid_postfix(&bio) == 1);
    bio.shutdown = 0;
    Require(BIO_get_shutdown_nid_postfix(&bio) == 0);
    BIO_set_shutdown_nid_postfix(&bio, 1);
    Require(bio.shutdown == 1 && bio.initialized == 0);
    BIO_set_shutdown_nid_postfix(&bio, 0);
    Require(bio.shutdown == 0);
    bio.flags = 0x1;
    BIO_set_flags_nid_postfix(&bio, 0x6);
    Require(bio.flags == 0x7);
    BIO_clear_flags_nid_postfix(&bio, 0x3);
    Require(bio.flags == 0x4);
    BIO_clear_flags_nid_postfix(&bio, 0x8);
    Require(bio.flags == 0x4);
}
