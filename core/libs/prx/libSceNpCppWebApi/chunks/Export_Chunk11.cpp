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

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V127ReportResultsRequestFactory6createEPNS1_6Common10LibContextENS5_12IntrusivePtrINS3_19RequestMatchResultsEEEPNS8_INS3_20ReportResultsRequestEEE(void* self, const IntrusivePtr* results, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x50, &DestroyMatchesModelMembers24And48);
    if (created != 0) return created;
    if (results == nullptr || results->object == nullptr) return ParameterBodyMissing;
    CopyIntrusivePtr(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(out->object) + 24), results, ReferenceCountOffset);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V127RequestPlayerResultsFactory6createEPNS1_6Common10LibContextEPKciPNS5_12IntrusivePtrINS3_20RequestPlayerResultsEEE(void* self, const char* name, std::int32_t rank, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x48, &DestroyMatchesModelString16);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V127RequestTeamStatisticFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_20RequestTeamStatisticEEE(void* self, const char* name, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x70, &DestroyRequestTeamStatistic);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    return AssignMatchesString(MatchesStringAt(out->object, 16), name);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V129RequestMatchStatisticsFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_22RequestMatchStatisticsEEE(void* self, IntrusivePtr* out) {
    return MatchesFactoryCreate(self, out, 0x50, &DestroyMatchesModelVectors16And48);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V129RequestPlayerStatisticFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEEPNSB_INS3_22RequestPlayerStatisticEEE(void* self, const char* name, const IntrusivePtr* statistics, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x48, &DestroyMatchesModelString16AndVector48);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return CopyMatchesVector(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(out->object) + 48), statistics);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V130RequestTeamMemberResultFactory6createEPNS1_6Common10LibContextEPKcdPNS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEE(void* self, const char* name, double score, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x38, &DestroyMatchesModelString16);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V131RequestCompetitiveResultFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24RequestCompetitiveResultEEE(void* self, IntrusivePtr* out) {
    return MatchesFactoryCreate(self, out, 0x50, &DestroyMatchesModelVectors16And48);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchDetailRequestFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24UpdateMatchDetailRequestEEE(void* self, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0xa8, &DestroyUpdateMatchDetailRequest);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 24), out->libContext);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchStatusRequestFactory6createEPNS1_6Common10LibContextENS3_12UpdateStatusEPNS5_12IntrusivePtrINS3_24UpdateMatchStatusRequestEEE(void* self, std::int32_t status, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x20, &DestroyMatchesModel);
    if (created != 0) return created;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V133RequestTeamMemberStatisticFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEEPNSB_INS3_26RequestTeamMemberStatisticEEE(void* self, const char* name, const IntrusivePtr* statistics, IntrusivePtr* out) {
    const int created = MatchesFactoryCreate(self, out, 0x48, &DestroyMatchesModelString16AndVector48);
    if (created != 0) return created;
    ConstructMatchesString(MatchesStringAt(out->object, 16), out->libContext);
    const int assigned = AssignMatchesString(MatchesStringAt(out->object, 16), name);
    if (assigned < 0) return assigned;
    return CopyMatchesVector(reinterpret_cast<IntrusivePtr*>(static_cast<std::uint8_t*>(out->object) + 48), statistics);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi10leaveMatchEiRKNS4_21ParameterToLeaveMatchERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi11createMatchEiRKNS4_22ParameterToCreateMatchERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_19CreateMatchResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi13reportResultsEiRKNS4_24ParameterToReportResultsERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi14getMatchDetailEiRKNS4_25ParameterToGetMatchDetailERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22GetMatchDetailResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi17updateMatchDetailEiRKNS4_28ParameterToUpdateMatchDetailERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi17updateMatchStatusEiRKNS4_28ParameterToUpdateMatchStatusERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(void* self, const std::uint64_t* context, const char* text, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, true, text, 0x40, 0x20, 0x38, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv(void* self) {
    return TerminateMatchesParameter(self, true, 0x20);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatchC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatchD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_17LeaveMatchRequestEEE(void* self, const std::uint64_t* context, const char* text, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, true, text, 0x40, 0x20, 0x38, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatch9terminateEv(void* self) {
    return TerminateMatchesParameter(self, true, 0x20);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatchC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatchD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_18CreateMatchRequestEEE(void* self, const std::uint64_t* context, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, false, nullptr, 0x20, 0x00, 0x18, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch9terminateEv(void* self) {
    return TerminateMatchesParameter(self, false, 0x00);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatchC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatchD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResults10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_20ReportResultsRequestEEE(void* self, const std::uint64_t* context, const char* text, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, true, text, 0x40, 0x20, 0x38, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResults9terminateEv(void* self) {
    return TerminateMatchesParameter(self, true, 0x20);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResultsC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResultsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* text) {
    return InitializeGetMatchDetailParameter(self, context, text);
}

void APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail17setnpServiceLabelEi(void* self, int label) {
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation == nullptr) return;
    auto* implementation = static_cast<std::uint8_t*>(parameter->implementation);
}

void APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail7setviewENS5_4ViewE(void* self, int view) {
    auto* parameter = static_cast<MatchesParameter*>(self);
    if (parameter->implementation == nullptr) return;
    auto* implementation = static_cast<std::uint8_t*>(parameter->implementation);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetail9terminateEv(void* self) {
    return TerminateGetMatchDetailParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetailC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi25ParameterToGetMatchDetailD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetail10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_24UpdateMatchDetailRequestEEE(void* self, const std::uint64_t* context, const char* text, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, true, text, 0x40, 0x20, 0x38, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetailC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetailD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatus10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_24UpdateMatchStatusRequestEEE(void* self, const std::uint64_t* context, const char* text, const IntrusivePtr* body) {
    return InitializeMatchesParameter(self, context, true, text, 0x40, 0x20, 0x38, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatus9terminateEv(void* self) {
    return TerminateMatchesParameter(self, true, 0x20);
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatusC1Ev(void* self) {
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatusD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi7Matches2V18MatchApi9joinMatchEiRKNS4_20ParameterToJoinMatchERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V119CreateMatchResponse10getMatchIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10teamsIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x48) != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster12playersIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x28) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse10getMatchIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x30;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse13getActivityIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x60;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse13getCustomDataEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x138;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi7Matches2V122GetMatchDetailResponse15customDataIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x158) != 0;
}


bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10priceIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xd8) != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku8getPriceEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0xd4);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku15getDisplayPriceEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0xe0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer7setsortENS5_4SortE() {
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku18originalPriceIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x108) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product14getDescriptionEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x130;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product14getReleaseDateEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x80;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku23getDisplayOriginalPriceEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x110;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product14getDisplayNameEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x60;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku16getOriginalPriceEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x104);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku25displayOriginalPriceIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x130) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku8getLabelEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x60;
}

int APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer9setoffsetEi() {
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product16releaseDateIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0xa0) != 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setcontainerIdsEPKc(void* self, const char* containerIds) {
    return SetParameterText(self, containerIds);
}

int APS5_VABI _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer8setlimitEi() {
    return 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku17displayPriceIsSetEv(const void* self) {
    return *reinterpret_cast<const std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x100) != 0;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi13InGameCatalog2V57Product5getIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6VectorINS4_INS1_13InGameCatalog2V59ContainerEEEEEEENS4_INS2_18ResponseHeaderBaseEEEE11hasResponseEv() {
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS2_6VectorINS4_INS1_13InGameCatalog2V59ContainerEEEEEEENS4_INS2_18ResponseHeaderBaseEEEEaSERKSE_(void* self, const void* source) {
    auto* dst = static_cast<std::uint64_t*>(self);
    const auto* src = static_cast<const std::uint64_t*>(source);
    dst[0] = src[0];
    dst[1] = src[1];
    return self;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE5emptyEv(const void* self) {
    return HostVectorSize(self, __func__) == 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V57ProductEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_13InGameCatalog2V53SkuEEptEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEC1ERKSA_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, 0x30);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS2_6VectorINS4_INS1_13InGameCatalog2V59ContainerEEEEEEENS4_INS2_18ResponseHeaderBaseEEEEC1ERKSE_(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE5beginEv(void* out, void* self) {
    return HostVectorBegin(out, self, 1, __func__);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE3endEv(void* out, void* self) {
    return HostVectorEnd(out, self, 1, sizeof(IntrusivePtr), __func__);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V53SkuEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEEppEv(void* self) {
    return HostIteratorAdvance(self, sizeof(IntrusivePtr));
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE5emptyEv(const void* self) {
    return HostVectorSize(self, __func__) == 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEaSERKSA_(IntrusivePtr* self, const IntrusivePtr* source) {
    AssignIntrusivePtr(self, source, 0x30, __func__);
}

}
