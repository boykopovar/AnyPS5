#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::AutomaticThread check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

int main() {
    alignas(16) unsigned char thread[256] = {};
    const char* name = _ZNK3WTF15AutomaticThread4nameEv(thread);
    Require(name != nullptr);
    Require(std::strcmp(name, "WTF::AutomaticThread") == 0);
    Require(_ZNK3WTF15AutomaticThread4nameEv(thread + 128) == name);
    bool threw = false;
    try {
        _ZNK3WTF15AutomaticThread4nameEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    std::puts("WTF::AutomaticThread::name tests passed");
    return 0;
}
