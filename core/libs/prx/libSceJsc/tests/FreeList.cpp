#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC::FreeList check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using Jsc::HeapAbi::FreeCell;
using Jsc::HeapAbi::FreeList;

FreeList Filled() {
    return {0x1111, 0x2222, reinterpret_cast<char*>(0x3333), 44, 55, 64};
}

template<typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestBump() {
    alignas(16) char payload[256]{};
    FreeList list = Filled();
    _ZN3JSC8FreeList14initializeBumpEPcj(&list, payload + 256, 192);
    Require(list.scrambledHead == 0);
    Require(list.secret == 0);
    Require(list.payloadEnd == payload + 256);
    Require(list.remaining == 192);
    Require(list.originalSize == 192);
    Require(list.cellSize == 64);

    list = Filled();
    _ZN3JSC8FreeList14initializeBumpEPcj(&list, nullptr, 0);
    Require(list.scrambledHead == 0 && list.secret == 0 && !list.payloadEnd);
    Require(list.remaining == 0 && list.originalSize == 0 && list.cellSize == 64);
}

void TestList() {
    alignas(16) unsigned char cells[64]{};
    auto* head = reinterpret_cast<FreeCell*>(cells);
    const std::uintptr_t secret = 0xA5A5F00DCAFEBEEFull;
    FreeList list = Filled();
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, head, secret, 4096);
    Require(list.scrambledHead == (reinterpret_cast<std::uintptr_t>(head) ^ secret));
    Require((list.scrambledHead ^ list.secret) == reinterpret_cast<std::uintptr_t>(head));
    Require(list.secret == secret);
    Require(!list.payloadEnd);
    Require(list.remaining == 0);
    Require(list.originalSize == 4096);
    Require(list.cellSize == 64);

    list = Filled();
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, nullptr, secret, 0);
    Require(list.scrambledHead == secret);
    Require((list.scrambledHead ^ list.secret) == 0);
    Require(list.originalSize == 0 && list.cellSize == 64);

    list = Filled();
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, head, 0, 32);
    Require(list.scrambledHead == reinterpret_cast<std::uintptr_t>(head));
    Require(list.secret == 0 && list.originalSize == 32);
}

void TestNull() {
    Require(Throws([] { _ZN3JSC8FreeList14initializeBumpEPcj(nullptr, nullptr, 1); }));
    Require(Throws([] { _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(nullptr, nullptr, 1, 1); }));
}

}  // namespace

int main() {
    TestBump();
    TestList();
    TestNull();
    std::puts("libSceJsc JSC::FreeList passed");
    return 0;
}
