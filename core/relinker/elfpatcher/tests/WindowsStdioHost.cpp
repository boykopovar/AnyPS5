#include <cstdio>

extern "C" int shared() {
    std::fputs("Windows DLL stdout probe\n", stdout);
    std::fputs("Windows DLL stderr probe\n", stderr);
    std::fflush(stdout);
    std::fflush(stderr);
    return 11;
}
