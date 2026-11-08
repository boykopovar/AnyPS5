#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>

int main() {
    ERR_load_X509_strings_nid_postfix();
    ERR_load_PEM_strings_nid_postfix();
    OpenSSL_add_all_digests_nid_postfix();
    std::puts("libSceLibreSsl3 error-string loader tests passed");
}
