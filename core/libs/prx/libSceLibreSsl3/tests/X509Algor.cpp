#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_ALGOR_get0 check failed at line %u\n", location.line());
    std::abort();
}

int object = 0;
int sentinel = 0;

}

int main() {
    char octets[] = "params";
    LibreSsl3Asn1Type parameter{4, octets};
    LibreSsl3X509Algor algor{&object, &parameter};

    const void* outObject = &sentinel;
    int outType = 99;
    const void* outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, &algor);
    Require(outObject == &object && outType == 4 && outValue == octets);

    LibreSsl3X509Algor bare{&object, nullptr};
    outObject = &sentinel;
    outType = 99;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, &bare);
    Require(outObject == &object && outType == LibreSsl3Asn1Undefined && outValue == nullptr);

    outObject = &sentinel;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, nullptr, &outValue, &algor);
    Require(outObject == &object && outValue == nullptr);

    outType = 99;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(nullptr, &outType, &outValue, &algor);
    Require(outType == 4 && outValue == octets);

    outType = 99;
    X509_ALGOR_get0_nid_postfix(nullptr, &outType, nullptr, &algor);
    Require(outType == 4);

    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(nullptr, nullptr, &outValue, nullptr);
    Require(outValue == nullptr);
    X509_ALGOR_get0_nid_postfix(nullptr, nullptr, nullptr, nullptr);
}
