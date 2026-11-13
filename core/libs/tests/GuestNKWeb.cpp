#include "prx/libSceNKWeb/libSceNKWeb.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NKWeb check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TException, typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

int main() {
    Require(sceNKWebInitialize() == 0);
    Require(sceNKWebInitialize() == 0);
    Require(sceNKWebTerminate() == 0);
    Require(sceNKWebTerminate() == 0);
    Require(sceNKWebInitialize() == 0);
    Require(sceNKWebTerminate() == 0);

    Require(Throws<std::runtime_error>([] {
        libSceNKWebUnknown_HTKKQjsQMiQ_nid_no_patch();
    }));
    Require(Throws<std::runtime_error>([] {
        libSceNKWebUnknown_HvqTa7FlgMI_nid_no_patch();
    }));
    Require(Throws<std::runtime_error>([] {
        libSceNKWebUnknown_q_plus_Emt1gYris_nid_no_patch();
    }));

    return 0;
}
