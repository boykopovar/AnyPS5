#include "prx/libSceIcu/Data.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/udata.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

int calls = 0;
UDataFileAccess observedAccess = UDATA_NO_FILES;
UErrorCode observedStatus = U_ZERO_ERROR;
UErrorCode replacement = U_ZERO_ERROR;
bool replaceStatus = false;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Set(UDataFileAccess access, std::int32_t* guestStatus) {
    const int previous = calls;
    const auto incoming = guestStatus ? static_cast<UErrorCode>(*guestStatus) : U_ZERO_ERROR;
    udata_setFileAccess_nid_postfix(access, guestStatus);
    Expect(calls == previous + 1 && observedAccess == access, "file access mode did not reach ICU");
    Expect(observedStatus == incoming, "ICU did not receive the incoming status");
    if (guestStatus) {
        const auto expected = replaceStatus && U_SUCCESS(incoming) ? replacement : incoming;
        Expect(*guestStatus == expected, "changed ICU status did not reach the guest or overwrote a prior failure");
    }
}

}

U_CAPI void U_EXPORT2 udata_setCommonData(const void*, UErrorCode*) {
    std::abort();
}

U_CAPI void U_EXPORT2 udata_setFileAccess(UDataFileAccess access, UErrorCode* status) {
    Expect(status != nullptr, "ICU received a null status pointer");
    ++calls;
    observedAccess = access;
    observedStatus = *status;
    if (replaceStatus) *status = replacement;
}

int main() {
    for (auto access : {UDATA_FILES_FIRST, UDATA_ONLY_PACKAGES, UDATA_PACKAGES_FIRST, UDATA_NO_FILES}) {
        for (bool replace : {false, true}) {
            replaceStatus = replace;
            for (auto after : {U_ZERO_ERROR, U_USING_DEFAULT_WARNING, U_INTERNAL_PROGRAM_ERROR}) {
                replacement = after;
                Set(access, nullptr);
                for (auto incoming : {U_ZERO_ERROR, U_USING_FALLBACK_WARNING, U_BUFFER_OVERFLOW_ERROR}) {
                    std::int32_t status = incoming;
                    Set(access, &status);
                }
            }
        }
    }
    const int previous = calls;
    static const std::int32_t readOnly = U_ZERO_ERROR;
    auto* readOnlyStatus = const_cast<std::int32_t*>(&readOnly);
    Expect(!GuestWritable_nid_no_patch(readOnlyStatus, sizeof(readOnly)), "read-only fixture is writable");
    udata_setFileAccess_nid_postfix(UDATA_FILES_FIRST, readOnlyStatus);
    udata_setFileAccess_nid_postfix(UDATA_FILES_FIRST, reinterpret_cast<std::int32_t*>(std::uintptr_t{1}));
    Expect(calls == previous, "unwritable guest status changed file access");
}
