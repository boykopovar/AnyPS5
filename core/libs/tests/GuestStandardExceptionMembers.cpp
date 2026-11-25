#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using Object = LibcException::ExceptionObject;

struct Members {
    const char* name;
    void (APS5_VABI* construct1)(Object*, const char*);
    void (APS5_VABI* construct2)(Object*, const char*);
    void (APS5_VABI* copy1)(Object*, const Object*);
    void (APS5_VABI* copy2)(Object*, const Object*);
    Object* (APS5_VABI* assign)(Object*, const Object*);
    const char* (APS5_VABI* what)(const Object*);
    void (APS5_VABI* destroy1)(Object*);
    void (APS5_VABI* destroy2)(Object*);
    void (APS5_VABI* destroyDeleting)(Object*);
};

void Require(bool condition, const char* name, int line) {
    if (condition) return;
    std::fprintf(stderr, "standard exception member check failed for %s at line %d\n", name, line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), members.name, __LINE__)

void Check(const Members& members) {
    char text[] = "first message";
    Object first{}, second{}, firstCopy{}, secondCopy{};
    members.construct1(&first, text);
    text[0] = 'X';
    REQUIRE(std::strcmp(members.what(&first), "first message") == 0);
    members.construct2(&second, "second message");
    REQUIRE(std::strcmp(members.what(&second), "second message") == 0);
    REQUIRE(second.vtable == first.vtable);

    members.copy1(&firstCopy, &first);
    REQUIRE(firstCopy.vtable == first.vtable && std::strcmp(members.what(&firstCopy), "first message") == 0);
    members.copy2(&secondCopy, &second);
    REQUIRE(secondCopy.vtable == first.vtable && std::strcmp(members.what(&secondCopy), "second message") == 0);

    REQUIRE(members.assign(&firstCopy, &second) == &firstCopy);
    REQUIRE(std::strcmp(members.what(&firstCopy), "second message") == 0);
    REQUIRE(std::strcmp(members.what(&second), "second message") == 0);
    REQUIRE(members.assign(&secondCopy, &secondCopy) == &secondCopy && std::strcmp(members.what(&secondCopy), "second message") == 0);

    members.destroy1(&first);
    REQUIRE(std::strcmp(members.what(&firstCopy), "second message") == 0);
    members.destroy2(&second);
    REQUIRE(std::strcmp(members.what(&secondCopy), "second message") == 0);
    members.destroy1(&firstCopy);
    members.destroy2(&secondCopy);

    auto* allocated = static_cast<Object*>(std::malloc(sizeof(Object)));
    REQUIRE(allocated != nullptr);
    members.construct1(allocated, "deleted message");
    REQUIRE(std::strcmp(members.what(allocated), "deleted message") == 0);
    members.destroyDeleting(allocated);
}

#define MEMBERS(type) {#type, \
    _ZNSt##type##C1EPKc_nid_postfix, _ZNSt##type##C2EPKc_nid_postfix, \
    _ZNSt##type##C1ERKS__nid_postfix, _ZNSt##type##C2ERKS__nid_postfix, \
    _ZNSt##type##aSERKS__nid_postfix, _ZNKSt##type##4whatEv_nid_postfix, \
    _ZNSt##type##D1Ev_nid_postfix, _ZNSt##type##D2Ev_nid_postfix, _ZNSt##type##D0Ev_nid_postfix}

}

int main() {
    const Members classes[] = {
        MEMBERS(11logic_error),
        MEMBERS(12domain_error),
        MEMBERS(12length_error),
        MEMBERS(12out_of_range),
        MEMBERS(16invalid_argument),
        MEMBERS(13runtime_error),
    };
    for (const auto& members : classes) Check(members);
    std::puts("standard exception constructors, copies, assignment, what() and destructors passed");
    return 0;
}
