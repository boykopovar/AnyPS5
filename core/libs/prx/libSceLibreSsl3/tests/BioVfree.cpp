#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_vfree check failed at line %u\n", location.line());
    std::abort();
}

void CheckReleasesAReference() {
    auto* bio = BIO_new_mem_buf_nid_postfix("abc", 3);
    Require(bio != nullptr);
    Require(bio->references == 1);
    bio->references = 2;

    BIO_vfree_nid_postfix(bio);
    Require(bio->references == 1);

    BIO_vfree_nid_postfix(bio);
}

void CheckAcceptsNull() {
    BIO_vfree_nid_postfix(nullptr);
}

}

int main() {
    CheckAcceptsNull();
    CheckReleasesAReference();
    std::printf("libSceLibreSsl3 BIO_vfree tests passed\n");
    return 0;
}
