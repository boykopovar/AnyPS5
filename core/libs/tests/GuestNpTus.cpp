#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceNpTusCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId);
int APS5_VABI sceNpTusDeleteNpTitleCtx(int titleCtxId);
int APS5_VABI sceNpTssCreateNpTitleCtx(std::uint32_t serviceLabel, const NpId* npId);
int APS5_VABI sceNpTssCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId);
int APS5_VABI sceNpTusCreateRequest(int titleCtxId);
int APS5_VABI sceNpTusDeleteRequest(int reqId);
int APS5_VABI sceNpTusAbortRequest(int reqId);
int APS5_VABI sceNpTusPollAsync(int reqId, int* result);
int APS5_VABI sceNpTusGetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusSetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusAddAndGetVariableAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusWaitAsync(int reqId, int* result);
int APS5_VABI sceNpTusGetMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSave(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusSetMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option);
int APS5_VABI sceNpTusSetMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option);
int APS5_VABI sceNpTusSetMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariable(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, int arrayLen, void* option);
}

static constexpr int SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int TOO_MANY_OBJECTS = static_cast<int>(0x80550706);
static constexpr int ABORTED = static_cast<int>(0x80550707);
static constexpr int INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int TOO_MANY_SLOTID = static_cast<int>(0x80550718);

static void Require(bool value) { if (!value) std::abort(); }

static void RequireFinished(int reqId, int expected) {
    int result = 0;
    Require(sceNpTusPollAsync(reqId, &result) == 0 && result == expected);
}

