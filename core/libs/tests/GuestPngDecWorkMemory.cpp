#include "prx/libScePngDec/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: PNG work-memory assertion failed\n", location.file_name(), location.line());
    std::abort();
}

int main() {
    const PngDecCreateParam w1None{sizeof(PngDecCreateParam), 0, 1};
    Require(scePngDecQueryMemorySize(&w1None) == 53968);
    const PngDecCreateParam w1Sixteen{sizeof(PngDecCreateParam), 1, 1};
    Require(scePngDecQueryMemorySize(&w1Sixteen) == 54544);

    const PngDecCreateParam w7None{sizeof(PngDecCreateParam), 0, 7};
    Require(scePngDecQueryMemorySize(&w7None) == 53968);
    const PngDecCreateParam w7Sixteen{sizeof(PngDecCreateParam), 1, 7};
    Require(scePngDecQueryMemorySize(&w7Sixteen) == 54544);

    const PngDecCreateParam w8None{sizeof(PngDecCreateParam), 0, 8};
    Require(scePngDecQueryMemorySize(&w8None) == 53968);
    const PngDecCreateParam w8Sixteen{sizeof(PngDecCreateParam), 1, 8};
    Require(scePngDecQueryMemorySize(&w8Sixteen) == 54544);

    const PngDecCreateParam w9None{sizeof(PngDecCreateParam), 0, 9};
    Require(scePngDecQueryMemorySize(&w9None) == 54032);
    const PngDecCreateParam w9Sixteen{sizeof(PngDecCreateParam), 1, 9};
    Require(scePngDecQueryMemorySize(&w9Sixteen) == 54672);

    const PngDecCreateParam w16None{sizeof(PngDecCreateParam), 0, 16};
    Require(scePngDecQueryMemorySize(&w16None) == 54032);
    const PngDecCreateParam w16Sixteen{sizeof(PngDecCreateParam), 1, 16};
    Require(scePngDecQueryMemorySize(&w16Sixteen) == 54672);

    const PngDecCreateParam w17None{sizeof(PngDecCreateParam), 0, 17};
    Require(scePngDecQueryMemorySize(&w17None) == 54096);
    const PngDecCreateParam w17Sixteen{sizeof(PngDecCreateParam), 1, 17};
    Require(scePngDecQueryMemorySize(&w17Sixteen) == 54800);

    const PngDecCreateParam w1921None{sizeof(PngDecCreateParam), 0, 1921};
    Require(scePngDecQueryMemorySize(&w1921None) == 69328);
    const PngDecCreateParam w1921Sixteen{sizeof(PngDecCreateParam), 1, 1921};
    Require(scePngDecQueryMemorySize(&w1921Sixteen) == 85264);

    return 0;
}
