#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using SelfCall = int (APS5_VABI*)(void*);
using Size = std::size_t (APS5_VABI*)(const void*);
using Element = void* (APS5_VABI*)(void*, std::uint64_t);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

static constexpr int ParameterNotInitialized = static_cast<int>(0x80553502);
static constexpr int ParameterInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ParameterAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ParameterTerminatedUninitialized = static_cast<int>(0x80553510);
static constexpr int ParameterBodyMissing = static_cast<int>(0x80553512);

struct ParameterLifecycle {
    int (APS5_VABI* construct)(void*);
    int (APS5_VABI* destroy)(void*);
    int (APS5_VABI* terminate)(void*);
    int (APS5_VABI* setText)(void*, const char*);
};

template <typename TInitialize>
static void TestParameterLifecycle(const ParameterLifecycle& lifecycle, TInitialize initialize) {
    std::uint64_t context = 0;
    alignas(16) unsigned char parameter[0x20];
    std::memset(parameter, 0x5a, sizeof(parameter));
    Require(lifecycle.construct(parameter) == 0, "a parameter constructor failed");
    for (std::size_t index = 0; index < sizeof(parameter); ++index)
        Require(parameter[index] == (index < 0x10 ? 0 : 0x5a), "a parameter constructor wrote the wrong bytes");
    Require(lifecycle.setText == nullptr || lifecycle.setText(parameter, "text") == ParameterNotInitialized, "a setter accepted an uninitialized parameter");
    Require(lifecycle.terminate(parameter) == ParameterTerminatedUninitialized, "terminate accepted an uninitialized parameter");
    Require(initialize(parameter, nullptr, true) == ParameterInvalidArgument, "initialize accepted a null context");
    Require(initialize(parameter, &context, true) == ParameterNotInitialized, "initialize accepted an uninitialized context");
    Require(_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(nullptr, &context) == 0, "the context did not initialize");
    Require(initialize(parameter, &context, false) == ParameterInvalidArgument, "initialize accepted a null argument");
    Require(lifecycle.terminate(parameter) == ParameterTerminatedUninitialized, "a failed initialize left the parameter initialized");
    Require(initialize(parameter, &context, true) == 0, "initialize failed");
    Require(initialize(parameter, &context, true) == ParameterAlreadyInitialized, "initialize ran twice");
    Require(lifecycle.setText == nullptr || lifecycle.setText(parameter, nullptr) == ParameterInvalidArgument, "a setter accepted a null string");
    Require(lifecycle.setText == nullptr || lifecycle.setText(parameter, "text") == 0, "a setter failed");
    Require(lifecycle.terminate(parameter) == 0, "terminate failed");
    Require(lifecycle.terminate(parameter) == ParameterTerminatedUninitialized, "terminate ran twice");
    Require(initialize(parameter, &context, true) == 0, "initialize after terminate failed");
    Require(lifecycle.terminate(parameter) == 0, "the second terminate failed");
    Require(lifecycle.destroy(parameter) == 0, "a parameter destructor failed");
    for (std::size_t index = 0; index < sizeof(parameter); ++index)
        Require(parameter[index] == (index < 0x10 ? 0 : 0x5a), "a parameter lifecycle wrote the wrong bytes");
    Require(_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(&context) == 0, "the context did not terminate");
}

static void TestParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainerC1Ev,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainerD1Ev,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer9terminateEv,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setcontainerIdsEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const bool before = _ZNK3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer13isInitializedEv(parameter);
            const int result = present ? _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer10initializeEPNS1_6Common10LibContextE(parameter, context) : ParameterInvalidArgument;
            Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer13isInitializedEv(parameter) == (before || result == 0), "isInitialized disagrees with initialize");
            Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setserviceLabelEi(parameter, 1) == 0, "setserviceLabel failed");
            return result;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserC1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9terminateEv,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9setfieldsEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser10initializeEPNS1_6Common10LibContextEPKcSA_(parameter, context, "me", present ? "slot" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataC1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9terminateEv,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData11setobjectIdEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10initializeEPNS1_6Common10LibContextEPKci(parameter, context, present ? "me" : nullptr, 0);
        });
}

