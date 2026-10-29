#include "prx/libSceLibcInternal/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {
int guestError;
int backendResult;
int backendError;
int calls;
const char* expectedPath;
const LibcUtimbuf* expectedTimes;

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Utime forwarding check failed at line %d\n", line);
        std::abort();
    }
}
}

#define Check(value) Require((value), __LINE__)

extern "C" int* APS5_VABI __error_nid_postfix() {
    return &guestError;
}

extern "C" int APS5_VABI utimes_nid_postfix(const char* path, const KernelTimeval* times) {
    ++calls;
    Check(path == expectedPath);
    Check((times == nullptr) == (expectedTimes == nullptr));
    if (expectedTimes) {
        Check(times[0].tv_sec == expectedTimes->actime && times[0].tv_usec == 0);
        Check(times[1].tv_sec == expectedTimes->modtime && times[1].tv_usec == 0);
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
                Check(utime_nid_postfix(name, value) == result);
                Check(calls == previousCalls + 1);
                Check(guestError == (result == 0 ? 71 : 14));
            }
        }
    }
}
