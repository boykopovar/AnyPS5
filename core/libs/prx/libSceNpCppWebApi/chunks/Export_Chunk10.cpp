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

static void APS5_VABI DestroyUpdateMatchDetailRequest(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 24));
    ReleaseMatchesMember(object, 112, ReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyRequestMatchResults(void* object) {
    ReleaseMatchesMember(object, 32, ReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelString16AndVector48(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    ReleaseMatchesMember(object, 48, VectorReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelString16AndVector72(void* object) {
    DestroyMatchesString(MatchesStringAt(object, 16));
    ReleaseMatchesMember(object, 72, VectorReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelVector24(void* object) {
    ReleaseMatchesMember(object, 24, VectorReferenceCountOffset);
    std::free(object);
}

static void APS5_VABI DestroyMatchesModelMembers24And48(void* object) {
    ReleaseMatchesMember(object, 24, ReferenceCountOffset);
    ReleaseMatchesMember(object, 48, ReferenceCountOffset);
    std::free(object);
}

static int MatchesFactoryCreate(const void* context, IntrusivePtr* out, const std::size_t size, void (APS5_VABI* deleter)(void*)) {
    if (context == nullptr || out == nullptr) return ParameterInvalidArgument;
    void* object = std::calloc(1, size);
    if (object == nullptr) return MatchesOutOfMemory;
    ReferenceCount(object, ReferenceCountOffset).store(1);
    try {
        ReleaseIntrusivePtr(out, ReferenceCountOffset, __func__);
    } catch (...) {
        std::free(object);
        throw;
    }
    out->object = object;
    out->deleter = deleter;
    out->libContext = const_cast<void*>(context);
    return 0;
}

static int CopyMatchesVector(IntrusivePtr* member, const IntrusivePtr* source) {
    if (source == nullptr) return ParameterInvalidArgument;
    if (source->object == nullptr) return 0;
    const IntrusivePtr* begin;
    const IntrusivePtr* end;
    const auto* body = static_cast<const VectorBody*>(source->object);
    if (body->magic == VectorBodyMagic && body->elementKind == 1) {
        begin = static_cast<const IntrusivePtr*>(body->elements);
        end = begin + body->size;
    } else {
        const auto* vector = reinterpret_cast<const std::uint8_t*>(source);
        begin = *reinterpret_cast<IntrusivePtr* const*>(vector + 0x18);
        end = *reinterpret_cast<IntrusivePtr* const*>(vector + 0x28);
    }
    for (const IntrusivePtr* element = begin; element != end; ++element) {
        if (element->object == nullptr) return ParameterInvalidArgument;
        if (VectorPushBackIntrusivePtr(member, element) != 0) return MatchesOutOfMemory;
    }
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer12setAccountIdEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 96), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer13setPlayerNameEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 56), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer9setTeamIdEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 136), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V113RemovedPlayer9setReasonERKNS3_11LeaveReasonE(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V116JoinMatchRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 88), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam11setTeamNameEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 48), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V117LeaveMatchRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118AddedPlayerFactory6createEPNS1_6Common10LibContextEPKcNS3_10PlayerTypeEPNS5_12IntrusivePtrINS3_11AddedPlayerEEE(void* self, const char* name, std::int32_t playerType, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0xb0, &DestroyAddedPlayer);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 56), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 96), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 136), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest13setCustomDataEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 0x70), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr || value->object == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 152), value, ReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setExpirationTimeERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest9setZoneIdEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 56), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer12setAccountIdEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 96), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer13setPlayerNameEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 56), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 72), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults8setScoreERKd(void* self, const double* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 16), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 48), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCompetitiveResultERKNS1_6Common12IntrusivePtrINS3_24RequestCompetitiveResultEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr || value->object == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 32), value, ReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCooperativeResultERKNS3_24RequestCooperativeResultE(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120RemovedPlayerFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RemovedPlayerEEE(void* self, const char* name, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x38, &DestroyMatchesModelString16);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    return AssignMatchesString(MatchesStringAt(out->object, 16), name);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest18setMatchStatisticsERKNS1_6Common12IntrusivePtrINS3_22RequestMatchStatisticsEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr || value->object == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 48), value, ReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120RequestMemberFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RequestMemberEEE(void* self, const char* name, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x30, &DestroyMatchesModelString16);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    return AssignMatchesString(MatchesStringAt(out->object, 16), name);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120RequestPlayerResults8setScoreERKd(void* self, const double* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 80), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 48), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 48), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 16), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V123JoinMatchRequestFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_11AddedPlayerEEEEEPNS9_INS3_16JoinMatchRequestEEE(void* self, const IntrusivePtr* players, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x30, &DestroyMatchesModelVector24);
    if (created != 0) return created;
    return CopyMatchesVector(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(out->object) + 24), players);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V123RequestMatchTeamFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_16RequestMatchTeamEEE(void* self, const char* name, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x78, &DestroyRequestMatchTeam);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 48), out->libContext);
    return AssignMatchesString(MatchesStringAt(out->object, 16), name);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124LeaveMatchRequestFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_13RemovedPlayerEEEEEPNS9_INS3_17LeaveMatchRequestEEE(void* self, const IntrusivePtr* players, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x30, &DestroyMatchesModelVector24);
    if (created != 0) return created;
    return CopyMatchesVector(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(out->object) + 24), players);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 48), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 16), value, VectorReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest13setCustomDataEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 0x48), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(void* self, const IntrusivePtr* value) {
    if (value == nullptr || value->object == nullptr) return ParameterBodyMissing;
    AssignIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(self) + 112), value, ReferenceCountOffset, __func__);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setExpirationTimeERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest9setZoneIdEPKc(void* self, const char* value) {
    const int result = AssignMatchesString(MatchesStringAt(self, 24), value);
    if (result < 0) return result;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchStatusRequest17setNpServiceLabelERKi(void* self, const int* value) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE(void* self, const char* name, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0xb8, &DestroyCreateMatchRequest);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 24), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 56), out->libContext);
    return AssignMatchesString(MatchesStringAt(out->object, 24), name);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V125RequestMatchPlayerFactory6createEPNS1_6Common10LibContextEPKcNS3_10PlayerTypeEPNS5_12IntrusivePtrINS3_18RequestMatchPlayerEEE(void* self, const char* name, std::int32_t playerType, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x88, &DestroyRequestMatchPlayer);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 56), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 96), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V125RequestTeamResultsFactory6createEPNS1_6Common10LibContextEPKciPNS5_12IntrusivePtrINS3_18RequestTeamResultsEEE(void* self, const char* name, std::int32_t rank, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x68, &DestroyMatchesModelString16AndVector72);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V126AdditionalStatisticFactory6createEPNS1_6Common10LibContextEPKcS9_PNS5_12IntrusivePtrINS3_19AdditionalStatisticEEE(void* self, const char* name, const char* value, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x50, &DestroyMatchesModelStrings16And48);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    ConstructMatchesString(MatchesStringAt(out->object, 48), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return AssignMatchesString(MatchesStringAt(out->object, 48), value);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V126RequestInGameRosterFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_19RequestInGameRosterEEE(void* self, IntrusivePtr* out) {
    return MatchesFactoryCreate(self, out, 0x50, &DestroyMatchesModelVectors16And48);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V126RequestMatchResultsFactory6createEPNS1_6Common10LibContextENS3_14ResultsVersionEPNS5_12IntrusivePtrINS3_19RequestMatchResultsEEE(void* self, std::int32_t version, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x40, &DestroyRequestMatchResults);
    if (created != 0) return created;
    return 0;
}

}
