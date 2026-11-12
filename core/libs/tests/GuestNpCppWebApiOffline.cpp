#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using SelfCall = int (APS5_VABI*)(void*);
using Size = std::size_t (APS5_VABI*)(const void*);
using Empty = bool (APS5_VABI*)(const void*);
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
            Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setserviceLabelEj(parameter, 1u) == 0, "setserviceLabel(unsigned) failed");
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

template <typename TInitialize>
static int InitializeSessionWithBody(TInitialize initialize, void* parameter, const std::uint64_t* context, bool present) {
    if (present) return initialize(parameter, context, "session", &body);
    Require(initialize(parameter, context, "session", &missingBody) == ParameterBodyMissing, "initialize accepted a missing request body");
    return initialize(parameter, context, nullptr, &body);
}

static void TestPlayerSessionParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessionsC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessionsD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessions9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessions10initializeEPNS1_6Common10LibContextEPKc(parameter, context, present ? "session" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitationsC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitationsD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitations9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return InitializeSessionWithBody(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitations10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_49PostPlayerSessionsSessionIdInvitationsRequestBodyEEE, parameter, context, present);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionPropertiesC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionPropertiesD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionProperties9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return InitializeSessionWithBody(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionProperties10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_39PatchPlayerSessionsSessionIdRequestBodyEEE, parameter, context, present);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeaderC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeaderD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeader9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return InitializeSessionWithBody(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeader10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_43PutPlayerSessionsSessionIdLeaderRequestBodyEEE, parameter, context, present);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsersC1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsersD1Ev,
            _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsers9terminateEv,
            nullptr,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return InitializeSessionWithBody(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsers10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_60PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEEE, parameter, context, present);
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

static const Empty empties[] = {
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE5emptyEv,
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE5emptyEv,
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
    for (const auto empty : empties) Require(empty(&vector) == true, "a vector without a body is not empty");
    for (const auto element : elements) Require(Throws([&] { element(&vector, 0); }), "indexing a vector without a body did not throw");

    alignas(8) std::int32_t first[4] = {1};
    alignas(8) std::int32_t second[4] = {1};
    alignas(8) std::int32_t third[4] = {1};
    const IntrusivePtr pushed[] = {{first, nullptr, &context}, {second, nullptr, &context}, {third, nullptr, &context}};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(&vector, &context);
    for (const auto empty : empties) Require(empty(&vector) == true, "a newly initialized vector is not empty");
    for (const auto& element : pushed)
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(&vector, &element) == 0, "pushBack failed");
    for (const auto size : sizes) Require(size(&vector) == 3, "a vector reported the wrong size");
    for (const auto empty : empties) Require(empty(&vector) == false, "a populated vector reported empty");
    for (const auto element : elements) {
        Require(static_cast<IntrusivePtr*>(element(&vector, 0))->object == first, "operator[] returned the wrong first element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1))->object == second, "operator[] returned the wrong second element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 2))->object == third, "operator[] returned the wrong third element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1)) == static_cast<IntrusivePtr*>(element(&vector, 0)) + 1, "operator[] elements are not contiguous");
        Require(static_cast<IntrusivePtr*>(element(&vector, 2)) == static_cast<IntrusivePtr*>(element(&vector, 1)) + 1, "operator[] elements are not contiguous");
    }
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(&vector);
    Require(first[0] == 1 && second[0] == 1 && third[0] == 1, "destroying the vector did not release its references");

    alignas(8) unsigned char foreignBody[0x50] = {};
    IntrusivePtr foreign{foreignBody, nullptr, &context};
    for (const auto size : sizes) Require(Throws([&] { size(&foreign); }), "sizing a foreign body did not throw");
    for (const auto empty : empties) Require(Throws([&] { empty(&foreign); }), "emptying a foreign body did not throw");
    for (const auto element : elements) Require(Throws([&] { element(&foreign, 0); }), "indexing a foreign body did not throw");
}

