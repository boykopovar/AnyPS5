#include "prx/libSceGvMp4Parser/libSceGvMp4Parser.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GvMp4Parser: %s\n", message);
    std::abort();
}

}

int main() {
    errno = 71;
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "Finalize without Initialize did not return 0");
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "a second Finalize did not return 0");
    Require(errno == 71, "Finalize changed errno");
    std::puts("GvMp4Parser tests passed");
    return 0;
}
