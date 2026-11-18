#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstring>

namespace {
constexpr char ownerName[] = "ANYPS5_RETAINED_OWNER_6F81";
constexpr char innerName[] = "ANYPS5_RETAINED_INNER_6F81";
constexpr char originalValue[] = "ANYPS5_RETAINED_INNER_6F81=retained";

const char* RetainedBorrow(int* guestError) {
    if (setenv_nid_postfix(ownerName, originalValue, 1) != 0) return "owner setup failed";
    char* const retained = getenv_nid_postfix(ownerName);
    if (retained == nullptr) return "owner value is missing";
    if (std::memcmp(retained, originalValue, sizeof(originalValue)) != 0) return "owner has the wrong original value";
    if (putenv_nid_postfix(retained) != 0) return "interior putenv failed";
    char* const inner = getenv_nid_postfix(innerName);
    if (inner != retained + sizeof(innerName)) return "putenv copied its input";
    if (std::strcmp(inner, "retained") != 0) return "interior value is incorrect";
    if (unsetenv_nid_postfix(innerName) != 0) return "interior removal failed";
    if (getenv_nid_postfix(innerName) != nullptr) return "interior name remains after removal";
    if (getenv_nid_postfix(ownerName) != retained) return "interior removal changed the owner pointer";
    if (std::memcmp(retained, originalValue, sizeof(originalValue)) != 0) return "interior removal changed the owner value";
    *guestError = 73;
    const int result = setenv_nid_postfix(ownerName, "short", 1);
    const int error = *guestError;
    if (result != 0) return "owner overwrite failed";
    if (error != 73) return "owner overwrite changed errno";
    char* const current = getenv_nid_postfix(ownerName);
    if (current == nullptr) return "overwritten owner is missing";
    if (std::strcmp(current, "short") != 0) return "overwritten owner has the wrong value";
    if (std::memcmp(retained, originalValue, sizeof(originalValue)) != 0)
        return "retained putenv storage changed after alias removal";
    if (current == retained) return "owner overwrite reused retained putenv storage";
    if (getenv_nid_postfix(innerName) != nullptr) return "owner overwrite restored the removed interior name";
    return nullptr;
}
}

int main() {
    int* const guestError = __error_nid_postfix();
    const int savedError = *guestError;
    char** storage = nullptr;
    BindGuestEnvironment_nid_no_patch(&storage);
    char** const initial = storage;
    const bool empty = initial != nullptr && initial[0] == nullptr;
    const char* failure = empty ? RetainedBorrow(guestError) : "initial guest environment is not empty";
    if (empty) {
        const int innerResult = unsetenv_nid_postfix(innerName);
        const int ownerResult = unsetenv_nid_postfix(ownerName);
        if (failure == nullptr && (innerResult != 0 || ownerResult != 0)) failure = "environment cleanup failed";
        storage = initial;
    }
    BindGuestEnvironment_nid_no_patch(nullptr);
    if (failure != nullptr) std::fprintf(stderr, "guest environment retained borrow: %s\n", failure);
    else std::puts("guest environment retained borrow checks passed");
    *guestError = savedError;
    return failure == nullptr ? 0 : 1;
}
