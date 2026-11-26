#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <typeinfo>

#define FUNDAMENTAL_TYPES(X) X(v) X(w) X(b) X(c) X(a) X(h) X(s) X(t) X(i) X(j) X(l) X(m) X(x) X(y) X(n) X(o) X(f) X(d) X(e) X(g) X(Dn) X(Ds) X(Di) X(Du) X(Dh)
#define DECLARE_RECORDS(code) extern std::type_info _ZTI##code##_nid_postfix, _ZTIP##code##_nid_postfix, _ZTIPK##code##_nid_postfix;

extern "C" {
FUNDAMENTAL_TYPES(DECLARE_RECORDS)
}

namespace {

struct PointerRecord { const void* vtable; const char* name; unsigned flags; const std::type_info* pointee; };

struct Records {
    const char* code;
    const std::type_info* type;
    const std::type_info* pointer;
    const std::type_info* constPointer;
};

#define RECORDS(code) {#code, &_ZTI##code##_nid_postfix, &_ZTIP##code##_nid_postfix, &_ZTIPK##code##_nid_postfix},
const Records types[] = {FUNDAMENTAL_TYPES(RECORDS)};

void Require(bool condition, const char* code, int line) {
    if (condition) return;
    std::fprintf(stderr, "fundamental type_info check failed for %s at line %d\n", code, line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), entry.code, __LINE__)

const void* VtableOf(const std::type_info* type) { return *reinterpret_cast<const void* const*>(type); }

const PointerRecord& Pointer(const std::type_info* type) { return *reinterpret_cast<const PointerRecord*>(type); }

}

int main() {
    const void* fundamentalVtable = VtableOf(types[0].type);
    const void* pointerVtable = VtableOf(types[0].pointer);
    for (const auto& entry : types) {
        const std::string code = entry.code;
        REQUIRE(std::strcmp(entry.type->name(), code.c_str()) == 0);
        REQUIRE(std::strcmp(entry.pointer->name(), ("P" + code).c_str()) == 0);
        REQUIRE(std::strcmp(entry.constPointer->name(), ("PK" + code).c_str()) == 0);
        REQUIRE(VtableOf(entry.type) == fundamentalVtable);
        REQUIRE(VtableOf(entry.pointer) == pointerVtable && VtableOf(entry.constPointer) == pointerVtable);
        REQUIRE(Pointer(entry.pointer).flags == 0 && Pointer(entry.pointer).pointee == entry.type);
        REQUIRE(Pointer(entry.constPointer).flags == 1 && Pointer(entry.constPointer).pointee == entry.type);
    }
    const auto& entry = types[0];
    REQUIRE(fundamentalVtable != pointerVtable);
    std::puts("fundamental and pointer type_info records passed");
    return 0;
}