static IntrusivePtr body{reinterpret_cast<void*>(0x1000), nullptr, nullptr};
static IntrusivePtr missingBody{nullptr, nullptr, nullptr};

static void TestSessionManagerParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi31ParameterToCreatePlayerSessionsC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi31ParameterToCreatePlayerSessionsD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi31ParameterToCreatePlayerSessions9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const int result = _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi31ParameterToCreatePlayerSessions10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_29PostPlayerSessionsRequestBodyEEE(parameter, context, present ? &body : &missingBody);
            if (present || result != ParameterBodyMissing) return result;
            return ParameterInvalidArgument;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToJoinPlayerSessionAsPlayerC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToJoinPlayerSessionAsPlayerD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToJoinPlayerSessionAsPlayer9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const auto initialize = _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToJoinPlayerSessionAsPlayer10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_51PostPlayerSessionsSessionIdMemberPlayersRequestBodyEEE;
            if (present) return initialize(parameter, context, "session", &body);
            Require(initialize(parameter, context, "session", &missingBody) == ParameterBodyMissing, "initialize accepted a missing request body");
            return initialize(parameter, context, nullptr, &body);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29ParameterToLeavePlayerSessionC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29ParameterToLeavePlayerSessionD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29ParameterToLeavePlayerSession9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29ParameterToLeavePlayerSession10initializeEPNS1_6Common10LibContextEPKcSA_(parameter, context, "session", present ? "me" : nullptr);
        });

    std::uint64_t context = 0;
    alignas(16) unsigned char parameter[0x10];
    std::memset(parameter, 0x5a, sizeof(parameter));
    const auto initialize = _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38ParameterToGetPlayerSessionInvitations10initializeEPNS1_6Common10LibContextEPKc;
    const auto setFilter = _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38ParameterToGetPlayerSessionInvitations26setinvitationInvalidFilterEPKc;
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38ParameterToGetPlayerSessionInvitationsC1Ev(parameter) == 0, "the invitations parameter constructor failed");
    for (const auto byte : parameter) Require(byte == 0, "the invitations parameter constructor left bytes");
    Require(setFilter(parameter, "filter") == ParameterNotInitialized, "the filter setter accepted an uninitialized parameter");
    Require(initialize(parameter, nullptr, "me") == ParameterInvalidArgument, "the invitations initialize accepted a null context");
    Require(initialize(parameter, &context, "me") == ParameterNotInitialized, "the invitations initialize accepted an uninitialized context");
    Require(_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(nullptr, &context) == 0, "the context did not initialize");
    Require(initialize(parameter, &context, nullptr) == ParameterInvalidArgument, "the invitations initialize accepted a null account");
    Require(setFilter(parameter, "filter") == ParameterNotInitialized, "a failed invitations initialize left the parameter initialized");
    Require(initialize(parameter, &context, "me") == 0, "the invitations initialize failed");
    Require(initialize(parameter, &context, "me") == ParameterAlreadyInitialized, "the invitations initialize ran twice");
    Require(setFilter(parameter, nullptr) == ParameterInvalidArgument, "the filter setter accepted a null string");
    Require(setFilter(parameter, "filter") == 0, "the filter setter failed");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38ParameterToGetPlayerSessionInvitationsD1Ev(parameter) == 0, "the invitations parameter destructor failed");
    Require(_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(&context) == 0, "the context did not terminate");
}

