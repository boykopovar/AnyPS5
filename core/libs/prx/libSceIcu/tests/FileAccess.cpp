#include "prx/libSceIcu/Data.hpp"
#include "unicode/udata.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

int calls = 0;
UDataFileAccess observedAccess = UDATA_NO_FILES;
bool receivedStatus = false;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Set(UDataFileAccess access, std::int32_t* guestStatus) {
    const int previous = calls;
    udata_setFileAccess_nid_postfix(access, guestStatus);
    Expect(calls == previous + 1 && observedAccess == access, "file access mode did not reach ICU");
    Expect(receivedStatus, "ICU received a null status pointer");
}

}

U_CAPI void U_EXPORT2 udata_setCommonData(const void*, UErrorCode*) {
    std::abort();
}

U_CAPI void U_EXPORT2 udata_setFileAccess(UDataFileAccess access, UErrorCode* status) {
    ++calls;
    observedAccess = access;
    receivedStatus = status != nullptr;
    if (status) *status = U_INTERNAL_PROGRAM_ERROR;
}

int main() {
    for (auto access : {UDATA_FILES_FIRST, UDATA_ONLY_PACKAGES, UDATA_PACKAGES_FIRST, UDATA_NO_FILES}) {
        Set(access, nullptr);
        for (std::int32_t incoming : {-128, 0, 15}) {
            auto status = incoming;
            Set(access, &status);
            Expect(status == incoming, "backend status escaped into guest storage");
        }
    }
    const int previous = calls;
    udata_setFileAccess_nid_postfix(UDATA_FILES_FIRST, reinterpret_cast<std::int32_t*>(std::uintptr_t{1}));
    Expect(calls == previous, "unwritable guest status changed file access");
}
