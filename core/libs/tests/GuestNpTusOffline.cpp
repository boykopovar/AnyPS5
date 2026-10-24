#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

extern "C" {
int APS5_VABI sceNpTusCreateNpTitleCtxA(std::uint32_t, int);
int APS5_VABI sceNpTusCreateRequest(int);
int APS5_VABI sceNpTusDeleteRequest(int);
int APS5_VABI sceNpTusWaitAsync(int, int*);
int APS5_VABI sceNpTssGetSmallStorage(int reqId, void* data, std::uint64_t maxSize, std::uint64_t* contentLength, void* option);
int APS5_VABI sceNpTssGetSmallStorageAsync(int reqId, void* data, std::uint64_t maxSize, std::uint64_t* contentLength, void* option);
int APS5_VABI sceNpTusAddAndGetVariable(int reqId, const void* targetNpId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusAddAndGetVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotData(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotDataA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotDataAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusDeleteMultiSlotDataAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option);
int APS5_VABI sceNpTusGetDataA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataAsync(int reqId, const void* targetNpId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetDataVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option);
int APS5_VABI sceNpTusGetFriendsDataStatus(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetFriendsDataStatusAsync(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetFriendsVariable(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetFriendsVariableAsync(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatus(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusAVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSave(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiSlotDataStatusVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatus(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusA(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusAAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusAVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusAsync(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSave(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserDataStatusVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariable(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableA(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableAAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableAsync(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusGetMultiUserVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option);
int APS5_VABI sceNpTusSetDataA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusSetDataAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusSetDataAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusSetDataAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusSetDataAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const void* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusSetDataVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const void* isLastChangedAuthor, const void* isLastChangedDate, void* option);
int APS5_VABI sceNpTusTryAndSetVariable(int reqId, const void* targetNpId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
int APS5_VABI sceNpTusTryAndSetVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option);
}

namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidArgument = static_cast<int>(0x80550704u);
constexpr int InvalidId = static_cast<int>(0x8055070Eu);
constexpr int InvalidType = static_cast<int>(0x80550711u);
constexpr int TooManySlotId = static_cast<int>(0x80550718u);
constexpr int TooManyNpId = static_cast<int>(0x80550719u);

int context = 0;
alignas(8) unsigned char buffer[1024];
unsigned char npId[64];
NpOnlineId virtualUser{};
NpOnlineId virtualUsers[2]{};
NpOnlineId emptyUser{};
unsigned char npIds[2][64];
std::uint64_t accounts[2] = {1, 2};
std::int32_t slots[2] = {1, 2};
std::int32_t manySlots[65] = {};
std::uint64_t length = 0;
int failures = 0;

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

int NewRequest() {
    const int req = sceNpTusCreateRequest(context);
    if (req <= 0) {
        std::fprintf(stderr, "sceNpTusCreateRequest failed\n");
        std::abort();
    }
    return req;
}

void CheckAsync(const char* name, const std::function<int(int)>& valid, const std::function<int()>& unknownRequest, const std::function<int(int)>& withOption) {
    int req = NewRequest();
    Require(valid(req) == 0, name, "valid arguments did not start the request");
    int result = 0;
    Require(sceNpTusWaitAsync(req, &result) == 0 && result == SignedOut, name, "the request did not finish signed out");
    sceNpTusDeleteRequest(req);
    Require(unknownRequest() == InvalidId, name, "an unknown request was not refused with INVALID_ID");
    if (withOption) {
        req = NewRequest();
        Require(withOption(req) == InvalidArgument, name, "a non-null option was not refused with INVALID_ARGUMENT");
        sceNpTusDeleteRequest(req);
    }
}

void CheckSync(const char* name, const std::function<int(int)>& call) {
    const int req = NewRequest();
    Require(call(req) == SignedOut, name, "the synchronous call did not return SIGNED_OUT");
    sceNpTusDeleteRequest(req);
}

}

int main() {
    std::memset(npId, 'n', sizeof(npId));
    std::memset(npIds, 'n', sizeof(npIds));
    std::strcpy(virtualUser.data, "virtual");
    std::strcpy(virtualUsers[0].data, "first");
    std::strcpy(virtualUsers[1].data, "second");
    context = sceNpTusCreateNpTitleCtxA(1, 1);
    if (context <= 0) return 1;
    CheckAsync("sceNpTssGetSmallStorageAsync", [&](int req) { return sceNpTssGetSmallStorageAsync(req, buffer, 16, &length, nullptr); }, [&]() { int req = 9999; return sceNpTssGetSmallStorageAsync(9999, buffer, 16, &length, nullptr); }, [&](int req) { return sceNpTssGetSmallStorageAsync(req, buffer, 16, &length, &buffer); });
    CheckAsync("sceNpTusAddAndGetVariableAVUserAsync", [&](int req) { return sceNpTusAddAndGetVariableAVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusAddAndGetVariableAVUserAsync(9999, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusAddAndGetVariableAVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusAddAndGetVariableAsync", [&](int req) { return sceNpTusAddAndGetVariableAsync(req, &npId, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusAddAndGetVariableAsync(9999, &npId, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusAddAndGetVariableAsync(req, &npId, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusAddAndGetVariableForCrossSaveAsync", [&](int req) { return sceNpTusAddAndGetVariableForCrossSaveAsync(req, 1, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusAddAndGetVariableForCrossSaveAsync(9999, 1, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusAddAndGetVariableForCrossSaveAsync(req, 1, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusAddAndGetVariableForCrossSaveVUserAsync", [&](int req) { return sceNpTusAddAndGetVariableForCrossSaveVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusAddAndGetVariableForCrossSaveVUserAsync(9999, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusAddAndGetVariableForCrossSaveVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusAddAndGetVariableVUserAsync", [&](int req) { return sceNpTusAddAndGetVariableVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusAddAndGetVariableVUserAsync(9999, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusAddAndGetVariableVUserAsync(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusDeleteMultiSlotDataAAsync", [&](int req) { return sceNpTusDeleteMultiSlotDataAAsync(req, 1, slots, 2, nullptr); }, [&]() { int req = 9999; return sceNpTusDeleteMultiSlotDataAAsync(9999, 1, slots, 2, nullptr); }, [&](int req) { return sceNpTusDeleteMultiSlotDataAAsync(req, 1, slots, 2, &buffer); });
    CheckAsync("sceNpTusDeleteMultiSlotDataAsync", [&](int req) { return sceNpTusDeleteMultiSlotDataAsync(req, &npId, slots, 2, nullptr); }, [&]() { int req = 9999; return sceNpTusDeleteMultiSlotDataAsync(9999, &npId, slots, 2, nullptr); }, [&](int req) { return sceNpTusDeleteMultiSlotDataAsync(req, &npId, slots, 2, &buffer); });
    CheckAsync("sceNpTusGetDataAAsync", [&](int req) { return sceNpTusGetDataAAsync(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { int req = 9999; return sceNpTusGetDataAAsync(9999, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataAAsync(req, 1, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckAsync("sceNpTusGetDataAVUserAsync", [&](int req) { return sceNpTusGetDataAVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { int req = 9999; return sceNpTusGetDataAVUserAsync(9999, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataAVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckAsync("sceNpTusGetDataAsync", [&](int req) { return sceNpTusGetDataAsync(req, &npId, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { int req = 9999; return sceNpTusGetDataAsync(9999, &npId, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataAsync(req, &npId, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckAsync("sceNpTusGetDataForCrossSaveAsync", [&](int req) { return sceNpTusGetDataForCrossSaveAsync(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { int req = 9999; return sceNpTusGetDataForCrossSaveAsync(9999, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataForCrossSaveAsync(req, 1, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckAsync("sceNpTusGetDataVUserAsync", [&](int req) { return sceNpTusGetDataVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { int req = 9999; return sceNpTusGetDataVUserAsync(9999, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckAsync("sceNpTusGetFriendsDataStatusAsync", [&](int req) { return sceNpTusGetFriendsDataStatusAsync(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetFriendsDataStatusAsync(9999, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetFriendsDataStatusAsync(req, 1, 0, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetFriendsVariableAsync", [&](int req) { return sceNpTusGetFriendsVariableAsync(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetFriendsVariableAsync(9999, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetFriendsVariableAsync(req, 1, 0, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusAAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusAAsync(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusAAsync(9999, 1, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusAAsync(req, 1, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusAVUserAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusAVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusAVUserAsync(9999, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusAVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusAsync(req, &npId, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusAsync(9999, &npId, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusAsync(req, &npId, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusForCrossSaveAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(9999, 1, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(req, 1, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(9999, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiSlotDataStatusVUserAsync", [&](int req) { return sceNpTusGetMultiSlotDataStatusVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiSlotDataStatusVUserAsync(9999, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiSlotDataStatusVUserAsync(req, &virtualUser, slots, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusAAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusAAsync(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusAAsync(9999, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusAAsync(req, accounts, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusAVUserAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusAVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusAVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusAVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusAsync(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusAsync(9999, npIds, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusAsync(req, npIds, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusForCrossSaveAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveAsync(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusForCrossSaveAsync(9999, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveAsync(req, accounts, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserDataStatusVUserAsync", [&](int req) { return sceNpTusGetMultiUserDataStatusVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserDataStatusVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserDataStatusVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableAAsync", [&](int req) { return sceNpTusGetMultiUserVariableAAsync(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableAAsync(9999, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableAAsync(req, accounts, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableAVUserAsync", [&](int req) { return sceNpTusGetMultiUserVariableAVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableAVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableAVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableAsync", [&](int req) { return sceNpTusGetMultiUserVariableAsync(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableAsync(9999, npIds, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableAsync(req, npIds, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableForCrossSaveAsync", [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveAsync(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableForCrossSaveAsync(9999, accounts, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveAsync(req, accounts, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableForCrossSaveVUserAsync", [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusGetMultiUserVariableVUserAsync", [&](int req) { return sceNpTusGetMultiUserVariableVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&]() { int req = 9999; return sceNpTusGetMultiUserVariableVUserAsync(9999, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); }, [&](int req) { return sceNpTusGetMultiUserVariableVUserAsync(req, virtualUsers, 1, buffer, sizeof(buffer), 2, &buffer); });
    CheckAsync("sceNpTusSetDataAAsync", [&](int req) { return sceNpTusSetDataAAsync(req, 1, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&]() { int req = 9999; return sceNpTusSetDataAAsync(9999, 1, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&](int req) { return sceNpTusSetDataAAsync(req, 1, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, &buffer); });
    CheckAsync("sceNpTusSetDataAVUserAsync", [&](int req) { return sceNpTusSetDataAVUserAsync(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&]() { int req = 9999; return sceNpTusSetDataAVUserAsync(9999, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&](int req) { return sceNpTusSetDataAVUserAsync(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, &buffer); });
    CheckAsync("sceNpTusSetDataAsync", [&](int req) { return sceNpTusSetDataAsync(req, &npId, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&]() { int req = 9999; return sceNpTusSetDataAsync(9999, &npId, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&](int req) { return sceNpTusSetDataAsync(req, &npId, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, &buffer); });
    CheckAsync("sceNpTusSetDataVUserAsync", [&](int req) { return sceNpTusSetDataVUserAsync(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&]() { int req = 9999; return sceNpTusSetDataVUserAsync(9999, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); }, [&](int req) { return sceNpTusSetDataVUserAsync(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableAAsync", [&](int req) { return sceNpTusTryAndSetVariableAAsync(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableAAsync(9999, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableAAsync(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableAVUserAsync", [&](int req) { return sceNpTusTryAndSetVariableAVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableAVUserAsync(9999, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableAVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableAsync", [&](int req) { return sceNpTusTryAndSetVariableAsync(req, &npId, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableAsync(9999, &npId, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableAsync(req, &npId, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableForCrossSaveAsync", [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveAsync(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableForCrossSaveAsync(9999, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveAsync(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableForCrossSaveVUserAsync", [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableForCrossSaveVUserAsync(9999, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckAsync("sceNpTusTryAndSetVariableVUserAsync", [&](int req) { return sceNpTusTryAndSetVariableVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&]() { int req = 9999; return sceNpTusTryAndSetVariableVUserAsync(9999, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); }, [&](int req) { return sceNpTusTryAndSetVariableVUserAsync(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), &buffer); });
    CheckSync("sceNpTssGetSmallStorage", [&](int req) { return sceNpTssGetSmallStorage(req, buffer, 16, &length, nullptr); });
    CheckSync("sceNpTusAddAndGetVariable", [&](int req) { return sceNpTusAddAndGetVariable(req, &npId, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusAddAndGetVariableA", [&](int req) { return sceNpTusAddAndGetVariableA(req, 1, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusAddAndGetVariableAVUser", [&](int req) { return sceNpTusAddAndGetVariableAVUser(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusAddAndGetVariableForCrossSave", [&](int req) { return sceNpTusAddAndGetVariableForCrossSave(req, 1, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusAddAndGetVariableForCrossSaveVUser", [&](int req) { return sceNpTusAddAndGetVariableForCrossSaveVUser(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusAddAndGetVariableVUser", [&](int req) { return sceNpTusAddAndGetVariableVUser(req, &virtualUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusDeleteMultiSlotData", [&](int req) { return sceNpTusDeleteMultiSlotData(req, &npId, slots, 2, nullptr); });
    CheckSync("sceNpTusDeleteMultiSlotDataA", [&](int req) { return sceNpTusDeleteMultiSlotDataA(req, 1, slots, 2, nullptr); });
    CheckSync("sceNpTusGetDataA", [&](int req) { return sceNpTusGetDataA(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataAVUser", [&](int req) { return sceNpTusGetDataAVUser(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataForCrossSave", [&](int req) { return sceNpTusGetDataForCrossSave(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetFriendsDataStatus", [&](int req) { return sceNpTusGetFriendsDataStatus(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetFriendsVariable", [&](int req) { return sceNpTusGetFriendsVariable(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatus", [&](int req) { return sceNpTusGetMultiSlotDataStatus(req, &npId, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusA", [&](int req) { return sceNpTusGetMultiSlotDataStatusA(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusAVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusAVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusForCrossSave", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSave(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatus", [&](int req) { return sceNpTusGetMultiUserDataStatus(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusA", [&](int req) { return sceNpTusGetMultiUserDataStatusA(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusAVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusAVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusForCrossSave", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSave(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariable", [&](int req) { return sceNpTusGetMultiUserVariable(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableA", [&](int req) { return sceNpTusGetMultiUserVariableA(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableAVUser", [&](int req) { return sceNpTusGetMultiUserVariableAVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableVUser", [&](int req) { return sceNpTusGetMultiUserVariableVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusSetDataA", [&](int req) { return sceNpTusSetDataA(req, 1, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusSetDataAVUser", [&](int req) { return sceNpTusSetDataAVUser(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusTryAndSetVariable", [&](int req) { return sceNpTusTryAndSetVariable(req, &npId, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableA", [&](int req) { return sceNpTusTryAndSetVariableA(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableAVUser", [&](int req) { return sceNpTusTryAndSetVariableAVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableForCrossSave", [&](int req) { return sceNpTusTryAndSetVariableForCrossSave(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableForCrossSaveVUser", [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableVUser", [&](int req) { return sceNpTusTryAndSetVariableVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    {
        int req = NewRequest();
        Require(sceNpTusAddAndGetVariableVUserAsync(req, &emptyUser, 1, 5, nullptr, nullptr, buffer, sizeof(buffer), nullptr) == InvalidArgument, "sceNpTusAddAndGetVariableVUserAsync", "an empty virtual user was taken");
        NpOnlineId partlyEmpty[2]{};
        std::strcpy(partlyEmpty[0].data, "first");
        Require(sceNpTusGetMultiUserVariableVUserAsync(req, partlyEmpty, 1, buffer, sizeof(buffer), 2, nullptr) == InvalidArgument, "sceNpTusGetMultiUserVariableVUserAsync", "an empty virtual user in the array was taken");
        Require(sceNpTusTryAndSetVariableAAsync(req, 1, 1, 0, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr) == InvalidType, "sceNpTusTryAndSetVariableAAsync", "operation type 0 was taken");
        Require(sceNpTusTryAndSetVariableAAsync(req, 1, 1, 7, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr) == InvalidType, "sceNpTusTryAndSetVariableAAsync", "operation type 7 was taken");
        Require(sceNpTusTryAndSetVariableAsync(req, npId, 1, 7, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr) == InvalidArgument, "sceNpTusTryAndSetVariableAsync", "operation type 7 was not refused with INVALID_ARGUMENT");
        Require(sceNpTssGetSmallStorageAsync(req, buffer, 64 * 1024 + 1, &length, nullptr) == InvalidArgument, "sceNpTssGetSmallStorageAsync", "a size above 64 KiB was taken");
        Require(sceNpTusDeleteMultiSlotDataAAsync(req, 1, manySlots, 65, nullptr) == TooManySlotId, "sceNpTusDeleteMultiSlotDataAAsync", "65 slots were taken");
        Require(sceNpTusGetMultiSlotDataStatusAsync(req, npId, manySlots, buffer, sizeof(buffer), 65, nullptr) == TooManyNpId, "sceNpTusGetMultiSlotDataStatusAsync", "65 slots did not return TOO_MANY_NPID");
        const std::int32_t negative[2] = {1, -1};
        Require(sceNpTusDeleteMultiSlotDataAAsync(req, 1, negative, 2, nullptr) == InvalidArgument, "sceNpTusDeleteMultiSlotDataAAsync", "a negative slot id was taken");
        Require(sceNpTusGetMultiUserVariableAAsync(req, accounts, 1, buffer, sizeof(buffer), 102, nullptr) == TooManyNpId, "sceNpTusGetMultiUserVariableAAsync", "102 users were taken");
        Require(sceNpTusGetFriendsVariableAsync(req, 1, 0, 5, buffer, sizeof(buffer), 2, nullptr) == InvalidArgument, "sceNpTusGetFriendsVariableAsync", "sort type 5 was taken");
        Require(sceNpTusGetFriendsVariableAsync(req, 1, 0, 1, buffer, sizeof(buffer), 101, nullptr) == TooManyNpId, "sceNpTusGetFriendsVariableAsync", "101 friends were taken");
        sceNpTusDeleteRequest(req);
    }
    if (failures != 0) return 1;
    std::puts("NpTus offline request tests passed");
    return 0;
}
