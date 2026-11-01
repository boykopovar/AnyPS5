#include "prx/libSceFios2/include/Api.hpp"
#include <cstdlib>
#include <cstdio>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Fios2 bounds check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
    Require(sceFiosArchiveGetMountBufferSize() == 0);
    Require(sceFiosArchiveGetMountBufferSizeSync() == 0);
    Require(sceFiosArchiveMount() == 0);
    Require(sceFiosArchiveMountSync() == 0);
    Require(sceFiosArchiveMountWithOrder() == 0);
    Require(sceFiosArchiveMountWithOrderSync() == 0);
    Require(sceFiosArchiveSetDecompressorThreadCount() == 0);
    Require(sceFiosArchiveUnmount() == 0);
    Require(sceFiosArchiveUnmountSync() == 0);
    return 0;
}
