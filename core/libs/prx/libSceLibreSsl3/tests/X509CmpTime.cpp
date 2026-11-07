#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_cmp_current_time check failed at line %u\n", location.line());
    std::abort();
}

int Compare(std::string_view text, int type) {
    Asn1String time{static_cast<int>(text.size()), type,
        reinterpret_cast<unsigned char*>(const_cast<char*>(text.data())), 0};
    return X509_cmp_current_time_nid_postfix(&time);
}

}

int main() {
    Require(Compare("200101000000Z", 23) == -1);
    Require(Compare("500101000000Z", 23) == -1);
    Require(Compare("200229120000Z", 23) == -1);
    Require(Compare("200101000000Z", 0) == -1);
    Require(Compare("491231235959Z", 23) == 1);
    Require(Compare("20500101000000Z", 24) == 1);
    Require(Compare("99991231235959Z", 0) == 1);
    Require(Compare("20200101000000Z", 24) == 0);
    Require(Compare("200101000000Z", 24) == 0);
    Require(Compare("20500101000000Z", 23) == 0);
    Require(Compare("200101000000Z", 4) == 0);
    Require(Compare("201301000000Z", 23) == 0);
    Require(Compare("200001000000Z", 23) == 0);
    Require(Compare("210229000000Z", 23) == 0);
    Require(Compare("200431000000Z", 23) == 0);
    Require(Compare("200101240000Z", 23) == 0);
    Require(Compare("200101006000Z", 23) == 0);
    Require(Compare("200101000060Z", 23) == 0);
    Require(Compare("200101000000X", 23) == 0);
    Require(Compare("2001010000Z", 23) == 0);
    Require(Compare("20010100000Z", 23) == 0);
    Require(Compare("20010100000aZ", 23) == 0);
    Require(Compare("2001010000+0Z", 23) == 0);
    Require(Compare("205001010000000Z", 24) == 0);
    Require(X509_cmp_current_time_nid_postfix(nullptr) == 0);
    Asn1String empty{13, 23, nullptr, 0};
    Require(X509_cmp_current_time_nid_postfix(&empty) == 0);
}
