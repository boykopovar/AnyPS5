#include "prx/libSceAudioSystem/libSceAudioSystem.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AUDIOSYSTEM VALUE SLOTS: %s\n", message);
    std::abort();
}

std::int32_t Read(const std::int32_t* value) {
    std::int32_t result = 0;
    std::memcpy(&result, value, sizeof(result));
    return result;
}

std::int32_t Stored(std::int32_t* out) {
    Require(libSceAudioSystemUnknown_mS9gpe47Eio_nid_no_patch(out) == 0, "mS9gpe47Eio result");
    return Read(out);
}

std::int32_t Taken(std::int32_t* out) {
    Require(libSceAudioSystemUnknown_FkFcs7NTFDo_nid_no_patch(out) == 0, "FkFcs7NTFDo result");
    return Read(out);
}

void Give(std::int32_t value) {
    Require(libSceAudioSystemUnknown_ds20PmJfSSk_nid_no_patch(value) == 0, "ds20PmJfSSk result");
}

void Store(std::int32_t value) {
    Require(libSceAudioSystemUnknown_uoPBJTVWhrg_nid_no_patch(value) == 0, "uoPBJTVWhrg result");
}

}

int main() {
    auto* out = static_cast<std::int32_t*>(GuestHeap::GuestHeapAllocate_nid_postfix(sizeof(std::int32_t)));
    Require(out != nullptr, "guest allocation failed");

    Require(Stored(out) == -1, "stored value does not start at -1");
    for (const std::int32_t value : {0, 7, -2, 0x7fffffff, static_cast<std::int32_t>(0x80000000)}) {
        Store(value);
        Require(Stored(out) == value, "stored value was not returned");
        Require(Stored(out) == value, "reading the stored value changed it");
    }

    Require(Taken(out) == 0, "pending value does not start at 0");
    Give(5);
    Give(9);
    Require(Taken(out) == 5, "a second value replaced a pending one");
    Require(Taken(out) == 0, "taking the pending value did not clear it");
    Give(-3);
    Require(Taken(out) == -3, "pending value after a clear was not taken");
    Give(0);
    Give(11);
    Require(Taken(out) == 11, "a zero value blocked the next one");
    Require(Stored(out) == static_cast<std::int32_t>(0x80000000), "the pending value changed the stored one");

    std::puts("AudioSystem value slot tests passed");
    return 0;
}