static void TestLeaderboardEntryAccessors() {
    alignas(16) unsigned char objects[3][0xd0];
    std::memset(objects, 0xa5, sizeof(objects));
    constexpr std::uint32_t values[] = {0, 1, 0x80000000u, 0xffffffffu, 0x12345678u};
    for (const auto value : values) {
        for (unsigned int flags = 0; flags < 8; ++flags) {
            for (unsigned int index = 0; index < 3; ++index) {
                auto* object = objects[index] + 0x10;
                const auto highest = value ^ (index * 0x13579bdu);
                const auto serial = ~highest;
                const auto highestSerial = highest ^ 0xa55a5aa5u;
                const auto pcBits = value ^ (index * 0x2468ace1u);
                std::int32_t pcId;
                std::memcpy(&pcId, &pcBits, sizeof(pcId));
                std::memcpy(object + 0x80, &pcId, sizeof(pcId));
                object[0x38] = (flags >> 2) & 1;
                std::memcpy(object + 0x3c, &highest, sizeof(highest));
                std::memcpy(object + 0x40, &highestSerial, sizeof(highestSerial));
                std::memcpy(object + 0x98, &serial, sizeof(serial));
                object[0x68] = flags & 1;
                object[0xb8] = (flags >> 1) & 1;
                unsigned char before[sizeof(objects)];
                std::memcpy(before, objects, sizeof(before));
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12commentIsSetEv(object) == ((flags & 4) != 0), "comment presence does not read its own flag");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry7getPcIdEv(object) == pcId, "PcId does not read its stored signed field");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14getHighestRankEv(object) == highest, "highest rank does not read the entry field");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13getSerialRankEv(object) == serial, "serial rank does not read the entry field");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry20getHighestSerialRankEv(object) == highestSerial, "highest serial rank does not read the entry field");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13objectIdIsSetEv(object) == ((flags & 1) != 0), "object ID presence does not read its own flag");
                Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14smallDataIsSetEv(object) == ((flags & 2) != 0), "small data presence does not read its own flag");
                Require(static_cast<const void*>(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry10getCommentEv(object)) == object + 0x18, "comment getter did not return its embedded object reference");
                Require(static_cast<const void*>(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry11getObjectIdEv(object)) == object + 0x48, "object ID getter did not return its embedded object reference");
                Require(static_cast<const void*>(_ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12getSmallDataEv(object)) == object + 0xa0, "small data getter did not return its embedded object reference");
                Require(std::memcmp(before, objects, sizeof(objects)) == 0, "entry accessors modified the object or adjacent storage");
            }
        }
    }
}


