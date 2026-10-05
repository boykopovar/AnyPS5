#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_WEBAPI_ERROR_UNAVAILABLE = static_cast<int>(0x80552901);

// Offline PSN: request entry points (API calls, transaction start/readData, factories) fail with
// SCE_NP_WEBAPI_ERROR_UNAVAILABLE; parameter bookkeeping succeeds; response accessors stay unimplemented
// because a failed request never produces a response.
extern "C" {

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
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

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody10getEntriesEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
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
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V129RecordScoreRequestBodyFactory6createEPNS1_6Common10LibContextElPNS5_12IntrusivePtrINS3_22RecordScoreRequestBodyEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V14User12setAccountIdERKm() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V14User7setPcIdERKi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi10getRankingEiRKNS4_21ParameterToGetRankingERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_22GetRankingResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking10initializeEPNS1_6Common10LibContextEi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi22getLargeDataByObjectIdEiRKNS4_33ParameterToGetLargeDataByObjectIdERNS1_6Common21DownStreamTransactionINS8_12IntrusivePtrINS4_37GetLargeDataByObjectIdResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId10initializeEPNS1_6Common10LibContextEPKc() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectId9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi33ParameterToGetLargeDataByObjectIdD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi18getBoardDefinitionEiRKNS4_29ParameterToGetBoardDefinitionERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_30GetBoardDefinitionResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition10initializeEPNS1_6Common10LibContextEi() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinitionD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi11recordScoreEiRKNS4_22ParameterToRecordScoreERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_23RecordScoreResponseBodyEEENSA_INS4_26RecordScoreResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi15recordLargeDataEiRKNS4_26ParameterToRecordLargeDataERNS1_6Common19UpStreamTransactionINS8_12IntrusivePtrINS3_27RecordLargeDataResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore10initializeEPNS1_6Common10LibContextEiNS6_12IntrusivePtrINS3_22RecordScoreRequestBodyEEE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore22setxPsnAtomicOperationENS5_19XPsnAtomicOperationE() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScore9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi22ParameterToRecordScoreD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData10initializeEPNS1_6Common10LibContextEiNS5_19XPsnAtomicOperationEPKc() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeData9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi12Leaderboards2V19RecordApi26ParameterToRecordLargeDataD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi12downloadDataEiRKNS4_23ParameterToDownloadDataERNS1_6Common21DownStreamTransactionINS8_12IntrusivePtrINS4_27DownloadDataResponseHeadersEEEEE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10initializeEPNS1_6Common10LibContextEPKci() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9terminateEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common10InitParamsC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common10InitParamsD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common10LibContextC1Ev(uint64_t* self) {
    *self = 0;
    return 0;
}

// Initialization succeeds as on an offline console (see libSceJson2); individual requests fail.
int APS5_VABI _ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(const void* params, uint64_t* context) {
    (void)params;
    if (context) *context = 1;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

struct IntrusivePtr {
    void* object;
    void (APS5_VABI* deleter)(void*);
    void* libContext;
};
static_assert(sizeof(IntrusivePtr) == 0x18);

static constexpr std::size_t ReferenceCountOffset = 0;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;

static std::atomic_ref<std::int32_t> ReferenceCount(void* object, const std::size_t offset) {
    return std::atomic_ref<std::int32_t>(*reinterpret_cast<std::int32_t*>(static_cast<std::uint8_t*>(object) + offset));
}

static void CopyIntrusivePtr(IntrusivePtr* self, const IntrusivePtr* source, const std::size_t referenceCountOffset) {
    *self = *source;
    if (self->object) ReferenceCount(self->object, referenceCountOffset).fetch_add(1);
}

static void ReleaseIntrusivePtr(IntrusivePtr* self, const std::size_t referenceCountOffset, const char* function) {
    if (!self->object) return;
    if (ReferenceCount(self->object, referenceCountOffset).fetch_sub(1) != 1) return;
    if (!self->deleter) NotImplemented_nid_no_patch(function);
    self->deleter(self->object);
    self->object = nullptr;
    self->deleter = nullptr;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_(IntrusivePtr* self, const IntrusivePtr* source) {
    CopyIntrusivePtr(self, source, ReferenceCountOffset);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEC1Ev(IntrusivePtr* self) {
    *self = {};
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, ReferenceCountOffset, __func__);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEED1Ev(IntrusivePtr* self) {
    ReleaseIntrusivePtr(self, VectorReferenceCountOffset, __func__);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEED2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V127RecordLargeDataResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V127RecordLargeDataResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextEm() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V127RecordLargeDataResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE8sendDataEPKvm() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V127RecordLargeDataResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common19UpStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V127RecordLargeDataResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V17ViewApi37GetLargeDataByObjectIdResponseHeadersEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V17ViewApi37GetLargeDataByObjectIdResponseHeadersEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V17ViewApi37GetLargeDataByObjectIdResponseHeadersEEEE8readDataEPcm() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V17ViewApi37GetLargeDataByObjectIdResponseHeadersEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_12Leaderboards2V17ViewApi37GetLargeDataByObjectIdResponseHeadersEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE5startEPNS2_10LibContextE() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE6finishEv() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEE8readDataEPcm() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common21DownStreamTransactionINS2_12IntrusivePtrINS1_17TitleCloudStorage2V17DataApi27DownloadDataResponseHeadersEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

struct CommonString {
    std::int32_t referenceCount;
    char* buffer;
    std::uint64_t bufferSize;
    void* libContext;
};
static_assert(sizeof(CommonString) == 0x20 && offsetof(CommonString, buffer) == 0x08);
static_assert(offsetof(CommonString, bufferSize) == 0x10 && offsetof(CommonString, libContext) == 0x18);

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(CommonString* self, void* libContext) {
    self->referenceCount = 0;
    self->buffer = nullptr;
    self->bufferSize = 0;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringD1Ev(CommonString* self) {
    if (self->buffer != nullptr) NotImplemented_nid_no_patch("sce::Np::CppWebApi::Common::String buffer release");
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEE8pushBackERKS8_() {
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEEC1EPNS2_10LibContextE(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE3endEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE5beginEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEEppEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(uint64_t* context) {
    *context = 0;
    return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody22getLastUpdatedDateTimeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V123RecordScoreResponseBody10getTmpRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V123RecordScoreResponseBody16getTmpSerialRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody11getSortModeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody13getEntryLimitEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody13getUpdateModeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody13sortModeIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody15entryLimitIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody15updateModeIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMaxScoreLimitEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMinScoreLimitEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18maxScoreLimitIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18minScoreLimitIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody20getLargeDataNumLimitEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody21getLargeDataSizeLimitEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody22largeDataNumLimitIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody23largeDataSizeLimitIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry10getCommentEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry11getObjectIdEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry11getOnlineIdEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12commentIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12getAccountIdEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12getSmallDataEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13getSerialRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13objectIdIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14getHighestRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14smallDataIsSetEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry20getHighestSerialRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry7getPcIdEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry7getRankEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry8getScoreEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V19RecordApi26RecordScoreResponseHeaders24getXPsnAtomicOperationIdEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V15EntryEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE18getResponseHeadersERSB_() {
 return SCE_NP_WEBAPI_ERROR_UNAVAILABLE;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common6Binary4sizeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String5c_strEv(const CommonString* self) {
    return self->buffer != nullptr ? self->buffer : "";
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEEdeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEEneERKS9_() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


APS5_EXPORT("+gSMGXHgrPU", sceNpCppWebApiUnknown00);
int APS5_VABI sceNpCppWebApiUnknown00(void) {
    NotImplemented_nid_no_patch("+gSMGXHgrPU");
    return 0;
}

APS5_EXPORT("-XNfwZTFkWw", sceNpCppWebApiUnknown01);
int APS5_VABI sceNpCppWebApiUnknown01(void) {
    NotImplemented_nid_no_patch("-XNfwZTFkWw");
    return 0;
}

APS5_EXPORT("0f5+eBcgB14", sceNpCppWebApiUnknown02);
int APS5_VABI sceNpCppWebApiUnknown02(void) {
    NotImplemented_nid_no_patch("0f5+eBcgB14");
    return 0;
}

APS5_EXPORT("1iq5Jtw4jVs", sceNpCppWebApiUnknown03);
int APS5_VABI sceNpCppWebApiUnknown03(void) {
    NotImplemented_nid_no_patch("1iq5Jtw4jVs");
    return 0;
}

APS5_EXPORT("1wpCXuFzH10", sceNpCppWebApiUnknown04);
int APS5_VABI sceNpCppWebApiUnknown04(void) {
    NotImplemented_nid_no_patch("1wpCXuFzH10");
    return 0;
}

APS5_EXPORT("4AFThLW28xQ", sceNpCppWebApiUnknown05);
int APS5_VABI sceNpCppWebApiUnknown05(void) {
    NotImplemented_nid_no_patch("4AFThLW28xQ");
    return 0;
}

APS5_EXPORT("8-9Y4oS+OuE", sceNpCppWebApiUnknown06);
int APS5_VABI sceNpCppWebApiUnknown06(void) {
    NotImplemented_nid_no_patch("8-9Y4oS+OuE");
    return 0;
}

APS5_EXPORT("8dWkWjo5EeE", sceNpCppWebApiUnknown07);
int APS5_VABI sceNpCppWebApiUnknown07(void) {
    NotImplemented_nid_no_patch("8dWkWjo5EeE");
    return 0;
}

APS5_EXPORT("BHvfIGdarBQ", sceNpCppWebApiUnknown08);
int APS5_VABI sceNpCppWebApiUnknown08(void) {
    NotImplemented_nid_no_patch("BHvfIGdarBQ");
    return 0;
}

APS5_EXPORT("Fxxr5lYBfl4", sceNpCppWebApiUnknown09);
int APS5_VABI sceNpCppWebApiUnknown09(void) {
    NotImplemented_nid_no_patch("Fxxr5lYBfl4");
    return 0;
}

APS5_EXPORT("M1LnjRscx-w", sceNpCppWebApiUnknown10);
int APS5_VABI sceNpCppWebApiUnknown10(void) {
    NotImplemented_nid_no_patch("M1LnjRscx-w");
    return 0;
}

APS5_EXPORT("PsX657reZeo", sceNpCppWebApiUnknown11);
int APS5_VABI sceNpCppWebApiUnknown11(void) {
    NotImplemented_nid_no_patch("PsX657reZeo");
    return 0;
}

APS5_EXPORT("RvH9AZ5rCkQ", sceNpCppWebApiUnknown12);
int APS5_VABI sceNpCppWebApiUnknown12(void) {
    NotImplemented_nid_no_patch("RvH9AZ5rCkQ");
    return 0;
}

APS5_EXPORT("SR3BKonD8yk", sceNpCppWebApiUnknown13);
int APS5_VABI sceNpCppWebApiUnknown13(void) {
    NotImplemented_nid_no_patch("SR3BKonD8yk");
    return 0;
}

APS5_EXPORT("TSoQhYKyq5g", sceNpCppWebApiUnknown14);
int APS5_VABI sceNpCppWebApiUnknown14(void) {
    NotImplemented_nid_no_patch("TSoQhYKyq5g");
    return 0;
}

APS5_EXPORT("TgsJjQqp+2k", sceNpCppWebApiUnknown15);
int APS5_VABI sceNpCppWebApiUnknown15(void) {
    NotImplemented_nid_no_patch("TgsJjQqp+2k");
    return 0;
}

APS5_EXPORT("UHQxE3HhTXA", sceNpCppWebApiUnknown16);
int APS5_VABI sceNpCppWebApiUnknown16(void) {
    NotImplemented_nid_no_patch("UHQxE3HhTXA");
    return 0;
}

APS5_EXPORT("ZN-glpk0Rug", sceNpCppWebApiUnknown17);
int APS5_VABI sceNpCppWebApiUnknown17(void) {
    NotImplemented_nid_no_patch("ZN-glpk0Rug");
    return 0;
}

APS5_EXPORT("ZQzXwokCnUE", sceNpCppWebApiUnknown18);
int APS5_VABI sceNpCppWebApiUnknown18(void) {
    NotImplemented_nid_no_patch("ZQzXwokCnUE");
    return 0;
}

APS5_EXPORT("ZwjmbSr4Cxc", sceNpCppWebApiUnknown19);
int APS5_VABI sceNpCppWebApiUnknown19(void) {
    NotImplemented_nid_no_patch("ZwjmbSr4Cxc");
    return 0;
}

APS5_EXPORT("b7leY-LNVnI", sceNpCppWebApiUnknown20);
int APS5_VABI sceNpCppWebApiUnknown20(void) {
    NotImplemented_nid_no_patch("b7leY-LNVnI");
    return 0;
}

APS5_EXPORT("c3J9K9XbQqE", sceNpCppWebApiUnknown21);
int APS5_VABI sceNpCppWebApiUnknown21(void) {
    NotImplemented_nid_no_patch("c3J9K9XbQqE");
    return 0;
}

APS5_EXPORT("cSLbQUQO2Ns", sceNpCppWebApiUnknown22);
int APS5_VABI sceNpCppWebApiUnknown22(void) {
    NotImplemented_nid_no_patch("cSLbQUQO2Ns");
    return 0;
}

APS5_EXPORT("cqOxjL0ZfFA", sceNpCppWebApiUnknown23);
int APS5_VABI sceNpCppWebApiUnknown23(void) {
    NotImplemented_nid_no_patch("cqOxjL0ZfFA");
    return 0;
}

APS5_EXPORT("d1eEjWR60wk", sceNpCppWebApiUnknown24);
int APS5_VABI sceNpCppWebApiUnknown24(void) {
    NotImplemented_nid_no_patch("d1eEjWR60wk");
    return 0;
}

APS5_EXPORT("epwr+cBCIFs", sceNpCppWebApiUnknown25);
int APS5_VABI sceNpCppWebApiUnknown25(void) {
    NotImplemented_nid_no_patch("epwr+cBCIFs");
    return 0;
}

APS5_EXPORT("eruduJ0KrT0", sceNpCppWebApiUnknown26);
int APS5_VABI sceNpCppWebApiUnknown26(void) {
    NotImplemented_nid_no_patch("eruduJ0KrT0");
    return 0;
}

APS5_EXPORT("fG06a38iao8", sceNpCppWebApiUnknown27);
int APS5_VABI sceNpCppWebApiUnknown27(void) {
    NotImplemented_nid_no_patch("fG06a38iao8");
    return 0;
}

APS5_EXPORT("jfpSx14AeLc", sceNpCppWebApiUnknown28);
int APS5_VABI sceNpCppWebApiUnknown28(void) {
    NotImplemented_nid_no_patch("jfpSx14AeLc");
    return 0;
}

APS5_EXPORT("kBxwE4YfbIM", sceNpCppWebApiUnknown29);
int APS5_VABI sceNpCppWebApiUnknown29(void) {
    NotImplemented_nid_no_patch("kBxwE4YfbIM");
    return 0;
}

APS5_EXPORT("kz2Z38yTLq0", sceNpCppWebApiUnknown30);
int APS5_VABI sceNpCppWebApiUnknown30(void) {
    NotImplemented_nid_no_patch("kz2Z38yTLq0");
    return 0;
}

APS5_EXPORT("lB8driFKaoU", sceNpCppWebApiUnknown31);
int APS5_VABI sceNpCppWebApiUnknown31(void) {
    NotImplemented_nid_no_patch("lB8driFKaoU");
    return 0;
}

APS5_EXPORT("mCsV7izOkjY", sceNpCppWebApiUnknown32);
int APS5_VABI sceNpCppWebApiUnknown32(void) {
    NotImplemented_nid_no_patch("mCsV7izOkjY");
    return 0;
}

APS5_EXPORT("n1yCLMxpseQ", sceNpCppWebApiUnknown33);
int APS5_VABI sceNpCppWebApiUnknown33(void) {
    NotImplemented_nid_no_patch("n1yCLMxpseQ");
    return 0;
}

APS5_EXPORT("nL9vSVq-29s", sceNpCppWebApiUnknown34);
int APS5_VABI sceNpCppWebApiUnknown34(void) {
    NotImplemented_nid_no_patch("nL9vSVq-29s");
    return 0;
}

APS5_EXPORT("nmz5JYKcMfY", sceNpCppWebApiUnknown35);
int APS5_VABI sceNpCppWebApiUnknown35(void) {
    NotImplemented_nid_no_patch("nmz5JYKcMfY");
    return 0;
}

APS5_EXPORT("oHRl7a+zdMU", sceNpCppWebApiUnknown36);
int APS5_VABI sceNpCppWebApiUnknown36(void) {
    NotImplemented_nid_no_patch("oHRl7a+zdMU");
    return 0;
}

APS5_EXPORT("pSpO3hPNv64", sceNpCppWebApiUnknown37);
int APS5_VABI sceNpCppWebApiUnknown37(void) {
    NotImplemented_nid_no_patch("pSpO3hPNv64");
    return 0;
}

APS5_EXPORT("q4UFICay6Hk", sceNpCppWebApiUnknown38);
int APS5_VABI sceNpCppWebApiUnknown38(void) {
    NotImplemented_nid_no_patch("q4UFICay6Hk");
    return 0;
}

APS5_EXPORT("qiD2PU2Jstc", sceNpCppWebApiUnknown39);
int APS5_VABI sceNpCppWebApiUnknown39(void) {
    NotImplemented_nid_no_patch("qiD2PU2Jstc");
    return 0;
}

APS5_EXPORT("tFe964qzGEM", sceNpCppWebApiUnknown40);
int APS5_VABI sceNpCppWebApiUnknown40(void) {
    NotImplemented_nid_no_patch("tFe964qzGEM");
    return 0;
}

APS5_EXPORT("tHaO36kincQ", sceNpCppWebApiUnknown41);
int APS5_VABI sceNpCppWebApiUnknown41(void) {
    NotImplemented_nid_no_patch("tHaO36kincQ");
    return 0;
}

APS5_EXPORT("umm5m+mXiZs", sceNpCppWebApiUnknown42);
int APS5_VABI sceNpCppWebApiUnknown42(void) {
    NotImplemented_nid_no_patch("umm5m+mXiZs");
    return 0;
}

APS5_EXPORT("uwhztB49KOQ", sceNpCppWebApiUnknown43);
int APS5_VABI sceNpCppWebApiUnknown43(void) {
    NotImplemented_nid_no_patch("uwhztB49KOQ");
    return 0;
}

APS5_EXPORT("w4K4nTYwhVE", sceNpCppWebApiUnknown44);
int APS5_VABI sceNpCppWebApiUnknown44(void) {
    NotImplemented_nid_no_patch("w4K4nTYwhVE");
    return 0;
}

APS5_EXPORT("wFcm6bgWB9Q", sceNpCppWebApiUnknown45);
int APS5_VABI sceNpCppWebApiUnknown45(void) {
    NotImplemented_nid_no_patch("wFcm6bgWB9Q");
    return 0;
}

APS5_EXPORT("wp8+c84G5Xw", sceNpCppWebApiUnknown46);
int APS5_VABI sceNpCppWebApiUnknown46(void) {
    NotImplemented_nid_no_patch("wp8+c84G5Xw");
    return 0;
}

APS5_EXPORT("zjqTmP0ST9A", sceNpCppWebApiUnknown47);
int APS5_VABI sceNpCppWebApiUnknown47(void) {
    NotImplemented_nid_no_patch("zjqTmP0ST9A");
    return 0;
}

APS5_EXPORT("zzO8ZGJ74ng", sceNpCppWebApiUnknown48);
int APS5_VABI sceNpCppWebApiUnknown48(void) {
    NotImplemented_nid_no_patch("zzO8ZGJ74ng");
    return 0;
}

APS5_EXPORT("12wY179+CE8", sceNpCppWebApiUnknown49);
int APS5_VABI sceNpCppWebApiUnknown49(void) {
    NotImplemented_nid_no_patch("12wY179+CE8");
    return 0;
}

APS5_EXPORT("3bjEFf8OhTs", sceNpCppWebApiUnknown50);
int APS5_VABI sceNpCppWebApiUnknown50(void) {
    NotImplemented_nid_no_patch("3bjEFf8OhTs");
    return 0;
}

APS5_EXPORT("6FK4IOnANkc", sceNpCppWebApiUnknown51);
int APS5_VABI sceNpCppWebApiUnknown51(void) {
    NotImplemented_nid_no_patch("6FK4IOnANkc");
    return 0;
}

APS5_EXPORT("EJAOHYWUki4", sceNpCppWebApiUnknown52);
int APS5_VABI sceNpCppWebApiUnknown52(void) {
    NotImplemented_nid_no_patch("EJAOHYWUki4");
    return 0;
}

APS5_EXPORT("FlagLhjAEmc", sceNpCppWebApiUnknown53);
int APS5_VABI sceNpCppWebApiUnknown53(void) {
    NotImplemented_nid_no_patch("FlagLhjAEmc");
    return 0;
}

APS5_EXPORT("G3h-NDHnyW4", sceNpCppWebApiUnknown54);
int APS5_VABI sceNpCppWebApiUnknown54(void) {
    NotImplemented_nid_no_patch("G3h-NDHnyW4");
    return 0;
}

APS5_EXPORT("H0XjNhflED4", sceNpCppWebApiUnknown55);
int APS5_VABI sceNpCppWebApiUnknown55(void) {
    NotImplemented_nid_no_patch("H0XjNhflED4");
    return 0;
}

APS5_EXPORT("IbWx007Acn4", sceNpCppWebApiUnknown56);
int APS5_VABI sceNpCppWebApiUnknown56(void) {
    NotImplemented_nid_no_patch("IbWx007Acn4");
    return 0;
}

APS5_EXPORT("Is+nI7Hq9jU", sceNpCppWebApiUnknown57);
int APS5_VABI sceNpCppWebApiUnknown57(void) {
    NotImplemented_nid_no_patch("Is+nI7Hq9jU");
    return 0;
}

APS5_EXPORT("Kc4x2uy0FFk", sceNpCppWebApiUnknown58);
int APS5_VABI sceNpCppWebApiUnknown58(void) {
    NotImplemented_nid_no_patch("Kc4x2uy0FFk");
    return 0;
}

APS5_EXPORT("OK+Ggpve7B0", sceNpCppWebApiUnknown59);
int APS5_VABI sceNpCppWebApiUnknown59(void) {
    NotImplemented_nid_no_patch("OK+Ggpve7B0");
    return 0;
}

APS5_EXPORT("P3daBLFFREw", sceNpCppWebApiUnknown60);
int APS5_VABI sceNpCppWebApiUnknown60(void) {
    NotImplemented_nid_no_patch("P3daBLFFREw");
    return 0;
}

APS5_EXPORT("RZcseJ3THfY", sceNpCppWebApiUnknown61);
int APS5_VABI sceNpCppWebApiUnknown61(void) {
    NotImplemented_nid_no_patch("RZcseJ3THfY");
    return 0;
}

APS5_EXPORT("RbvteD35X-8", sceNpCppWebApiUnknown62);
int APS5_VABI sceNpCppWebApiUnknown62(void) {
    NotImplemented_nid_no_patch("RbvteD35X-8");
    return 0;
}

APS5_EXPORT("XCtf+23QT0k", sceNpCppWebApiUnknown63);
int APS5_VABI sceNpCppWebApiUnknown63(void) {
    NotImplemented_nid_no_patch("XCtf+23QT0k");
    return 0;
}

APS5_EXPORT("a4q15LI1a4E", sceNpCppWebApiUnknown64);
int APS5_VABI sceNpCppWebApiUnknown64(void) {
    NotImplemented_nid_no_patch("a4q15LI1a4E");
    return 0;
}

APS5_EXPORT("eT4TQB7OsLA", sceNpCppWebApiUnknown65);
int APS5_VABI sceNpCppWebApiUnknown65(void) {
    NotImplemented_nid_no_patch("eT4TQB7OsLA");
    return 0;
}

APS5_EXPORT("g2dEqFnhFuw", sceNpCppWebApiUnknown66);
int APS5_VABI sceNpCppWebApiUnknown66(void) {
    NotImplemented_nid_no_patch("g2dEqFnhFuw");
    return 0;
}

APS5_EXPORT("lPAMjFZEpt8", sceNpCppWebApiUnknown67);
int APS5_VABI sceNpCppWebApiUnknown67(void) {
    NotImplemented_nid_no_patch("lPAMjFZEpt8");
    return 0;
}

APS5_EXPORT("mBagn+lW-iM", sceNpCppWebApiUnknown68);
int APS5_VABI sceNpCppWebApiUnknown68(void) {
    NotImplemented_nid_no_patch("mBagn+lW-iM");
    return 0;
}

APS5_EXPORT("mkWsKEh0h0o", sceNpCppWebApiUnknown69);
int APS5_VABI sceNpCppWebApiUnknown69(void) {
    NotImplemented_nid_no_patch("mkWsKEh0h0o");
    return 0;
}

APS5_EXPORT("nE0ooeCMRm8", sceNpCppWebApiUnknown70);
int APS5_VABI sceNpCppWebApiUnknown70(void) {
    NotImplemented_nid_no_patch("nE0ooeCMRm8");
    return 0;
}

APS5_EXPORT("qf2I9BlKXis", sceNpCppWebApiUnknown71);
int APS5_VABI sceNpCppWebApiUnknown71(void) {
    NotImplemented_nid_no_patch("qf2I9BlKXis");
    return 0;
}

APS5_EXPORT("xwy1I52dE8o", sceNpCppWebApiUnknown72);
int APS5_VABI sceNpCppWebApiUnknown72(void) {
    NotImplemented_nid_no_patch("xwy1I52dE8o");
    return 0;
}
}
