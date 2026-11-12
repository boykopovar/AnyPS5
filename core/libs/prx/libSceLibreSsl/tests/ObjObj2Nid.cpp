#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void BuiltIn() {
    Require(OBJ_obj2nid_nid_postfix(nullptr) == 0, "a null object is NID_undef");
    const LibreSslAsn1Object commonName = LibreSslObjects_nid_no_patch[13];
    Require(OBJ_obj2nid_nid_postfix(&commonName) == 13, "a table entry object returns its NID");
    LibreSslAsn1Object searched = commonName;
    searched.nid = 0;
    Require(OBJ_obj2nid_nid_postfix(&searched) == 13, "a caller-built object with the entry data resolves through the table");
}

void CallerBuilt() {
    const unsigned char unknown[] = {0x2a, 0x03, 0x04};
    const LibreSslAsn1Object unmatched{nullptr, nullptr, 0, 3, unknown, 0};
    Require(OBJ_obj2nid_nid_postfix(&unmatched) == 0, "unknown data is NID_undef");
    const LibreSslAsn1Object preset{nullptr, nullptr, 5000, 3, unknown, 0};
    Require(OBJ_obj2nid_nid_postfix(&preset) == 5000, "a nonzero NID is returned as stored");
    const LibreSslAsn1Object missingData{nullptr, nullptr, 0, 3, nullptr, 0};
    Require(OBJ_obj2nid_nid_postfix(&missingData) == 0, "NID 0 with null data is NID_undef");
}

void Created() {
    const unsigned char data[] = {0x2a, 0x03, 0x04};
    const int added = OBJ_create_nid_postfix("1.2.3.4", "obj2nidAdded", "obj2nid added");
    Require(added == LibreSslObjectCount, "the first added object takes NID 956");
    const LibreSslAsn1Object object{nullptr, nullptr, 0, 3, data, 0};
    Require(OBJ_obj2nid_nid_postfix(&object) == added, "an OBJ_create object resolves by encoding");
    OBJ_cleanup_nid_postfix();
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "OBJ_cleanup removes the added object");
}

} // namespace

int main() {
    BuiltIn();
    CallerBuilt();
    Created();
}
