#include "prx/libSceNpTus/libSceNpTus.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>


namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidArgument = static_cast<int>(0x80550704u);
constexpr int InsufficientArgument = static_cast<int>(0x8055070Cu);
constexpr int InvalidAlignment = static_cast<int>(0x80550714u);
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
std::int64_t variables[2] = {5, 6};
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

struct FriendsOption {
    std::uint64_t size;
    std::uint32_t* startSerialRank;
    std::uint32_t* hits;
};

void CheckFriends(const char* name, int accepted, int maxSortType, bool waits, const std::function<int(int, int, int, void*, int, const FriendsOption*)>& call) {
    const int req = NewRequest();
    Require(call(req, 1, 1, nullptr, 2, nullptr) == InsufficientArgument, name, "a null array was taken");
    Require(call(req, 1, 1, buffer, 0, nullptr) == InvalidArgument, name, "an array length of 0 was taken");
    Require(call(req, -1, 1, buffer, 2, nullptr) == InvalidArgument, name, "a negative slot id was taken");
    Require(call(req, 1, 1, buffer, 101, nullptr) == TooManyNpId, name, "101 friends were taken");
    Require(call(req, 1, 0, buffer, 2, nullptr) == InvalidArgument, name, "sort type 0 was taken");
    Require(call(req, 1, maxSortType + 1, buffer, 2, nullptr) == InvalidArgument, name, "a sort type above the last one was taken");
    std::uint32_t rank = 0;
    std::uint32_t hits = 0x5a5a5a5a;
    FriendsOption option{sizeof(FriendsOption) + 8, &rank, &hits};
    Require(call(req, 1, 1, buffer, 2, &option) == InvalidAlignment, name, "an option of the wrong size was taken");
    option.size = sizeof(FriendsOption);
    Require(call(req, 1, 1, buffer, 2, &option) == InvalidArgument, name, "a start rank of 0 was taken");
    rank = 3;
    Require(call(req, 1, maxSortType, buffer, 2, &option) == accepted, name, "a valid option was refused");
    Require(hits == 0x5a5a5a5a, name, "the hit count was written offline");
    option.startSerialRank = nullptr;
    Require(call(req, 1, 1, buffer, 100, &option) == accepted, name, "an option without a start rank or 100 friends was refused");
    Require(call(9999, 1, 1, buffer, 2, nullptr) == InvalidId, name, "an unknown request was not refused with INVALID_ID");
    int result = 0;
    if (!waits) Require(sceNpTusWaitAsync(req, &result) == 0 && result == SignedOut, name, "the request did not finish signed out");
    sceNpTusDeleteRequest(req);
}

struct TssOption {
    std::uint64_t size;
    std::uint64_t* offset;
    std::uint64_t* lastByte;
    void* ifParam;
};

