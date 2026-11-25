#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using Object = LibcException::ExceptionObject;

struct Members {
    const char* name;
    const char* what;
    void (APS5_VABI* construct1)(Object*);
    void (APS5_VABI* construct2)(Object*);
    const char* (APS5_VABI* whatCall)(const Object*);
    void (APS5_VABI* destroy1)(Object*);
    void (APS5_VABI* destroy2)(Object*);
    void (APS5_VABI* destroyDeleting)(Object*);
};

void Require(bool condition, const char* name, int line) {
    if (condition) return;
    std::fprintf(stderr, "plain standard exception check failed for %s at line %d\n", name, line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), members.name, __LINE__)

void Check(const Members& members) {
    Object first{nullptr, nullptr};
    Object second{nullptr, nullptr};
    members.construct1(&first);
    members.construct2(&second);
    REQUIRE(first.vtable != nullptr && second.vtable == first.vtable);
    REQUIRE(std::strcmp(members.whatCall(&first), members.what) == 0);
    REQUIRE(std::strcmp(members.whatCall(&second), members.what) == 0);
    members.destroy1(&first);
    members.destroy2(&second);

    auto* allocated = static_cast<Object*>(std::malloc(sizeof(Object)));
    REQUIRE(allocated != nullptr);
    members.construct1(allocated);
    REQUIRE(allocated->vtable == first.vtable && std::strcmp(members.whatCall(allocated), members.what) == 0);
    members.destroyDeleting(allocated);
}

#define MEMBERS(type, what) {#type, what, \
    _ZNSt##type##C1Ev_nid_postfix, _ZNSt##type##C2Ev_nid_postfix, _ZNKSt##type##4whatEv_nid_postfix, \
    _ZNSt##type##D1Ev_nid_postfix, _ZNSt##type##D2Ev_nid_postfix, _ZNSt##type##D0Ev_nid_postfix}

}

int main() {
    const Members classes[] = {
        MEMBERS(9exception, "std::exception"),
        MEMBERS(8bad_cast, "std::bad_cast"),
        MEMBERS(10bad_typeid, "std::bad_typeid"),
        MEMBERS(9bad_alloc, "bad allocation"),
        MEMBERS(20bad_array_new_length, "bad allocation"),
        MEMBERS(17bad_function_call, "bad function call"),
        MEMBERS(19bad_optional_access, "bad_optional_access"),
    };
    const void* vtables[sizeof(classes) / sizeof(classes[0])]{};
    for (std::size_t index = 0; index < sizeof(classes) / sizeof(classes[0]); ++index) {
        const auto& members = classes[index];
        Check(members);
        Object probe{nullptr, nullptr};
        members.construct1(&probe);
        for (std::size_t other = 0; other < index; ++other) REQUIRE(vtables[other] != probe.vtable);
        vtables[index] = probe.vtable;
        members.destroy1(&probe);
    }
    std::puts("plain standard exception constructors, what() and destructors passed");
    return 0;
}
