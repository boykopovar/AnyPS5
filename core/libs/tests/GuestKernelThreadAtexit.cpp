#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"
#include <cstdint>
#include <cstdlib>
#include <vector>

int main() {
    int stackValue = 1;
    std::vector<std::uint8_t> heap(16);
    Require(sceKernelRtldThreadAtexitIncrement(nullptr) == -1);
    Require(sceKernelRtldThreadAtexitIncrement(&stackValue) == -1);
    Require(sceKernelRtldThreadAtexitIncrement(heap.data()) == -1);
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(nullptr) == -1);
    Require(_sceKernelRtldThreadAtexitIncrement_nid_postfix(&stackValue) == -1);

    Require(sceKernelRtldThreadAtexitDecrement(nullptr) == 0);
    Require(sceKernelRtldThreadAtexitDecrement(&stackValue) == 0);
    Require(sceKernelRtldThreadAtexitDecrement(heap.data()) == 0);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(nullptr) == 0);

    const void* moduleAddress = reinterpret_cast<const void*>(&sceKernelRtldThreadAtexitIncrement);
    Require(sceKernelRtldThreadAtexitIncrement(moduleAddress) == 0);
    Require(_sceKernelRtldThreadAtexitDecrement_nid_postfix(moduleAddress) == 0);
    Require(sceKernelRtldThreadAtexitDecrement(moduleAddress) == 0);
    return 0;
}
