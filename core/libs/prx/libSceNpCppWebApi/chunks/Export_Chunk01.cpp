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

static void ConstructParameter(void* self) {
}

static int InitializeParameter(void* self, const std::uint64_t* context, const bool argumentsPresent) {
    if (context == nullptr) return ParameterInvalidArgument;
    if (*context == 0) return ParameterNotInitialized;
    auto* parameter = static_cast<ParameterObject*>(self);
    if (parameter->implementation != nullptr) return ParameterAlreadyInitialized;
    if (!argumentsPresent) return ParameterInvalidArgument;
    parameter->implementation = &ParameterImplementation;
    return 0;
}

static int SetParameterText(const void* self, const char* text) {
    if (static_cast<const ParameterObject*>(self)->implementation == nullptr) return ParameterNotInitialized;
    return text == nullptr ? ParameterInvalidArgument : 0;
}

static int TerminateParameter(void* self) {
    auto* parameter = static_cast<ParameterObject*>(self);
    if (parameter->implementation == nullptr) return ParameterTerminatedUninitialized;
    parameter->implementation = nullptr;
    return 0;
}

static int InitializeParameterWithBody(void* self, const std::uint64_t* context, const bool textsPresent, const IntrusivePtr* body) {
    const int result = InitializeParameter(self, context, textsPresent);
    if (result != 0 || (body != nullptr && body->object != nullptr)) return result;
    static_cast<ParameterObject*>(self)->implementation = nullptr;
    return ParameterBodyMissing;
}

static constexpr int ParameterOutOfMemory = static_cast<int>(0x80553504);

static constexpr std::size_t BoardDefinitionImplementationSize = 0x40;
static constexpr std::size_t ContainerImplementationSize = 0x88;

static CommonString* CommonStringAt(void* implementation, const std::size_t offset) {
    return reinterpret_cast<CommonString*>(static_cast<std::uint8_t*>(implementation) + offset);
}

static void ConstructCommonString(CommonString* self, void* libContext) {
    self->referenceCount = 0;
    self->buffer = nullptr;
    self->bufferSize = 0;
    self->libContext = libContext;
}

static void DestroyCommonString(CommonString* self) {
    std::free(self->buffer);
    self->buffer = nullptr;
    self->bufferSize = 0;
}

static int AssignCommonString(CommonString* self, const char* text) {
    if (text == nullptr) return ParameterInvalidArgument;
    const std::size_t length = std::strlen(text);
    char* const oldBuffer = self->buffer;
    char* const buffer = static_cast<char*>(std::malloc(length + 1));
    if (buffer == nullptr) {
        self->buffer = oldBuffer;
        return ParameterOutOfMemory;
    }
    std::memcpy(buffer, text, length + 1);
    self->buffer = buffer;
    self->bufferSize = length + 1;
    std::free(oldBuffer);
    return 0;
}

static void* AllocateParameterImplementation(const std::size_t size) {
    return std::calloc(1, size);
}

static constexpr int ActivityInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ActivityOutOfMemory = static_cast<int>(0x80553504);
static constexpr int ActivityAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ActivityNotInitialized = static_cast<int>(0x80553510);

struct ActivityParameter {
    void* base;
    void* state;
};

struct ActivityParameterState {
    int limit;
    int npServiceLabel;
};

static_assert(sizeof(ActivityParameter) == 0x10 && offsetof(ActivityParameter, state) == 0x08);