static void TestLeaderboardsParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingC1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingD1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return present ? _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking10initializeEPNS1_6Common10LibContextEi(parameter, context, 7) : ParameterInvalidArgument;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionC1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionD1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return present ? _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition10initializeEPNS1_6Common10LibContextEi(parameter, context, 7) : ParameterInvalidArgument;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdC1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdD1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId10initializeEPNS1_6Common10LibContextEPKc(parameter, context, present ? "object" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataC1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataD1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData10initializeEPNS1_6Common10LibContextEiNS5_19XPsnAtomicOperationEPKc(parameter, context, 7, 0, present ? "object" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreC1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreD1Ev,
            _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const int result = _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore10initializeEPNS1_6Common10LibContextEiNS6_12IntrusivePtrINS3_22RecordScoreRequestBodyEEE(parameter, context, 7, present ? &body : &missingBody);
            if (present || result != ParameterBodyMissing) return result;
            return ParameterInvalidArgument;
        });
}

static void TestUserProfileParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriendsC1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriendsD1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends10initializeEPNS1_6Common10LibContextEPKc(parameter, context, present ? "me" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresencesC1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresencesD1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresences9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresences10initializeEPNS1_6Common10LibContextEPKc(parameter, context, present ? "1,2" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsersC1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsersD1Ev,
            _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return present ? _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers10initializeEPNS1_6Common10LibContextE(parameter, context) : ParameterInvalidArgument;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatusC1Ev,
            _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatusD1Ev,
            _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatus9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return present ? _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatus10initializeEPNS1_6Common10LibContextEm(parameter, context, 1) : ParameterInvalidArgument;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanityC1Ev,
            _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanityD1Ev,
            _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanity9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const auto initialize = _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanity10initializeEPNS1_6Common10LibContextEPKcSA_NS6_12IntrusivePtrINS3_19WebApiFilterRequestEEE;
            if (present) return initialize(parameter, context, "en-US", "0", &body);
            Require(initialize(parameter, context, "en-US", "0", &missingBody) == ParameterBodyMissing, "initialize accepted a missing filter request");
            Require(initialize(parameter, context, nullptr, "0", &body) == ParameterInvalidArgument, "initialize accepted a null locale");
            return initialize(parameter, context, "en-US", nullptr, &body);
        });
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

static const Size sizes[] = {
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE4sizeEv,
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE4sizeEv,
};

static const Element elements[] = {
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEixEm,
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEEixEm,
};

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void TestVectorSizeAndElements() {
    std::uint64_t context = 1;
    IntrusivePtr vector{nullptr, nullptr, &context};
    for (const auto size : sizes) Require(size(&vector) == 0, "a vector without a body has elements");
    for (const auto element : elements) Require(Throws([&] { element(&vector, 0); }), "indexing a vector without a body did not throw");

    alignas(8) std::int32_t first[4] = {1};
    alignas(8) std::int32_t second[4] = {1};
    const IntrusivePtr pushed[] = {{first, nullptr, &context}, {second, nullptr, &context}};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(&vector, &context);
    for (const auto& element : pushed)
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(&vector, &element) == 0, "pushBack failed");
    for (const auto size : sizes) Require(size(&vector) == 2, "a vector reported the wrong size");
    for (const auto element : elements) {
        Require(static_cast<IntrusivePtr*>(element(&vector, 0))->object == first, "operator[] returned the wrong first element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1))->object == second, "operator[] returned the wrong second element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1)) == static_cast<IntrusivePtr*>(element(&vector, 0)) + 1, "operator[] elements are not contiguous");
    }
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(&vector);
    Require(first[0] == 1 && second[0] == 1, "destroying the vector did not release its references");

    alignas(8) unsigned char foreignBody[0x50] = {};
    IntrusivePtr foreign{foreignBody, nullptr, &context};
    for (const auto size : sizes) Require(Throws([&] { size(&foreign); }), "sizing a foreign body did not throw");
    for (const auto element : elements) Require(Throws([&] { element(&foreign, 0); }), "indexing a foreign body did not throw");
}

int main() {
    TestParameterLifecycles();
    TestSessionManagerParameterLifecycles();
    TestLeaderboardsParameterLifecycles();
    TestUserProfileParameterLifecycles();
    TestIteratorDestructors();
    TestVectorSizeAndElements();
    std::puts("NpCppWebApi offline tests passed");
    return 0;
}
