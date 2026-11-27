#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::HandleAbi {

struct HandleNode {
    std::uint64_t value;
    HandleNode* prev;
    HandleNode* next;
};

struct HandleNodeList {
    HandleNode headSentinel;
    HandleNode tailSentinel;
};

struct HandleSet {
    void* vm;
    void* blockListHead;
    void* blockListTail;
    HandleNodeList strongList;
    HandleNodeList immediateList;
};

static_assert(sizeof(HandleNode) == 24);
static_assert(offsetof(HandleNode, prev) == 8);
static_assert(offsetof(HandleNode, next) == 16);
static_assert(offsetof(HandleSet, strongList) == 24);
static_assert(offsetof(HandleSet, immediateList) == 72);

} // namespace Jsc::HandleAbi

extern "C" {

void APS5_VABI _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(Jsc::HandleAbi::HandleSet* self, std::uint64_t* slot, const std::uint64_t* value);

}