static void DestroyActivityParameter(ActivityParameter* self) {
    std::free(self->state);
    self->state = nullptr;
    self->base = nullptr;
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesC1Ev(void* rawSelf) {
    auto* self = static_cast<ActivityParameter*>(rawSelf);
    self->base = nullptr;
    self->state = nullptr;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesC2Ev(void* rawSelf) {
    return _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesC1Ev(rawSelf);
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesD1Ev(void* rawSelf) {
    DestroyActivityParameter(static_cast<ActivityParameter*>(rawSelf));
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesD2Ev(void* rawSelf) {
    return _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivitiesD1Ev(rawSelf);
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities10initializeEPNS1_6Common10LibContextENS7_12IntrusivePtrINS7_6VectorINS7_6StringEEEEE(void* rawSelf, void* libContext, const IntrusivePtr* accountIds) {
    (void)accountIds;
    auto* self = static_cast<ActivityParameter*>(rawSelf);
    if (libContext == nullptr) return ActivityInvalidArgument;
    if (self->base != nullptr || self->state != nullptr) return ActivityAlreadyInitialized;
    auto* state = static_cast<ActivityParameterState*>(std::malloc(sizeof(ActivityParameterState)));
    if (state == nullptr) return ActivityOutOfMemory;
    state->limit = 0;
    state->npServiceLabel = 0;
    self->base = libContext;
    self->state = state;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities9terminateEv(void* rawSelf) {
    auto* self = static_cast<ActivityParameter*>(rawSelf);
    if (self->base == nullptr) return ActivityNotInitialized;
    DestroyActivityParameter(self);
    return 0;
}

void APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities8setlimitEi(void* rawSelf, int limit) {
    auto* self = static_cast<ActivityParameter*>(rawSelf);
    auto* state = static_cast<ActivityParameterState*>(self->state);
    if (state != nullptr) state->limit = limit;
}

void APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi28ParameterToGetUserActivities17setnpServiceLabelEi(void* rawSelf, int label) {
    auto* self = static_cast<ActivityParameter*>(rawSelf);
    auto* state = static_cast<ActivityParameterState*>(self->state);
    if (state != nullptr) state->npServiceLabel = label;
}

int APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities11ActivityApi17getUserActivitiesEiRKNS5_28ParameterToGetUserActivitiesERNS1_6Common11TransactionINS9_12IntrusivePtrINS4_26GetUsersActivitiesResponseEEENSB_INS9_18ResponseHeaderBaseEEEEE(int user, const void* parameter, void* transaction) {
    (void)user;
    (void)parameter;
    (void)transaction;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity13getActivityIdEv(const void* self) {
    return static_cast<const std::byte*>(self) + 0x10;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity7getNameEv(const void* self) {
    return static_cast<const std::byte*>(self) + 0x30;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity9getHiddenEv(const void* self) {
    return *reinterpret_cast<const bool*>(static_cast<const std::byte*>(self) + 0x50);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity12matchIdIsSetEv(const void* self) {
    return *reinterpret_cast<const bool*>(static_cast<const std::byte*>(self) + 0x78);
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities8Activity10getMatchIdEv(const void* self) {
    return static_cast<const std::byte*>(self) + 0x58;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi10Activities2V114UserActivities14UserActivities12getAccountIdEv(const void* self) {
    return static_cast<const std::uint8_t*>(self) + 0x10;
}

static constexpr std::size_t StringVectorReferenceCountOffset = 0x38;
static constexpr std::size_t ScalarVectorReferenceCountOffset = 0x20;
static constexpr int VectorOutOfMemory = static_cast<int>(0x80553504);

static_assert(sizeof(CommonIterator) == 0x10 && offsetof(CommonIterator, element) == 0x08);

static void* HostVectorBegin(void* out, const void* self, const std::int32_t elementKind, const char* function);
static void* HostVectorEnd(void* out, const void* self, const std::int32_t elementKind, const std::size_t elementSize, const char* function);
static void* HostIteratorAdvance(void* self, const std::size_t elementSize);
static bool HostIteratorNotEqual(const void* self, const void* other);
static void* CopyMemberIntrusivePtr(void* out, const void* self, const std::size_t memberOffset, const std::size_t referenceCountOffset);
static void* CopyOnlineId(void* out, const void* self, const std::size_t memberOffset);

void* APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities26GetUsersActivitiesResponse8getUsersEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi10Activities2V114UserActivities14UserActivities13getActivitiesEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody18setStartSerialRankERKi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setGroupERKNS3_5GroupE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setLimitERKi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setUsersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_4UserEEEEE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody9setOffsetERKi() {
 return 0;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody10getEntriesEv(void* out, void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x30, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody10setCommentEPKc() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody12setSmallDataEPKvm() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody15setNeedsTmpRankERKb() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody19setComparedDateTimeERK10SceRtcTick() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody7setPcIdERKi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V128GetRankingRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_21GetRankingRequestBodyEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V129RecordScoreRequestBodyFactory6createEPNS1_6Common10LibContextElPNS5_12IntrusivePtrINS3_22RecordScoreRequestBodyEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V14User12setAccountIdERKm() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V14User7setPcIdERKi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi10getRankingEiRKNS4_21ParameterToGetRankingERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22GetRankingResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking10initializeEPNS1_6Common10LibContextEi(void* self, const std::uint64_t* context, int boardId) {
    (void)boardId;
    return InitializeParameter(self, context, true);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi22getLargeDataByObjectIdEiRKNS4_33ParameterToGetLargeDataByObjectIdERNS1_6Common21DownStreamTransactionINS8_12IntrusivePtrINS4_37GetLargeDataByObjectIdResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId10initializeEPNS1_6Common10LibContextEPKc(void* self, const std::uint64_t* context, const char* objectId) {
    return InitializeParameter(self, context, objectId != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi18getBoardDefinitionEiRKNS4_29ParameterToGetBoardDefinitionERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_30GetBoardDefinitionResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition10initializeEPNS1_6Common10LibContextEi(void* self, const std::uint64_t* context, int boardId) {
    if (context == nullptr) return ParameterInvalidArgument;
    if (*context == 0) return ParameterNotInitialized;
    auto* parameter = static_cast<ParameterObject*>(self);
    if (parameter->implementation != nullptr) return ParameterAlreadyInitialized;
    void* implementation = AllocateParameterImplementation(BoardDefinitionImplementationSize);
    if (implementation == nullptr) return ParameterOutOfMemory;
    ConstructCommonString(CommonStringAt(implementation, 0x08), const_cast<std::uint64_t*>(context));
    parameter->implementation = implementation;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9terminateEv(void* self) {
    auto* parameter = static_cast<ParameterObject*>(self);
    if (parameter->implementation == nullptr) return ParameterTerminatedUninitialized;
    void* implementation = const_cast<void*>(parameter->implementation);
    DestroyCommonString(CommonStringAt(implementation, 0x08));
    std::free(implementation);
    parameter->implementation = nullptr;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi11recordScoreEiRKNS4_22ParameterToRecordScoreERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_23RecordScoreResponseBodyEEENSA_INS4_26RecordScoreResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi15recordLargeDataEiRKNS4_26ParameterToRecordLargeDataERNS1_6Common19UpStreamTransactionINS8_12IntrusivePtrINS3_27RecordLargeDataResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore10initializeEPNS1_6Common10LibContextEiNS6_12IntrusivePtrINS3_22RecordScoreRequestBodyEEE(void* self, const std::uint64_t* context, int boardId, const IntrusivePtr* body) {
    (void)boardId;
    return InitializeParameterWithBody(self, context, true, body);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore22setxPsnAtomicOperationENS5_19XPsnAtomicOperationE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData10initializeEPNS1_6Common10LibContextEiNS5_19XPsnAtomicOperationEPKc(void* self, const std::uint64_t* context, int boardId, int operation, const char* objectId) {
    (void)boardId;
    (void)operation;
    return InitializeParameter(self, context, objectId != nullptr);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData9terminateEv(void* self) {
    return TerminateParameter(self);
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataC1Ev(void* self) {
    ConstructParameter(self);
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi12downloadDataEiRKNS4_23ParameterToDownloadDataERNS1_6Common21DownStreamTransactionINS8_12IntrusivePtrINS4_27DownloadDataResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}



void* APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead19getPlayerAttributesEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x68, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V115LocationFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_8LocationEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V116AttributeFactory6createEPNS1_6Common10LibContextEPKcNS3_13AttributeTypeES9_PNS5_12IntrusivePtrINS3_9AttributeEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody10getPlayersEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x80, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody10getPlayersEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0xa0, VectorReferenceCountOffset);
}

void* APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody19getTicketAttributesEv(void* out, const void* self) {
    return CopyMemberIntrusivePtr(out, self, 0x80, VectorReferenceCountOffset);
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate10setNatTypeERKi(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate11setTeamNameEPKc(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V121PlayerForTicketCreate19setPlayerAttributesERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_9AttributeEEEEE(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V123SubmitTicketRequestBody11setLocationERKNS1_6Common12IntrusivePtrINS3_8LocationEEE(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V123SubmitTicketRequestBody19setTicketAttributesERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_9AttributeEEEEE(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V128PlayerForTicketCreateFactory6createEPNS1_6Common10LibContextERKmNS3_8PlatformEPNS5_12IntrusivePtrINS3_21PlayerForTicketCreateEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V130SubmitTicketRequestBodyFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_21PlayerForTicketCreateEEEEEPNSB_INS3_23SubmitTicketRequestBodyEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZN3sce2Np9CppWebApi11Matchmaking2V13Api12cancelTicketEiRKNS4_23ParameterToCancelTicketERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE(void* self) {
    (void)self;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}
