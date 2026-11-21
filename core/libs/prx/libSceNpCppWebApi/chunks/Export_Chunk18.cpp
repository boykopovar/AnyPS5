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

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi26ParameterToGetGameSessions18setjoinStateFilterEPKc(void* self, const char* text) {
    return SetParameterText(self, text);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi29getJoinedPlayerSessionsByUserEiRKNS4_40ParameterToGetJoinedPlayerSessionsByUserERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_43GetUsersAccountIdPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi38setPlayerSessionMemberSystemPropertiesEiRKNS4_49ParameterToSetPlayerSessionMemberSystemPropertiesERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUser10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* accountId) {
    return InitializeParameter(self, context, accountId != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUser9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUserC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi40ParameterToGetJoinedPlayerSessionsByUserD1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi41deletePlayerSessionJoinableSpecifiedUsersEiRKNS4_52ParameterToDeletePlayerSessionJoinableSpecifiedUsersERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemProperties10initializeEPNS1_6Common10LibContextEPKcSA_NS6_12IntrusivePtrINS3_55PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEE(void* self, const std::uint64_t* context, const char* first, const char* second, const IntrusivePtr* body) {
    return InitializeParameterWithBody(self, context, first != nullptr && second != nullptr, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemProperties9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemPropertiesC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi49ParameterToSetPlayerSessionMemberSystemPropertiesD1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsers10initializeEPNS1_6Common10LibContextEPKcSA_(void* self, const std::uint64_t* context, const char* first, const char* second) {
    return InitializeParameter(self, context, first != nullptr && second != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsers9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsersC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi52ParameterToDeletePlayerSessionJoinableSpecifiedUsersD1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead21getSupportedPlatformsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x90, 0x38);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead21getSupportedPlatformsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x90, 0x38);
}

void APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession15setJoinDisabledERKb(void* self, const bool* value) {
    (void)self;
    (void)value;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V120RequestPlayerSession22setDisableSystemUiMenuERKNS1_6Common6VectorINS5_6StringEEE(void* self, const IntrusivePtr* value) {
    (void)self;
    (void)value;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V124RequestGameSessionPlayer14setCustomData1EPKvm(void* self, const void* data, std::size_t size) {
    (void)self;
    (void)data;
    (void)size;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead10getPlayersEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x10, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead13getSpectatorsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V128RequestJoinGameSessionPlayer14setCustomData1EPKvm(void* self, const void* data, std::size_t size) {
    (void)self;
    (void)data;
    (void)size;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V132RequestCreatePlayerSessionPlayer14setCustomData1EPKvm(void* self, const void* data, std::size_t size) {
    (void)self;
    (void)data;
    (void)size;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V137PatchGameSessionsSessionIdRequestBody13setMaxPlayersERKi(void* self, const int* value) {
    (void)self;
    (void)value;
}

void APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody13setMaxPlayersERKi(void* self, const int* value) {
    (void)self;
    (void)value;
}

void APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody15setJoinDisabledERKb(void* self, const bool* value) {
    (void)self;
    (void)value;
}

void APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody16setSwapSupportedERKb(void* self, const bool* value) {
    (void)self;
    (void)value;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V139PatchPlayerSessionsSessionIdRequestBody22setDisableSystemUiMenuERKNS1_6Common6VectorINS5_6StringEEE(void* self, const IntrusivePtr* value) {
    (void)self;
    (void)value;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBody17getPlayerSessionsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V150PutPlayerSessionsSessionIdLeaderRequestBodyFactory6createEPNS1_6Common10LibContextERKmPKcPNS5_12IntrusivePtrINS3_43PutPlayerSessionsSessionIdLeaderRequestBodyEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V155PatchPlayerSessionsSessionIdMembersAccountIdRequestBody14setCustomData1EPKvm(void* self, const void* data, std::size_t size) {
    (void)self;
    (void)data;
    (void)size;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V162PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_55PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V126PlayerSessionMemberForReadEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEEC1Ev(IntrusivePtr* self) {
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V119PlayerSessionPlayerEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V122PlayerSessionSpectatorEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5abortEv() {
    return TransactionNotStarted;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V155PostPlayerSessionsSessionIdMemberSpectatorsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
    return TransactionNotStarted;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE3endEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, true, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE5beginEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, false, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEE3endEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, true, __func__);
    return iterator;
}

CommonIterator* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEE5beginEv(CommonIterator* iterator, const IntrusivePtr* self) {
    iterator->referenceCount = 0;
    iterator->element = IntrusiveVectorElements(self, false, __func__);
    return iterator;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x18);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer14getCustomData1Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x80;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer16customData1IsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x98] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer16getJoinTimestampEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x50;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead13getMaxPlayersEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x5c);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead15maxPlayersIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x60] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead19getCreatedTimestampEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x38;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead21createdTimestampIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x58] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead23supportedPlatformsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0xa8] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118LeaderWithOnlineId11getPlatformEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x18);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer11getPlatformEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer14getCustomData1Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x70;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer16customData1IsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x88] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V119PlayerSessionPlayer16getJoinTimestampEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x50;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead11memberIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x88] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead13getMaxPlayersEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x5c);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead14sessionIdIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x30] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead15getJoinDisabledEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x89] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead15maxPlayersIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x60] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead16getMaxSpectatorsEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x64);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead18maxSpectatorsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x68] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead19getCreatedTimestampEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x38;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead20getInvitableUserTypeEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x15c);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead21createdTimestampIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x58] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead23supportedPlatformsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0xa8] != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead9getMemberEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x70, 0x0);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x18);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator11getPlatformEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator14getCustomData1Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x70;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator16customData1IsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x88] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V122PlayerSessionSpectator16getJoinTimestampEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x50;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead12playersIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x28] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V126PlayerSessionMemberForRead15spectatorsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x48] != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V131JoinedPlayerSessionWithPlatform12getSessionIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

}
