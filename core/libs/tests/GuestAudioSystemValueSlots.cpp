#include "prx/libSceAudioSystem/libSceAudioSystem.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
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

    Store(100);
    Require(Stored(out) == 100, "second stored value was not returned");
    Give(200);
    Require(Taken(out) == 200, "second pending value was not taken");
    Require(Taken(out) == 0, "second take did not clear pending value");
    errno = 71;
    Require(Stored(out) == 100, "second take altered stored value");
    Require(errno == 71, "mS9gpe47Eio preserved errno on success");
    errno = 71;
    Require(Taken(out) == 0, "pending read after clear with errno");
    Require(errno == 71, "FkFcs7NTFDo preserved errno on success");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { libSceAudioSystemUnknown_mS9gpe47Eio_nid_no_patch(reinterpret_cast<std::int32_t*>(0x10)); }, "mS9gpe47Eio threw on unmapped output");
    Require(errno == 71, "mS9gpe47Eio preserved errno on unmapped output");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { libSceAudioSystemUnknown_FkFcs7NTFDo_nid_no_patch(reinterpret_cast<std::int32_t*>(0x10)); }, "FkFcs7NTFDo threw on unmapped output");
    Require(errno == 71, "FkFcs7NTFDo preserved errno on unmapped output");

    std::puts("AudioSystem value slot tests passed");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
