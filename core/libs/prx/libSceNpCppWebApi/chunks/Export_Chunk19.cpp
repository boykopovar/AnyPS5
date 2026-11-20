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

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V126PlayerSessionMemberForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V129GetPlayerSessionsResponseBodyEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V132RequestCreatePlayerSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V143GetUsersAccountIdPlayerSessionsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V155PatchPlayerSessionsSessionIdMembersAccountIdRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V119PlayerSessionPlayerEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V122PlayerSessionSpectatorEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEEEEptEv(const IntrusivePtr* self) {
    return self->object;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEE4sizeEv(const void* self) {
    return HostVectorSize(self, __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEC1ERKS5_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_30CommunicationRestrictionStatus2V338CommunicationRestrictionStatusResponseEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}


bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V120GetOfferResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121GetTicketResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V18LocationEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V18LocationEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19AttributeEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19SubmitterEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V19SubmitterEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V113PlayerForReadEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V113PlayerForReadEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V118PlayerForOfferReadEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V118PlayerForOfferReadEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V19AttributeEEEEEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_11Matchmaking2V19AttributeEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEEdeEv(const void* self) {
    return static_cast<const CommonConstIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEEneERKS9_(const void* self, const void* other) {
    return static_cast<const CommonConstIterator*>(self)->element !=
           static_cast<const CommonConstIterator*>(other)->element;
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEE3endEv(const void* self) {
    const VectorBody* body = HostVectorBody(self, 1, __func__);
    return CommonConstIterator(0, body != nullptr && body->elements != nullptr ? static_cast<std::uint8_t*>(body->elements) + body->size * sizeof(IntrusivePtr) : nullptr);
}

CommonConstIterator APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEE5beginEv(const void* self) {
    const VectorBody* body = HostVectorBody(self, 1, __func__);
    return CommonConstIterator(0, body != nullptr ? body->elements : nullptr);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi22sendGameSessionMessageEiRKNS4_33ParameterToSendGameSessionMessageERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessage10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_50PostGameSessionsSessionIdSessionMessageRequestBodyEEE(void* self, const std::uint64_t* context, const char* sessionId, ...) {
    return InitializeParameter(self, context, sessionId != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessage9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessageC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V115GameSessionsApi33ParameterToSendGameSessionMessageD1Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V124GameSessionMemberForRead10getPlayersEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x10, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V130MemberWithMultiPlatformFactory6createEPNS1_6Common10LibContextERKmPKcPNS5_12IntrusivePtrINS3_23MemberWithMultiPlatformEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBody14getInvitationsEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi14SessionManager2V157PostGameSessionsSessionIdSessionMessageRequestBodyFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_23MemberWithMultiPlatformEEEEEPKcPNS9_INS3_50PostGameSessionsSessionIdSessionMessageRequestBodyEEE() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124GameSessionMemberForReadEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdSessionMessageRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x00);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdSessionMessageRequestBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdSessionMessageRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V117GameSessionPlayerEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEED2Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEED2Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEED2Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEED2Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x00, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEEE8pushBackERKS8_(IntrusivePtr* self, const void* element) {
    return VectorPushBackIntrusivePtr(self, static_cast<const IntrusivePtr*>(element));
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V123MemberWithMultiPlatformEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V114Representative11getPlatformEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x30);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer11getPlatformEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x30);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer12getJoinStateEv(const void* self) {
    return *reinterpret_cast<const std::int32_t*>(static_cast<const std::uint8_t*>(self) + 0x78);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer14joinStateIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x7c] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead11memberIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x88] != 0;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead12getSessionIdEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead14sessionIdIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x30] != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead9getMemberEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x70, ReferenceCountOffset);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead12getSessionIdEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

int APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead19getJoinableUserTypeEv(const void* self) {
    return *reinterpret_cast<const std::int32_t*>(static_cast<const std::uint8_t*>(self) + 0x134);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead21joinableUserTypeIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x138] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V124GameSessionMemberForRead12playersIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x28] != 0;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V131ResponsePlayerSessionInvitation15getInvitationIdEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBody16invitationsIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x48] != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V150PostGameSessionsSessionIdMemberPlayersResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}
