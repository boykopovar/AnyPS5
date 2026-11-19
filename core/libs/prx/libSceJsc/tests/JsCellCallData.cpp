#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>

using Jsc::CellAbi::CallData;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSCell call data check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

bool IsNone(const CallData& data) {
    return data.type == 0 && !data.first && !data.second;
}

} // namespace

int main() {
    using Getter = CallData (APS5_VABI *)(void*);
    unsigned char cell[64] = {};
    for (const Getter getter : {_ZN3JSC6JSCell11getCallDataEPS0_, _ZN3JSC6JSCell16getConstructDataEPS0_}) {
        Require(IsNone(getter(cell)));
        Require(IsNone(getter(nullptr)));
        CallData data{2, cell, cell};
        data = getter(cell);
        Require(IsNone(data));
    }
    for (const unsigned char byte : cell) Require(byte == 0);
    std::puts("JSCell call data tests passed");
    return 0;
}
