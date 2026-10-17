#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpTusCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId);
int APS5_VABI sceNpTusCreateRequest(int titleCtxId);
int APS5_VABI sceNpTusDeleteRequest(int reqId);
int APS5_VABI sceNpTusAbortRequest(int reqId);
int APS5_VABI sceNpTusPollAsync(int reqId, int* result);
int APS5_VABI sceNpTusGetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusSetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusAddAndGetVariableAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
}

static constexpr int SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int ABORTED = static_cast<int>(0x80550707);
static constexpr int INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int TOO_MANY_SLOTID = static_cast<int>(0x80550718);

static void Require(bool value) { if (!value) std::abort(); }

static void RequireFinished(int reqId, int expected) {
    int result = 0;
    Require(sceNpTusPollAsync(reqId, &result) == 0 && result == expected);
}

int main() {
    const int context = sceNpTusCreateNpTitleCtxA(0, 1);
    Require(context > 0);
    Require(sceNpTusCreateRequest(context + 1000) == INVALID_ID);

    const int request = sceNpTusCreateRequest(context);
    Require(request > 0);
    int result = 0;
    Require(sceNpTusPollAsync(request, &result) == 1);

    std::int32_t slots[2] = {0, 3};
    std::int64_t values[2] = {5, 7};
    std::uint8_t variables[256] = {};
    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, nullptr, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, slots, nullptr, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, slots, variables, sizeof(variables), 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, slots, variables, sizeof(variables), 2, variables) == INVALID_ARGUMENT);
    std::int32_t negative[1] = {-1};
    Require(sceNpTusDeleteMultiSlotVariableAAsync(request, 0, negative, 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableAAsync(request, 0, slots, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusAddAndGetVariableAAsync(request, 0, 0, 1, nullptr, nullptr, nullptr, 0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusAddAndGetVariableAAsync(request, 0, -1, 1, nullptr, nullptr, variables, sizeof(variables), nullptr) == INVALID_ARGUMENT);

    Require(sceNpTusGetMultiSlotVariableAAsync(request, 0, slots, variables, sizeof(variables), 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusSetMultiSlotVariableAAsync(request, 0, slots, values, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusDeleteMultiSlotVariableAAsync(request, 0, slots, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusAddAndGetVariableAAsync(request, 0, 0, 1, nullptr, nullptr, variables, sizeof(variables), nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);

    Require(sceNpTusAbortRequest(request) == 0);
    Require(sceNpTusSetMultiSlotVariableAAsync(request, 0, slots, values, 2, nullptr) == 0);
    RequireFinished(request, ABORTED);

    Require(sceNpTusDeleteRequest(request) == 0);
    Require(sceNpTusDeleteRequest(request) == INVALID_ID);
    Require(sceNpTusAbortRequest(request) == INVALID_ID);
    Require(sceNpTusPollAsync(request, &result) == INVALID_ID);
    Require(sceNpTusSetMultiSlotVariableAAsync(request, 0, slots, values, 2, nullptr) == INVALID_ID);
}
