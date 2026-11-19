#include "prx/libSceJpegParser/libSceJpegParser.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "JpegParser: %s\n", message);
    std::abort();
}

}

int main() {
    errno = 71;
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "Finalize without Initialize did not return 0");
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "a second Finalize did not return 0");
    Require(errno == 71, "Finalize changed errno");
    std::puts("JpegParser tests passed");
    return 0;
}
