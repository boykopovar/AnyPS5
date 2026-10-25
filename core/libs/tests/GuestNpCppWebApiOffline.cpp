#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using SelfCall = int (APS5_VABI*)(void*);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

static void TestIteratorDestructors() {
    unsigned char iterator[0x20];
    std::memset(iterator, 0x5a, sizeof(iterator));
    const SelfCall destructors[] = {
        _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEED2Ev,
        _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEED2Ev,
    };
    for (const auto call : destructors) Require(call(iterator) == 0, "an iterator destructor failed");
    for (const auto byte : iterator) Require(byte == 0x5a, "an iterator destructor wrote to the iterator");
}

int main() {
    TestIteratorDestructors();
    std::puts("NpCppWebApi offline tests passed");
    return 0;
}
