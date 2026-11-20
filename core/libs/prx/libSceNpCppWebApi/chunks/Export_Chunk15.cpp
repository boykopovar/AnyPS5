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

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V121PlayerForTicketCreateEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V121PlayerForTicketCreateEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEEixEm(void* self, std::uint64_t index) {
    return HostVectorElement(self, index, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V128RequestJoinGameSessionPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V128RequestJoinGameSessionPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V128RequestJoinGameSessionPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V111AddedPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V111AddedPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V111AddedPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEE8pushBackERKS8_(IntrusivePtr* self, const IntrusivePtr* element) {
    return VectorPushBackIntrusivePtr(self, element);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext) {
    self->object = nullptr;
    self->deleter = nullptr;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, 0x30, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE7reserveEi(IntrusivePtr* self, const std::int32_t count) {
    if (count < 0) return 0;
    auto* body = VectorBodyFor(self, 2, __func__);
    if (body == nullptr) return VectorOutOfMemory;
    if (body->size > static_cast<std::uint64_t>(count)) return 0;
    if (!VectorReserve(body, sizeof(CommonString), static_cast<std::uint64_t>(count))) return VectorOutOfMemory;
    return 0;
}



















































































const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail11getLastNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x60;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail12getFirstNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail13getMiddleNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x38;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail13lastNameIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x80) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail14firstNameIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x30) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi11UserProfile2V114PersonalDetail15middleNameIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x58) != 0;
}













const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V111GameSession12getSessionIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V114Representative11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x18);
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V114Representative12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead14getCustomData1Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0xd0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead14getCustomData2Ev(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0xf0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead14getMatchmakingEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x110, ReferenceCountOffset);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead16customData1IsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xe8) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead16customData2IsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x108) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead16matchmakingIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x128) != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead17getRepresentativeEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xb0, ReferenceCountOffset);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead19representativeIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xc8) != 0;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118LeaderWithOnlineId11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x18);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V118MatchmakingForRead10getOfferIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead11leaderIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x110) != 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_15ProfanityFilter2V223FilterProfanityResponseEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V121PlayerForTicketCreateEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V123SubmitTicketRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_11Matchmaking2V124SubmitTicketResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEE3getEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V111GameSessionEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V114RepresentativeEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118MatchmakingForReadEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V118RequestGameSessionEEptEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V122GameSessionPushContextEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124PlayerSessionPushContextEEcvbEv(const IntrusivePtr* self) {
    return self->object != nullptr;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V124RequestGameSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V127GetGameSessionsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V128PostGameSessionsResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V128RequestJoinGameSessionPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V137PatchGameSessionsSessionIdRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi25UploadDataResponseHeadersEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V111AddedPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V113RemovedPlayerEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V116JoinMatchRequestEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEptEv(const IntrusivePtr* self) {
    return self->object;
}

}
