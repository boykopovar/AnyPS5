#include "prx/libSceLibcInternal/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
int guestError;
int backendResult;
int backendError;
int calls;
const char* expectedPath;
const LibcUtimbuf* expectedTimes;

}

extern "C" int* APS5_VABI __error_nid_postfix() {
    return &guestError;
}

extern "C" bool GuestCopyIn_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    if (source == nullptr) return false;
    std::memcpy(destination, source, bytes);
    return true;
}

extern "C" int APS5_VABI utimes_nid_postfix(const char* path, const KernelTimeval* times) {
    ++calls;
    Require(path == expectedPath);
    Require((times == nullptr) == (expectedTimes == nullptr));
    if (expectedTimes) {
        Require(times[0].tv_sec == expectedTimes->actime && times[0].tv_usec == 0);
        Require(times[1].tv_sec == expectedTimes->modtime && times[1].tv_usec == 0);
    }
    guestError = backendError;
    return backendResult;
}

int main() {
    const char path[] = "/app0/timestamps";
    const LibcUtimbuf times{0x100000001LL, -123};
    for (const LibcUtimbuf* value : {&times, static_cast<const LibcUtimbuf*>(nullptr)}) {
        for (const char* name : {path, static_cast<const char*>(nullptr)}) {
            expectedPath = name;
            expectedTimes = value;
            for (int result : {0, -1}) {
                backendResult = result;
                backendError = result == 0 ? 22 : 14;
                guestError = 71;
                const int previousCalls = calls;
                Require(utime_nid_postfix(name, value) == result);
                Require(calls == previousCalls + 1);
                Require(guestError == (result == 0 ? 71 : 14));
            }
        }
    }
}