static void TestSessionManagerRemaining() {
    constexpr int kNotSignedIn = static_cast<int>(0x80552907);
    constexpr int kTransactionNotStarted = static_cast<int>(0x80553508);

    TestParameterLifecycle(
        {_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUserC1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUserD1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUser9terminateEv, nullptr},
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUser10initializeEPNS1_6Common10LibContextEPKc(parameter, context, present ? "account" : nullptr);
        });
    TestParameterLifecycle(
        {_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsersC1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsersD1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsers9terminateEv, nullptr},
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsers10initializeEPNS1_6Common10LibContextEPKcSA_(parameter, context, "session", present ? "user" : nullptr);
        });
    TestParameterLifecycle(
        {_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemPropertiesC1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemPropertiesD1Ev, _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemProperties9terminateEv, nullptr},
        [](void* parameter, const std::uint64_t* context, bool present) {
            const auto initialize = _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemProperties10initializeEPNS1_6Common10LibContextEPKcSA_NS6_12IntrusivePtrINS3_55PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEE;
            const int result = initialize(parameter, context, "session", "user", present ? &body : &missingBody);
            if (present || result != ParameterBodyMissing) return result;
            return ParameterInvalidArgument;
        });

    alignas(8) unsigned char player[0x170]{};
    *reinterpret_cast<std::uint32_t*>(player + 0x5c) = 8;
    player[0x60] = 1;
    *reinterpret_cast<std::uint32_t*>(player + 0x64) = 4;
    player[0x68] = 1;
    player[0x88] = 1;
    player[0x89] = 1;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead13getMaxPlayersEv(player) == 8, "the max player count is wrong");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead15maxPlayersIsSetEv(player), "the max player flag is clear");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead16getMaxSpectatorsEv(player) == 4, "the max spectator count is wrong");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead18maxSpectatorsIsSetEv(player), "the max spectator flag is clear");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead15getJoinDisabledEv(player), "the join-disabled flag is clear");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead11memberIsSetEv(player), "the member flag is clear");
    IntrusivePtr member{};
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead9getMemberEv(&member, player) == &member && member.object == nullptr, "the member handle is not empty");

    alignas(8) unsigned char sessionPlayer[0xa0]{};
    sessionPlayer[0x98] = 1;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer16customData1IsSetEv(sessionPlayer), "the game custom data flag is clear");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer14getCustomData1Ev(sessionPlayer) == sessionPlayer + 0x80, "the game custom data pointer is wrong");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer16getJoinTimestampEv(sessionPlayer) == sessionPlayer + 0x50, "the game join timestamp pointer is wrong");

    alignas(8) unsigned char memberObject[0x50]{};
    memberObject[0x28] = 1;
    memberObject[0x48] = 1;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead12playersIsSetEv(memberObject), "the players flag is clear");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead15spectatorsIsSetEv(memberObject), "the spectators flag is clear");
    IntrusivePtr players{};
    IntrusivePtr spectators{};
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead10getPlayersEv(&players, memberObject) == &players && players.object == nullptr, "the players handle is not empty");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead13getSpectatorsEv(&spectators, memberObject) == &spectators && spectators.object == nullptr, "the spectators handle is not empty");

    alignas(8) unsigned char joined[0x20]{};
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V131JoinedPlayerSessionWithPlatform12getSessionIdEv(joined) == joined + 0x10, "the joined session id pointer is wrong");

    alignas(8) unsigned char response[0x50]{};
    IntrusivePtr sessions{};
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBody17getPlayerSessionsEv(&sessions, response) == &sessions && sessions.object == nullptr, "the response sessions handle is not empty");

    IntrusivePtr vector{};
    CommonIterator first{}, last{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE5beginEv(&first, &vector);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE3endEv(&last, &vector);
    Require(first.element == nullptr && last.element == nullptr, "the empty player vector iterators are wrong");
    Require(!_ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEEneERKS9_(&first, &last), "the empty player vector iterators differ");

    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() == 0, "the transaction reports a response");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() == kTransactionNotStarted, "the transaction finish did not answer not-started");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29getJoinedPlayerSessionsByUserEiRKNS4_40ParameterToGetJoinedPlayerSessionsByUserERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_43GetUsersAccountIdPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(nullptr) == kNotSignedIn, "getJoinedPlayerSessionsByUser is not signed out");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38setPlayerSessionMemberSystemPropertiesEiRKNS4_49ParameterToSetPlayerSessionMemberSystemPropertiesERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(nullptr) == kNotSignedIn, "setPlayerSessionMemberSystemProperties is not signed out");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi41deletePlayerSessionJoinableSpecifiedUsersEiRKNS4_52ParameterToDeletePlayerSessionJoinableSpecifiedUsersERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(nullptr) == kNotSignedIn, "deletePlayerSessionJoinableSpecifiedUsers is not signed out");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V150PutPlayerSessionsSessionIdLeaderRequestBodyFactory6createEPNS1_6Common10LibContextERKmPKcPNS5_12IntrusivePtrINS3_43PutPlayerSessionsSessionIdLeaderRequestBodyEEE(nullptr) == kNotSignedIn, "the leader request factory is not signed out");

    _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession15setJoinDisabledERKb(nullptr, nullptr);
    _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody13setMaxPlayersERKi(nullptr, nullptr);
}

static void TestSmallInstantiations() {
    IntrusivePtr empty{};
    IntrusivePtr copied{};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEC1ERKS5_(&copied, &empty);
    Require(copied.object == nullptr, "the binary copy is not empty");
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEED1Ev(&copied);
    Require(!_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEcvbEv(&empty), "an empty restriction handle is true");

    alignas(8) unsigned char object[8]{};
    IntrusivePtr vector{};
    vector.object = object;
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEdeEv(&vector) == object, "the string vector operand is wrong");

    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() == 0, "the profanity transaction reports a response");
    alignas(8) unsigned char transaction[0x10]{};
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1ERKSB_(transaction) == 0, "the profanity transaction copy failed");
}

int main() {
    TestLeaderboardEntryAccessors();
    TestSessionManagerRemaining();
    TestSmallInstantiations();
    TestParameterLifecycles();
    TestSessionManagerParameterLifecycles();
    TestLeaderboardsParameterLifecycles();
    TestUserProfileParameterLifecycles();
    TestPlayerSessionParameterLifecycles();
    TestIteratorDestructors();
    TestVectorSizeAndElements();
    std::puts("NpCppWebApi offline tests passed");
    return 0;
}
