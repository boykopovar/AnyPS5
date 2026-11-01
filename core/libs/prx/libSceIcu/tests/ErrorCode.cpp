// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdlib>
#include <cstring>

namespace {
int calls;
void Check(bool value) { if (!value) std::abort(); }
void APS5_VABI Failure(const Icu::Abi::ErrorCode*) { ++calls; }
}
int main() {
    using namespace Icu::Abi;
    ErrorCode object{};
    _ZN6icu_689ErrorCodeD2Ev_nid_postfix(&object);
    object.errorCode = 7;
    Check(std::strcmp(_ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(&object), "U_MEMORY_ALLOCATION_ERROR") == 0);
    Check(_ZN6icu_689ErrorCode5resetEv_nid_postfix(&object) == 7 && object.errorCode == 0);
    std::array<GuestFunction, 3> table{};
    std::memcpy(table.data(), object.vtable, sizeof table);
    table[2] = reinterpret_cast<GuestFunction>(Failure);
    object.vtable = table.data();
    _ZNK6icu_689ErrorCode13assertSuccessEv_nid_postfix(&object);
    Check(calls == 0);
    object.errorCode = -128;
    _ZNK6icu_689ErrorCode13assertSuccessEv_nid_postfix(&object);
    Check(calls == 0);
    object.errorCode = 1;
    _ZNK6icu_689ErrorCode13assertSuccessEv_nid_postfix(&object);
    Check(calls == 1);
    object.errorCode = -999;
    Check(std::strcmp(_ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(&object), "[BOGUS UErrorCode]") == 0);
    _ZN6icu_689ErrorCodeD1Ev_nid_postfix(&object);
    return 0;
}