static void CheckTssTitleContexts() {
    const int shared = sceNpTssCreateNpTitleCtxA(0, 1);
    Require(shared > 0);
    const int request = sceNpTusCreateRequest(shared);
    Require(request > 0);
    Require(sceNpTusDeleteNpTitleCtx(shared) == 0);
    Require(sceNpTusDeleteNpTitleCtx(shared) == INVALID_ID);
    Require(sceNpTusDeleteRequest(request) == INVALID_ID);

    const NpId npId{};
    Require(sceNpTssCreateNpTitleCtx(0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTssCreateNpTitleCtx(0xFFFFFFFFu, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTssCreateNpTitleCtx(0xFFFFFFFFu, &npId) == INVALID_ARGUMENT);

    std::array<int, 32> contexts{};
    for (std::size_t index = 0; index < contexts.size(); ++index) {
        const std::size_t form = index % 3;
        contexts[index] = form == 0 ? sceNpTusCreateNpTitleCtxA(0, 1) : form == 1 ? sceNpTssCreateNpTitleCtxA(0, 1) : sceNpTssCreateNpTitleCtx(0, &npId);
        Require(contexts[index] > 0);
    }
    Require(sceNpTssCreateNpTitleCtxA(0, 1) == TOO_MANY_OBJECTS);
    Require(sceNpTssCreateNpTitleCtx(0, &npId) == TOO_MANY_OBJECTS);
    Require(sceNpTusCreateNpTitleCtxA(0, 1) == TOO_MANY_OBJECTS);
    Require(sceNpTssCreateNpTitleCtx(0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTssCreateNpTitleCtx(0xFFFFFFFFu, &npId) == INVALID_ARGUMENT);
    for (const int context : contexts) Require(sceNpTusDeleteNpTitleCtx(context) == 0);
}

static void CheckMultiSlotVariables() {
    const int context = sceNpTusCreateNpTitleCtxA(0, 1);
    Require(context > 0);
    const int request = sceNpTusCreateRequest(context);
    Require(request > 0);

    int result = 0;
    Require(sceNpTusWaitAsync(request + 1000, &result) == INVALID_ID);
    Require(sceNpTusWaitAsync(request, &result) == INVALID_ARGUMENT);

    std::int32_t slots[2] = {0, 3};
    std::int32_t negative[1] = {-1};
    std::int64_t values[2] = {5, 7};
    std::uint8_t variables[512];
    for (std::uint8_t& byte : variables) byte = 0x5a;
    const NpId npId{};
    const NpOnlineId user{{'u', 's', 'e', 'r'}, {}};
    const NpOnlineId empty{};

    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, nullptr, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, slots, nullptr, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, slots, variables, sizeof(variables), 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, negative, variables, sizeof(variables), 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, slots, variables, sizeof(variables), 2, variables) == INVALID_ARGUMENT);
    Require(sceNpTusWaitAsync(request, &result) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableAsync(request, nullptr, slots, variables, sizeof(variables), 2, nullptr) == 0);
    result = 0;
    Require(sceNpTusWaitAsync(request, &result) == 0 && result == SIGNED_OUT);
    Require(sceNpTusWaitAsync(request, nullptr) == 0);
    Require(sceNpTusGetMultiSlotVariableAsync(request, &npId, slots, variables, sizeof(variables), 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);

    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, nullptr, slots, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, slots, nullptr, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, nullptr, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, slots, variables, sizeof(variables), 0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, slots, variables, sizeof(variables), 2, variables) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, negative, variables, sizeof(variables), 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &empty, slots, variables, sizeof(variables), 2, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableVUserAsync(request, &user, slots, variables, sizeof(variables), 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);

    for (const auto call : {sceNpTusGetMultiSlotVariableAVUserAsync, sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync}) {
        Require(call(request, nullptr, slots, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
        Require(call(request, &user, nullptr, variables, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
        Require(call(request, &user, slots, nullptr, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
        Require(call(request, &user, slots, variables, sizeof(variables), 0, nullptr) == INVALID_ARGUMENT);
        Require(call(request, &user, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
        Require(call(request, &user, negative, variables, sizeof(variables), 1, nullptr) == INVALID_ARGUMENT);
        Require(call(request, &empty, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
        Require(call(request, &empty, slots, variables, sizeof(variables), 2, nullptr) == INVALID_ARGUMENT);
        Require(call(request, &user, slots, variables, sizeof(variables), 2, nullptr) == 0);
        RequireFinished(request, SIGNED_OUT);
    }
    for (const auto call : {sceNpTusGetMultiSlotVariableAVUser, sceNpTusGetMultiSlotVariableForCrossSaveVUser}) {
        Require(call(request, &empty, slots, variables, sizeof(variables), 2, nullptr) == INVALID_ARGUMENT);
        Require(call(request, &user, slots, variables, sizeof(variables), 2, nullptr) == SIGNED_OUT);
        Require(call(request + 1000, &user, slots, variables, sizeof(variables), 2, nullptr) == INVALID_ID);
    }

    Require(sceNpTusGetMultiSlotVariableForCrossSaveAsync(request, 0, slots, nullptr, sizeof(variables), 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableForCrossSaveAsync(request, 0, slots, variables, sizeof(variables), 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusGetMultiSlotVariableForCrossSaveAsync(request, 0, slots, variables, sizeof(variables), 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    for (const auto call : {sceNpTusGetMultiSlotVariableA, sceNpTusGetMultiSlotVariableForCrossSave}) {
        Require(call(request, 0, slots, variables, sizeof(variables), 65, nullptr) == TOO_MANY_SLOTID);
        Require(call(request, 0, slots, variables, sizeof(variables), 2, nullptr) == SIGNED_OUT);
        Require(call(request + 1000, 0, slots, variables, sizeof(variables), 2, nullptr) == INVALID_ID);
    }

    Require(sceNpTusSetMultiSlotVariableAsync(request, nullptr, slots, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableAsync(request, nullptr, nullptr, values, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableAsync(request, nullptr, slots, values, 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableAsync(request, nullptr, negative, values, 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableAsync(request, &npId, slots, values, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, nullptr, slots, values, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, slots, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, nullptr, values, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, slots, values, 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, slots, values, 2, values) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, negative, values, 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, slots, values, 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &empty, slots, values, 2, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableVUserAsync(request, &user, slots, values, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusSetMultiSlotVariableA(request, 0, slots, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusSetMultiSlotVariableA(request, 0, slots, values, 2, nullptr) == SIGNED_OUT);

    Require(sceNpTusDeleteMultiSlotVariableAsync(request, nullptr, slots, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, slots, 0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, slots, 2, values) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, slots, 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, negative, 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableAsync(request, &npId, slots, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, nullptr, slots, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, slots, 0, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, nullptr, 2, nullptr) == INSUFFICIENT_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, slots, 2, values) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, slots, 65, nullptr) == TOO_MANY_SLOTID);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, negative, 1, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &empty, slots, 2, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableVUserAsync(request, &user, slots, 2, nullptr) == 0);
    RequireFinished(request, SIGNED_OUT);
    Require(sceNpTusDeleteMultiSlotVariable(request, &npId, slots, 2, nullptr) == SIGNED_OUT);
    Require(sceNpTusDeleteMultiSlotVariableVUser(request, &user, slots, 2, nullptr) == SIGNED_OUT);
    Require(sceNpTusDeleteMultiSlotVariableA(request, 0, slots, 0, nullptr) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteMultiSlotVariableA(request, 0, slots, 2, nullptr) == SIGNED_OUT);

    Require(sceNpTusAbortRequest(request) == 0);
    Require(sceNpTusDeleteMultiSlotVariable(request, &npId, slots, 2, nullptr) == ABORTED);
    Require(sceNpTusGetMultiSlotVariableA(request, 0, slots, variables, sizeof(variables), 2, nullptr) == ABORTED);

    const int fresh = sceNpTusCreateRequest(context);
    Require(fresh > 0);
    Require(sceNpTusAbortRequest(fresh) == 0);
    Require(sceNpTusWaitAsync(fresh, &result) == INVALID_ARGUMENT);
    Require(sceNpTusDeleteNpTitleCtx(context) == 0);
    Require(sceNpTusWaitAsync(request, &result) == INVALID_ID);
    Require(sceNpTusSetMultiSlotVariableA(fresh, 0, slots, values, 2, nullptr) == INVALID_ID);
    for (const std::uint8_t byte : variables) Require(byte == 0x5a);
}

int main() {
    CheckMultiSlotVariables();

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

    Require(sceNpTusDeleteNpTitleCtx(0) == INVALID_ID);
    Require(sceNpTusDeleteNpTitleCtx(-1) == INVALID_ID);
    Require(sceNpTusDeleteNpTitleCtx(context + 1000) == INVALID_ID);
    const int siblingContext = sceNpTusCreateNpTitleCtxA(0, 1);
    const int siblingRequest = sceNpTusCreateRequest(siblingContext);
    Require(siblingContext > 0 && siblingRequest > 0);
    Require(sceNpTusSetMultiSlotVariableAAsync(siblingRequest, 0, slots, values, 2, nullptr) == 0);
    const int idle = sceNpTusCreateRequest(context);
    const int finished = sceNpTusCreateRequest(context);
    const int aborted = sceNpTusCreateRequest(context);
    Require(idle > 0 && finished > 0 && aborted > 0);
    Require(sceNpTusSetMultiSlotVariableAAsync(finished, 0, slots, values, 2, nullptr) == 0);
    Require(sceNpTusAbortRequest(aborted) == 0);
    Require(sceNpTusDeleteNpTitleCtx(context) == 0);
    Require(sceNpTusDeleteNpTitleCtx(context) == INVALID_ID);
    Require(sceNpTusCreateRequest(context) == INVALID_ID);
    for (const int child : {idle, finished, aborted}) {
        Require(sceNpTusPollAsync(child, &result) == INVALID_ID);
        Require(sceNpTusDeleteRequest(child) == INVALID_ID);
        Require(sceNpTusAbortRequest(child) == INVALID_ID);
        Require(sceNpTusSetMultiSlotVariableAAsync(child, 0, slots, values, 2, nullptr) == INVALID_ID);
    }
    RequireFinished(siblingRequest, SIGNED_OUT);
    Require(sceNpTusDeleteNpTitleCtx(siblingContext) == 0);

    std::array<int, 32> contexts{};
    for (int& titleContext : contexts) {
        titleContext = sceNpTusCreateNpTitleCtxA(0, 1);
        Require(titleContext > 0);
    }
    Require(sceNpTusCreateNpTitleCtxA(0, 1) == TOO_MANY_OBJECTS);
    for (int& titleContext : contexts) {
        const int previous = titleContext;
        Require(sceNpTusDeleteNpTitleCtx(previous) == 0);
        titleContext = sceNpTusCreateNpTitleCtxA(0, 1);
        Require(titleContext > 0 && titleContext != previous);
        Require(sceNpTusCreateRequest(previous) == INVALID_ID);
    }
    std::array<int, 256> children{};
    for (int& child : children) {
        child = sceNpTusCreateRequest(contexts[0]);
        Require(child > 0);
    }
    Require(sceNpTusCreateRequest(contexts[1]) == TOO_MANY_OBJECTS);
    Require(sceNpTusDeleteNpTitleCtx(contexts[0]) == 0);
    Require(sceNpTusCreateRequest(contexts[1]) > 0);
    for (const int child : children) Require(sceNpTusPollAsync(child, &result) == INVALID_ID);
    for (std::size_t index = 1; index < contexts.size(); ++index) Require(sceNpTusDeleteNpTitleCtx(contexts[index]) == 0);
    CheckTssTitleContexts();
}