void CheckTss(const char* name, int accepted, const std::function<int(int, int, void*, std::uint64_t, const TssOption*)>& call) {
    const int req = NewRequest();
    Require(call(req, 0, nullptr, 0, nullptr) == accepted, name, "a null data status did not reach the request");
    Require(call(req, -1, buffer, 0x18, nullptr) == InvalidArgument, name, "a negative slot id was taken");
    Require(call(req, 16, buffer, 0x18, nullptr) == InvalidArgument, name, "slot id 16 was taken");
    Require(call(req, 16, nullptr, 0, nullptr) == InvalidArgument, name, "the slot was not checked when data status is null");
    Require(call(req, 0, buffer, 0, nullptr) == InvalidAlignment, name, "data status size 0 was accepted");
    Require(call(req, 0, buffer, 0x17, nullptr) == InvalidAlignment, name, "a short data status size was accepted");
    Require(call(req, 0, buffer, 0x19, nullptr) == InvalidAlignment, name, "a long data status size was accepted");
    Require(call(req, 16, buffer, 0x17, nullptr) == InvalidAlignment, name, "the slot check ran before an invalid data status size");
    Require(call(9999, 0, buffer, 0x19, nullptr) == InvalidAlignment, name, "the request lookup ran before an invalid data status size");
    Require(call(req, 0, buffer, 0x18, nullptr) == accepted, name, "data status size 0x18 was refused");
    Require(call(req, 0, buffer, 0x18, reinterpret_cast<const TssOption*>(buffer)) == InvalidAlignment, name, "the option-size check did not run before the request lookup");
    TssOption option{sizeof(TssOption) - 8, nullptr, nullptr, nullptr};
    Require(call(req, 16, buffer, 0x18, &option) == InvalidAlignment, name, "the option-size check did not run before the slot check");
    Require(call(req, 15, buffer, 0x18, &option) == InvalidAlignment, name, "an option of the wrong size was taken");
    std::uint64_t offset = 4;
    option = TssOption{sizeof(TssOption), &offset, &offset, nullptr};
    Require(call(req, 15, buffer, 0x18, &option) == accepted, name, "a valid option was refused");
    Require(call(req, 0, buffer, 0x18, nullptr) == accepted, name, "slot id 0 without an option was refused");
    Require(call(9999, 0, buffer, 0x18, nullptr) == InvalidId, name, "an unknown request was not refused with INVALID_ID");
    int result = 0;
    if (accepted == 0) Require(sceNpTusWaitAsync(req, &result) == 0 && result == SignedOut, name, "the request did not finish signed out");
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
    CheckSync("sceNpTusGetData", [&](int req) { return sceNpTusGetData(req, &npId, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataA", [&](int req) { return sceNpTusGetDataA(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataAVUser", [&](int req) { return sceNpTusGetDataAVUser(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataForCrossSave", [&](int req) { return sceNpTusGetDataForCrossSave(req, 1, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetDataVUser", [&](int req) { return sceNpTusGetDataVUser(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckSync("sceNpTusGetFriendsDataStatus", [&](int req) { return sceNpTusGetFriendsDataStatus(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetFriendsVariable", [&](int req) { return sceNpTusGetFriendsVariable(req, 1, 0, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatus", [&](int req) { return sceNpTusGetMultiSlotDataStatus(req, &npId, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusA", [&](int req) { return sceNpTusGetMultiSlotDataStatusA(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusAVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusAVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusForCrossSave", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSave(req, 1, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusForCrossSaveVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotDataStatusVUser", [&](int req) { return sceNpTusGetMultiSlotDataStatusVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotVariable", [&](int req) { return sceNpTusGetMultiSlotVariable(req, &npId, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiSlotVariableVUser", [&](int req) { return sceNpTusGetMultiSlotVariableVUser(req, &virtualUser, slots, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatus", [&](int req) { return sceNpTusGetMultiUserDataStatus(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusA", [&](int req) { return sceNpTusGetMultiUserDataStatusA(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusAVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusAVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusForCrossSave", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSave(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusForCrossSaveVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserDataStatusVUser", [&](int req) { return sceNpTusGetMultiUserDataStatusVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariable", [&](int req) { return sceNpTusGetMultiUserVariable(req, npIds, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableA", [&](int req) { return sceNpTusGetMultiUserVariableA(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableAVUser", [&](int req) { return sceNpTusGetMultiUserVariableAVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableForCrossSave", [&](int req) { return sceNpTusGetMultiUserVariableForCrossSave(req, accounts, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableForCrossSaveVUser", [&](int req) { return sceNpTusGetMultiUserVariableForCrossSaveVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusGetMultiUserVariableVUser", [&](int req) { return sceNpTusGetMultiUserVariableVUser(req, virtualUsers, 1, buffer, sizeof(buffer), 2, nullptr); });
    CheckSync("sceNpTusSetData", [&](int req) { return sceNpTusSetData(req, &npId, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusSetDataA", [&](int req) { return sceNpTusSetDataA(req, 1, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusSetDataAVUser", [&](int req) { return sceNpTusSetDataAVUser(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusSetDataVUser", [&](int req) { return sceNpTusSetDataVUser(req, &virtualUser, 1, 16, 8, buffer, buffer, sizeof(buffer), nullptr, nullptr, nullptr); });
    CheckSync("sceNpTusSetMultiSlotVariable", [&](int req) { return sceNpTusSetMultiSlotVariable(req, &npId, slots, variables, 2, nullptr); });
    CheckSync("sceNpTusSetMultiSlotVariableVUser", [&](int req) { return sceNpTusSetMultiSlotVariableVUser(req, &virtualUser, slots, variables, 2, nullptr); });
    CheckSync("sceNpTusTryAndSetVariable", [&](int req) { return sceNpTusTryAndSetVariable(req, &npId, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableA", [&](int req) { return sceNpTusTryAndSetVariableA(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableAVUser", [&](int req) { return sceNpTusTryAndSetVariableAVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableForCrossSave", [&](int req) { return sceNpTusTryAndSetVariableForCrossSave(req, 1, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableForCrossSaveVUser", [&](int req) { return sceNpTusTryAndSetVariableForCrossSaveVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckSync("sceNpTusTryAndSetVariableVUser", [&](int req) { return sceNpTusTryAndSetVariableVUser(req, &virtualUser, 1, 1, 5, nullptr, nullptr, nullptr, buffer, sizeof(buffer), nullptr); });
    CheckFriends("sceNpTusGetFriendsDataStatusAAsync", 0, 2, false, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsDataStatusAAsync(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsDataStatusA", SignedOut, 2, true, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsDataStatusA(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsVariableAAsync", 0, 4, false, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsVariableAAsync(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsVariableA", SignedOut, 4, true, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsVariableA(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsDataStatusForCrossSaveAsync", 0, 2, false, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsDataStatusForCrossSaveAsync(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsDataStatusForCrossSave", SignedOut, 2, true, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsDataStatusForCrossSave(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsVariableForCrossSaveAsync", 0, 4, false, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsVariableForCrossSaveAsync(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckFriends("sceNpTusGetFriendsVariableForCrossSave", SignedOut, 4, true, [&](int req, int slotId, int sortType, void* array, int arrayLen, const FriendsOption* option) { return sceNpTusGetFriendsVariableForCrossSave(req, slotId, 0, sortType, array, 16, arrayLen, option); });
    CheckAsync("sceNpTusGetDataForCrossSaveVUserAsync", [&](int req) { return sceNpTusGetDataForCrossSaveVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&]() { return sceNpTusGetDataForCrossSaveVUserAsync(9999, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); }, [&](int req) { return sceNpTusGetDataForCrossSaveVUserAsync(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, &buffer); });
    CheckSync("sceNpTusGetDataForCrossSaveVUser", [&](int req) { return sceNpTusGetDataForCrossSaveVUser(req, &virtualUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr); });
    CheckAsync("sceNpTusDeleteMultiSlotDataVUserAsync", [&](int req) { return sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, slots, 2, nullptr); }, [&]() { return sceNpTusDeleteMultiSlotDataVUserAsync(9999, &virtualUser, slots, 2, nullptr); }, [&](int req) { return sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, slots, 2, &buffer); });
    CheckSync("sceNpTusDeleteMultiSlotDataVUser", [&](int req) { return sceNpTusDeleteMultiSlotDataVUser(req, &virtualUser, slots, 2, nullptr); });
    CheckTss("sceNpTssGetDataAsync", 0, [&](int req, int slotId, void* status, std::uint64_t statusSize, const TssOption* option) { return sceNpTssGetDataAsync(req, slotId, status, statusSize, buffer, 16, option); });
    CheckTss("sceNpTssGetData", SignedOut, [&](int req, int slotId, void* status, std::uint64_t statusSize, const TssOption* option) { return sceNpTssGetData(req, slotId, status, statusSize, buffer, 16, option); });
    {
        const int req = NewRequest();
        Require(sceNpTusGetData(req, &npId, 1, buffer, sizeof(buffer), buffer, 16, &buffer) == InvalidArgument, "sceNpTusGetData", "an option was not refused before waiting");
        Require(sceNpTusSetMultiSlotVariable(req, &npId, manySlots, reinterpret_cast<const std::int64_t*>(buffer), 65, nullptr) == TooManySlotId, "sceNpTusSetMultiSlotVariable", "65 slots were not refused before waiting");
        sceNpTusDeleteRequest(req);
    }
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
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, nullptr, 1, buffer, sizeof(buffer), buffer, 16, nullptr) == InsufficientArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "a null virtual user was taken");
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, &virtualUser, 1, nullptr, sizeof(buffer), buffer, 16, nullptr) == InsufficientArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "a null data status was taken");
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, &virtualUser, 1, buffer, 0, buffer, 16, nullptr) == InsufficientArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "a data status size of 0 was taken");
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, &virtualUser, -1, buffer, sizeof(buffer), buffer, 16, nullptr) == InvalidArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "a negative slot id was taken");
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, &emptyUser, 1, buffer, sizeof(buffer), buffer, 16, nullptr) == InvalidArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "an empty virtual user was taken");
        Require(sceNpTusGetDataForCrossSaveVUserAsync(req, nullptr, -1, buffer, sizeof(buffer), buffer, 16, nullptr) == InvalidArgument, "sceNpTusGetDataForCrossSaveVUserAsync", "the slot id was not checked before the virtual user");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, nullptr, slots, 2, nullptr) == InsufficientArgument, "sceNpTusDeleteMultiSlotDataVUserAsync", "a null virtual user was taken");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, nullptr, 2, nullptr) == InsufficientArgument, "sceNpTusDeleteMultiSlotDataVUserAsync", "null slot ids were taken");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, slots, 0, nullptr) == InsufficientArgument, "sceNpTusDeleteMultiSlotDataVUserAsync", "an array length of 0 was taken");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, manySlots, 65, nullptr) == TooManySlotId, "sceNpTusDeleteMultiSlotDataVUserAsync", "65 slots were taken");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, &virtualUser, negative, 2, nullptr) == InvalidArgument, "sceNpTusDeleteMultiSlotDataVUserAsync", "a negative slot id was taken");
        Require(sceNpTusDeleteMultiSlotDataVUserAsync(req, &emptyUser, slots, 2, nullptr) == InvalidArgument, "sceNpTusDeleteMultiSlotDataVUserAsync", "an empty virtual user was taken");
        sceNpTusDeleteRequest(req);

    }
    if (failures != 0) return 1;
    std::puts("NpTus offline request tests passed");
    return 0;
}
