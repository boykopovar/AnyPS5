extern "C" {
#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN = static_cast<int>(0x80552907);
static constexpr std::size_t ReferenceCountOffset = 0;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;
static constexpr int TransactionNotStarted = static_cast<int>(0x80553508);

// Offline PSN: request entry points (API calls, transaction start/readData, factories) fail with
// SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN, the constant shadPS4/fpPS4 document for this code; parameter
// bookkeeping succeeds; the audited accessors read the measured offsets and the rest stay
// unimplemented because a failed request never produces a response.

static void ConstructParameter(void* self);
static int InitializeParameter(void* self, const std::uint64_t* context, bool argumentsPresent);
static int InitializeParameterWithBody(void* self, const std::uint64_t* context, bool textsPresent, const IntrusivePtr* body);
static int TerminateParameter(void* self);

static constexpr int ParameterNotInitialized = static_cast<int>(0x80553502);
static constexpr int ParameterInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ParameterAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ParameterTerminatedUninitialized = static_cast<int>(0x80553510);
static constexpr int ParameterBodyMissing = static_cast<int>(0x80553512);

struct ParameterObject {
    void* base;
    const void* implementation;
};

static constexpr unsigned char ParameterImplementation = 0;

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi26getMultiDataStatusesByUserEiRKNS4_37ParameterToGetMultiDataStatusesByUserERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_32GetMultiDataStatusesResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V110DataStatus11getDataSizeEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

const char* APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V110DataStatus11getObjectIdEv(const void* self) {
    return reinterpret_cast<const char*>(static_cast<const std::uint8_t*>(self) + 0x70);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V110DataStatus8getOwnerEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x98, ReferenceCountOffset);
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V110DataStatus9getSlotIdEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0xb4);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V132GetMultiDataStatusesResponseBody17getDataStatusListEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V132GetMultiDataStatusesResponseBody23getTotalDataStatusCountEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x5c);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi17TitleCloudStorage2V15Owner12getAccountIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi10getFriendsEiRKNS4_21ParameterToGetFriendsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_18GetFriendsResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* accountId) {
    return InitializeParameter(self, context, accountId != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriends9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriendsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V110FriendsApi21ParameterToGetFriendsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi17getBasicPresencesEiRKNS4_28ParameterToGetBasicPresencesERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_25GetBasicPresencesResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresences10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* accountIds) {
    return InitializeParameter(self, context, accountIds != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresences9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresencesC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V111PresenceApi28ParameterToGetBasicPresencesD1Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V118GetFriendsResponse10getFriendsEv(void* self) {
    return static_cast<std::uint8_t*>(self) + 0x30;
}

// --------------------------------------------------------------------------
// SessionManager2V1 PlayerSessionsApi + InGameCatalog2V5 ContainerApi and Common
// container helpers imported by titles whose dumps lack libSceNpCppWebApi.prx:
// request entry points and transactions fail SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN
// offline; parameter bookkeeping succeeds; response accessors stay unimplemented.
// --------------------------------------------------------------------------

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V149PostPlayerSessionsSessionIdInvitationsRequestBody14setInvitationsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_30RequestPlayerSessionInvitationEEEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28sendPlayerSessionInvitationsEiRKNS4_39ParameterToSendPlayerSessionInvitationsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_50PostPlayerSessionsSessionIdInvitationsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi17getPlayerSessionsEiRKNS4_28ParameterToGetPlayerSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_29GetPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession14setCustomData2EPKvm() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsers10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_60PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEEE(void* self, const std::uint64_t* context, const char* sessionId, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, sessionId != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionProperties10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_39PatchPlayerSessionsSessionIdRequestBodyEEE(void* self, const std::uint64_t* context, const char* sessionId, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, sessionId != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeader9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionPropertiesC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsers9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession14setCustomData1EPKvm() {
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead16customData2IsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x200) != 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V124GetBlockingUsersResponse9getBlocksEv(void* self) {
    return static_cast<std::uint8_t*>(self) + 0x30;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionPropertiesD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitations10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_49PostPlayerSessionsSessionIdInvitationsRequestBodyEEE(void* self, const std::uint64_t* context, const char* sessionId, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, sessionId != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V146PatchPlayerSessionsSessionIdRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_39PatchPlayerSessionsSessionIdRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137RequestPlayerSessionInvitationFactory6createEPNS1_6Common10LibContextENS5_12IntrusivePtrINS3_2ToEEEPNS8_INS3_30RequestPlayerSessionInvitationEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody19setJoinableUserTypeERKNS3_22CustomJoinableUserTypeE() {
    return 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V127ResponsePlayerSessionPlayer11getPlatformEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x18;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V125GetBasicPresencesResponse17getBasicPresencesEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessions9setfieldsENS1_6Common12IntrusivePtrINS6_6VectorINS6_6StringEEEEE() {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessions9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29ParameterToLeavePlayerSession9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitationsD1Ev(void* self) {
    (void)self;
    return 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead14getCustomData1Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x1c8;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi16getBlockingUsersEiRKNS4_27ParameterToGetBlockingUsersERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_24GetBlockingUsersResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers10initializeEPNS1_6Common10LibContextE(void* self, const std::uint64_t* context) {
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsers9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsersC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi31ParameterToCreatePlayerSessions9terminateEv(void* self) {
    return TerminateParameter(self);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead16customData1IsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x1e0) != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38addPlayerSessionJoinableSpecifiedUsersEiRKNS4_49ParameterToAddPlayerSessionJoinableSpecifiedUsersERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_61PostPlayerSessionsSessionIdJoinableSpecifiedUsersResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead9getLeaderEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xf8, ReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitationsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessionsD1Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead28getExclusiveLeaderPrivilegesEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x188, StringVectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi26setPlayerSessionPropertiesEiRKNS4_37ParameterToSetPlayerSessionPropertiesERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi25changePlayerSessionLeaderEiRKNS4_36ParameterToChangePlayerSessionLeaderERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V150PutPlayerSessionsSessionIdLeaderRequestBodyFactory6createEPNS1_6Common10LibContextERKNS_4Json5ValueEPNS5_12IntrusivePtrINS3_43PutPlayerSessionsSessionIdLeaderRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V119JoinableUserFactory6createEPNS1_6Common10LibContextERKmPNS5_12IntrusivePtrINS3_12JoinableUserEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeaderD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V156PostPlayerSessionsSessionIdInvitationsRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_49PostPlayerSessionsSessionIdInvitationsRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi39ParameterToSendPlayerSessionInvitations9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessions10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* sessionIds) {
    return InitializeParameter(self, context, sessionIds != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi28ParameterToGetPlayerSessionsC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V133ResponsePlayerSessionMemberPlayer10getPlayersEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x10, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody28setExclusiveLeaderPrivilegesERKNS1_6Common6VectorINS5_6StringEEE() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V113PlayerSession9getMemberEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, ReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V19ToFactory6createEPNS1_6Common10LibContextERKmPNS5_12IntrusivePtrINS3_2ToEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsersD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeader10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_43PutPlayerSessionsSessionIdLeaderRequestBodyEEE(void* self, const std::uint64_t* context, const char* sessionId, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, sessionId != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V167PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_12JoinableUserEEEEEPNS9_INS3_60PostPlayerSessionsSessionIdJoinableSpecifiedUsersRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi36ParameterToChangePlayerSessionLeaderC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi37ParameterToSetPlayerSessionProperties9terminateEv(void* self) {
    return TerminateParameter(self);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118LeaderWithOnlineId12getAccountIdEv(const void* self) {
    std::uint64_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(self) + 0x10, sizeof(value));
    return value;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody20setInvitableUserTypeERKNS3_23CustomInvitableUserTypeE() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V129GetPlayerSessionsResponseBody17getPlayerSessionsEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToAddPlayerSessionJoinableSpecifiedUsersC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead14getCustomData2Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x1e8;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V127ResponsePlayerSessionPlayer12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V130PostPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V112JoinableUserEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11UserProfile2V19BlocksApi27ParameterToGetBlockingUsersD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi15filterProfanityEiRKNS4_26ParameterToFilterProfanityERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_23FilterProfanityResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanity10initializeEPNS1_6Common10LibContextEPKcSA_NS6_12IntrusivePtrINS3_19WebApiFilterRequestEEE(void* self, const std::uint64_t* context, const char* locale, const char* serviceLabel, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, locale != nullptr && serviceLabel != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanity9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanityC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V212ProfanityApi26ParameterToFilterProfanityD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi15ProfanityFilter2V226WebApiFilterRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_19WebApiFilterRequestEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi33getCommunicationRestrictionStatusEiRKNS4_44ParameterToGetCommunicationRestrictionStatusERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_38CommunicationRestrictionStatusResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatus10initializeEPNS1_6Common10LibContextEm(void* self, const std::uint64_t* context, std::uint64_t accountId) {
    (void)accountId;
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatus9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatusC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi30CommunicationRestrictionStatus2V333CommunicationRestrictionStatusApi44ParameterToGetCommunicationRestrictionStatusD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V118GetFriendsResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V124GetBlockingUsersResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_11UserProfile2V125GetBasicPresencesResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE21setOnFinishedCallbackEPFvliS8_PvEPFvliSC_ESC_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

}
