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

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1ERS7_(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1ERS7_(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEED1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEED1Ev(void* self) {
    (void)self;
    return 0;
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

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringC1EPNS2_10LibContextE(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6StringD1Ev(void* self) {
    (void)self;
    return 0;
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

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V15EntryEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEptEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEdeEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
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

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String5c_strEv() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
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

APS5_EXPORT("OgbbbsOtwlE", sceNpCppWebApiUnknown73);
int APS5_VABI sceNpCppWebApiUnknown73(void) {
    NotImplemented_nid_no_patch("OgbbbsOtwlE");
    return 0;
}

APS5_EXPORT("lSe4Oe+76o4", sceNpCppWebApiUnknown74);
int APS5_VABI sceNpCppWebApiUnknown74(void) {
    NotImplemented_nid_no_patch("lSe4Oe+76o4");
    return 0;
}

APS5_EXPORT("qwa0biONtPI", sceNpCppWebApiUnknown75);
int APS5_VABI sceNpCppWebApiUnknown75(void) {
    NotImplemented_nid_no_patch("qwa0biONtPI");
    return 0;
}

APS5_EXPORT("ygL-RUMNAn0", sceNpCppWebApiUnknown76);
int APS5_VABI sceNpCppWebApiUnknown76(void) {
    NotImplemented_nid_no_patch("ygL-RUMNAn0");
    return 0;
}

APS5_EXPORT("+0jo0J7h4CM", sceNpCppWebApiUnknown77);
int APS5_VABI sceNpCppWebApiUnknown77(void) {
    NotImplemented_nid_no_patch("+0jo0J7h4CM");
    return 0;
}

APS5_EXPORT("+HWmt-AJocg", sceNpCppWebApiUnknown78);
int APS5_VABI sceNpCppWebApiUnknown78(void) {
    NotImplemented_nid_no_patch("+HWmt-AJocg");
    return 0;
}

APS5_EXPORT("+Ts4tkJjqS8", sceNpCppWebApiUnknown79);
int APS5_VABI sceNpCppWebApiUnknown79(void) {
    NotImplemented_nid_no_patch("+Ts4tkJjqS8");
    return 0;
}

APS5_EXPORT("+rWjmNf5G5E", sceNpCppWebApiUnknown80);
int APS5_VABI sceNpCppWebApiUnknown80(void) {
    NotImplemented_nid_no_patch("+rWjmNf5G5E");
    return 0;
}

APS5_EXPORT("-D8MD+5yuAw", sceNpCppWebApiUnknown81);
int APS5_VABI sceNpCppWebApiUnknown81(void) {
    NotImplemented_nid_no_patch("-D8MD+5yuAw");
    return 0;
}

APS5_EXPORT("-NWoybNRyYQ", sceNpCppWebApiUnknown82);
int APS5_VABI sceNpCppWebApiUnknown82(void) {
    NotImplemented_nid_no_patch("-NWoybNRyYQ");
    return 0;
}

APS5_EXPORT("0YED2+p7aiI", sceNpCppWebApiUnknown83);
int APS5_VABI sceNpCppWebApiUnknown83(void) {
    NotImplemented_nid_no_patch("0YED2+p7aiI");
    return 0;
}

APS5_EXPORT("0juFlJ9R6ss", sceNpCppWebApiUnknown84);
int APS5_VABI sceNpCppWebApiUnknown84(void) {
    NotImplemented_nid_no_patch("0juFlJ9R6ss");
    return 0;
}

APS5_EXPORT("13KoEaVmuJ8", sceNpCppWebApiUnknown85);
int APS5_VABI sceNpCppWebApiUnknown85(void) {
    NotImplemented_nid_no_patch("13KoEaVmuJ8");
    return 0;
}

APS5_EXPORT("1FITx-pXXA0", sceNpCppWebApiUnknown86);
int APS5_VABI sceNpCppWebApiUnknown86(void) {
    NotImplemented_nid_no_patch("1FITx-pXXA0");
    return 0;
}

APS5_EXPORT("1bf+c7ebx0A", sceNpCppWebApiUnknown87);
int APS5_VABI sceNpCppWebApiUnknown87(void) {
    NotImplemented_nid_no_patch("1bf+c7ebx0A");
    return 0;
}

APS5_EXPORT("1fyEy3F+G+g", sceNpCppWebApiUnknown88);
int APS5_VABI sceNpCppWebApiUnknown88(void) {
    NotImplemented_nid_no_patch("1fyEy3F+G+g");
    return 0;
}

APS5_EXPORT("1rlHMruxwHk", sceNpCppWebApiUnknown89);
int APS5_VABI sceNpCppWebApiUnknown89(void) {
    NotImplemented_nid_no_patch("1rlHMruxwHk");
    return 0;
}

APS5_EXPORT("2mQGp1B16Uc", sceNpCppWebApiUnknown90);
int APS5_VABI sceNpCppWebApiUnknown90(void) {
    NotImplemented_nid_no_patch("2mQGp1B16Uc");
    return 0;
}

APS5_EXPORT("3sNsQzNdylI", sceNpCppWebApiUnknown91);
int APS5_VABI sceNpCppWebApiUnknown91(void) {
    NotImplemented_nid_no_patch("3sNsQzNdylI");
    return 0;
}

APS5_EXPORT("4W6b6so-L-E", sceNpCppWebApiUnknown92);
int APS5_VABI sceNpCppWebApiUnknown92(void) {
    NotImplemented_nid_no_patch("4W6b6so-L-E");
    return 0;
}

APS5_EXPORT("58HnTZbC0+k", sceNpCppWebApiUnknown93);
int APS5_VABI sceNpCppWebApiUnknown93(void) {
    NotImplemented_nid_no_patch("58HnTZbC0+k");
    return 0;
}

APS5_EXPORT("5UV12de1fGo", sceNpCppWebApiUnknown94);
int APS5_VABI sceNpCppWebApiUnknown94(void) {
    NotImplemented_nid_no_patch("5UV12de1fGo");
    return 0;
}

APS5_EXPORT("5f9xDKxYjbY", sceNpCppWebApiUnknown95);
int APS5_VABI sceNpCppWebApiUnknown95(void) {
    NotImplemented_nid_no_patch("5f9xDKxYjbY");
    return 0;
}

APS5_EXPORT("6QL0WMBsvbs", sceNpCppWebApiUnknown96);
int APS5_VABI sceNpCppWebApiUnknown96(void) {
    NotImplemented_nid_no_patch("6QL0WMBsvbs");
    return 0;
}

APS5_EXPORT("73XG6dPCOr0", sceNpCppWebApiUnknown97);
int APS5_VABI sceNpCppWebApiUnknown97(void) {
    NotImplemented_nid_no_patch("73XG6dPCOr0");
    return 0;
}

APS5_EXPORT("75Hbvij1fk8", sceNpCppWebApiUnknown98);
int APS5_VABI sceNpCppWebApiUnknown98(void) {
    NotImplemented_nid_no_patch("75Hbvij1fk8");
    return 0;
}

APS5_EXPORT("76U8Y1XJ5CE", sceNpCppWebApiUnknown99);
int APS5_VABI sceNpCppWebApiUnknown99(void) {
    NotImplemented_nid_no_patch("76U8Y1XJ5CE");
    return 0;
}

APS5_EXPORT("9OV-NYh2TAs", sceNpCppWebApiUnknown100);
int APS5_VABI sceNpCppWebApiUnknown100(void) {
    NotImplemented_nid_no_patch("9OV-NYh2TAs");
    return 0;
}

APS5_EXPORT("9VDL1bE1nFM", sceNpCppWebApiUnknown101);
int APS5_VABI sceNpCppWebApiUnknown101(void) {
    NotImplemented_nid_no_patch("9VDL1bE1nFM");
    return 0;
}

APS5_EXPORT("BBoN-+1fjTA", sceNpCppWebApiUnknown102);
int APS5_VABI sceNpCppWebApiUnknown102(void) {
    NotImplemented_nid_no_patch("BBoN-+1fjTA");
    return 0;
}

APS5_EXPORT("D4Lk1XQiiPQ", sceNpCppWebApiUnknown103);
int APS5_VABI sceNpCppWebApiUnknown103(void) {
    NotImplemented_nid_no_patch("D4Lk1XQiiPQ");
    return 0;
}

APS5_EXPORT("DXlC3IrK2Ts", sceNpCppWebApiUnknown104);
int APS5_VABI sceNpCppWebApiUnknown104(void) {
    NotImplemented_nid_no_patch("DXlC3IrK2Ts");
    return 0;
}

APS5_EXPORT("Dza-PctrwiU", sceNpCppWebApiUnknown105);
int APS5_VABI sceNpCppWebApiUnknown105(void) {
    NotImplemented_nid_no_patch("Dza-PctrwiU");
    return 0;
}

APS5_EXPORT("F5Up4uEenjU", sceNpCppWebApiUnknown106);
int APS5_VABI sceNpCppWebApiUnknown106(void) {
    NotImplemented_nid_no_patch("F5Up4uEenjU");
    return 0;
}

APS5_EXPORT("F6g64Yst1FQ", sceNpCppWebApiUnknown107);
int APS5_VABI sceNpCppWebApiUnknown107(void) {
    NotImplemented_nid_no_patch("F6g64Yst1FQ");
    return 0;
}

APS5_EXPORT("FZXAWifD-Zk", sceNpCppWebApiUnknown108);
int APS5_VABI sceNpCppWebApiUnknown108(void) {
    NotImplemented_nid_no_patch("FZXAWifD-Zk");
    return 0;
}

APS5_EXPORT("Fts2nXFk6F0", sceNpCppWebApiUnknown109);
int APS5_VABI sceNpCppWebApiUnknown109(void) {
    NotImplemented_nid_no_patch("Fts2nXFk6F0");
    return 0;
}

APS5_EXPORT("FwKWIx0rgyY", sceNpCppWebApiUnknown110);
int APS5_VABI sceNpCppWebApiUnknown110(void) {
    NotImplemented_nid_no_patch("FwKWIx0rgyY");
    return 0;
}

APS5_EXPORT("FywKncPHxLc", sceNpCppWebApiUnknown111);
int APS5_VABI sceNpCppWebApiUnknown111(void) {
    NotImplemented_nid_no_patch("FywKncPHxLc");
    return 0;
}

APS5_EXPORT("G-rWYY8TfUY", sceNpCppWebApiUnknown112);
int APS5_VABI sceNpCppWebApiUnknown112(void) {
    NotImplemented_nid_no_patch("G-rWYY8TfUY");
    return 0;
}

APS5_EXPORT("G47L4jUy648", sceNpCppWebApiUnknown113);
int APS5_VABI sceNpCppWebApiUnknown113(void) {
    NotImplemented_nid_no_patch("G47L4jUy648");
    return 0;
}

APS5_EXPORT("GBILv-xY3eU", sceNpCppWebApiUnknown114);
int APS5_VABI sceNpCppWebApiUnknown114(void) {
    NotImplemented_nid_no_patch("GBILv-xY3eU");
    return 0;
}

APS5_EXPORT("GMSN1x4y+cs", sceNpCppWebApiUnknown115);
int APS5_VABI sceNpCppWebApiUnknown115(void) {
    NotImplemented_nid_no_patch("GMSN1x4y+cs");
    return 0;
}

APS5_EXPORT("GN92gdelkN0", sceNpCppWebApiUnknown116);
int APS5_VABI sceNpCppWebApiUnknown116(void) {
    NotImplemented_nid_no_patch("GN92gdelkN0");
    return 0;
}

APS5_EXPORT("GOKrmbjnHY0", sceNpCppWebApiUnknown117);
int APS5_VABI sceNpCppWebApiUnknown117(void) {
    NotImplemented_nid_no_patch("GOKrmbjnHY0");
    return 0;
}

APS5_EXPORT("HdhDexEBHlQ", sceNpCppWebApiUnknown118);
int APS5_VABI sceNpCppWebApiUnknown118(void) {
    NotImplemented_nid_no_patch("HdhDexEBHlQ");
    return 0;
}

APS5_EXPORT("JzqS+6C+2F4", sceNpCppWebApiUnknown119);
int APS5_VABI sceNpCppWebApiUnknown119(void) {
    NotImplemented_nid_no_patch("JzqS+6C+2F4");
    return 0;
}

APS5_EXPORT("KWwIUfLC-7k", sceNpCppWebApiUnknown120);
int APS5_VABI sceNpCppWebApiUnknown120(void) {
    NotImplemented_nid_no_patch("KWwIUfLC-7k");
    return 0;
}

APS5_EXPORT("LFNbGrxObcY", sceNpCppWebApiUnknown121);
int APS5_VABI sceNpCppWebApiUnknown121(void) {
    NotImplemented_nid_no_patch("LFNbGrxObcY");
    return 0;
}

APS5_EXPORT("N2xzUyENlCU", sceNpCppWebApiUnknown122);
int APS5_VABI sceNpCppWebApiUnknown122(void) {
    NotImplemented_nid_no_patch("N2xzUyENlCU");
    return 0;
}

APS5_EXPORT("NG-lbm2BAKU", sceNpCppWebApiUnknown123);
int APS5_VABI sceNpCppWebApiUnknown123(void) {
    NotImplemented_nid_no_patch("NG-lbm2BAKU");
    return 0;
}

APS5_EXPORT("NRX8fDa7344", sceNpCppWebApiUnknown124);
int APS5_VABI sceNpCppWebApiUnknown124(void) {
    NotImplemented_nid_no_patch("NRX8fDa7344");
    return 0;
}

APS5_EXPORT("OLgCzPhsacA", sceNpCppWebApiUnknown125);
int APS5_VABI sceNpCppWebApiUnknown125(void) {
    NotImplemented_nid_no_patch("OLgCzPhsacA");
    return 0;
}

APS5_EXPORT("QWRYQMQSpIc", sceNpCppWebApiUnknown126);
int APS5_VABI sceNpCppWebApiUnknown126(void) {
    NotImplemented_nid_no_patch("QWRYQMQSpIc");
    return 0;
}

APS5_EXPORT("Qltt2qSBU1I", sceNpCppWebApiUnknown127);
int APS5_VABI sceNpCppWebApiUnknown127(void) {
    NotImplemented_nid_no_patch("Qltt2qSBU1I");
    return 0;
}

APS5_EXPORT("RQfqM1BZuoc", sceNpCppWebApiUnknown128);
int APS5_VABI sceNpCppWebApiUnknown128(void) {
    NotImplemented_nid_no_patch("RQfqM1BZuoc");
    return 0;
}

APS5_EXPORT("Rr1p24Dyq+w", sceNpCppWebApiUnknown129);
int APS5_VABI sceNpCppWebApiUnknown129(void) {
    NotImplemented_nid_no_patch("Rr1p24Dyq+w");
    return 0;
}

APS5_EXPORT("TJIV4zGjz8o", sceNpCppWebApiUnknown130);
int APS5_VABI sceNpCppWebApiUnknown130(void) {
    NotImplemented_nid_no_patch("TJIV4zGjz8o");
    return 0;
}

APS5_EXPORT("TpeZWm8upgg", sceNpCppWebApiUnknown131);
int APS5_VABI sceNpCppWebApiUnknown131(void) {
    NotImplemented_nid_no_patch("TpeZWm8upgg");
    return 0;
}

APS5_EXPORT("U-Mgu8Pn9y0", sceNpCppWebApiUnknown132);
int APS5_VABI sceNpCppWebApiUnknown132(void) {
    NotImplemented_nid_no_patch("U-Mgu8Pn9y0");
    return 0;
}

APS5_EXPORT("URZOfbC4fzU", sceNpCppWebApiUnknown133);
int APS5_VABI sceNpCppWebApiUnknown133(void) {
    NotImplemented_nid_no_patch("URZOfbC4fzU");
    return 0;
}

APS5_EXPORT("Uc5eSnk3dvE", sceNpCppWebApiUnknown134);
int APS5_VABI sceNpCppWebApiUnknown134(void) {
    NotImplemented_nid_no_patch("Uc5eSnk3dvE");
    return 0;
}

APS5_EXPORT("VKaRucCPRyU", sceNpCppWebApiUnknown135);
int APS5_VABI sceNpCppWebApiUnknown135(void) {
    NotImplemented_nid_no_patch("VKaRucCPRyU");
    return 0;
}

APS5_EXPORT("VTZ+4rVcoJM", sceNpCppWebApiUnknown136);
int APS5_VABI sceNpCppWebApiUnknown136(void) {
    NotImplemented_nid_no_patch("VTZ+4rVcoJM");
    return 0;
}

APS5_EXPORT("W+-RA2Vn-cc", sceNpCppWebApiUnknown137);
int APS5_VABI sceNpCppWebApiUnknown137(void) {
    NotImplemented_nid_no_patch("W+-RA2Vn-cc");
    return 0;
}

APS5_EXPORT("WN8MUUVljFU", sceNpCppWebApiUnknown138);
int APS5_VABI sceNpCppWebApiUnknown138(void) {
    NotImplemented_nid_no_patch("WN8MUUVljFU");
    return 0;
}

APS5_EXPORT("WNK0UpGK8Ws", sceNpCppWebApiUnknown139);
int APS5_VABI sceNpCppWebApiUnknown139(void) {
    NotImplemented_nid_no_patch("WNK0UpGK8Ws");
    return 0;
}

APS5_EXPORT("X1JE3HkJST8", sceNpCppWebApiUnknown140);
int APS5_VABI sceNpCppWebApiUnknown140(void) {
    NotImplemented_nid_no_patch("X1JE3HkJST8");
    return 0;
}

APS5_EXPORT("XFQNeE+EwJU", sceNpCppWebApiUnknown141);
int APS5_VABI sceNpCppWebApiUnknown141(void) {
    NotImplemented_nid_no_patch("XFQNeE+EwJU");
    return 0;
}

APS5_EXPORT("Z4nTIsAp+QM", sceNpCppWebApiUnknown142);
int APS5_VABI sceNpCppWebApiUnknown142(void) {
    NotImplemented_nid_no_patch("Z4nTIsAp+QM");
    return 0;
}

APS5_EXPORT("ZYTehDq4VkA", sceNpCppWebApiUnknown143);
int APS5_VABI sceNpCppWebApiUnknown143(void) {
    NotImplemented_nid_no_patch("ZYTehDq4VkA");
    return 0;
}

APS5_EXPORT("ZdVlyQI3f-c", sceNpCppWebApiUnknown144);
int APS5_VABI sceNpCppWebApiUnknown144(void) {
    NotImplemented_nid_no_patch("ZdVlyQI3f-c");
    return 0;
}

APS5_EXPORT("ZuRinzzVx1M", sceNpCppWebApiUnknown145);
int APS5_VABI sceNpCppWebApiUnknown145(void) {
    NotImplemented_nid_no_patch("ZuRinzzVx1M");
    return 0;
}

APS5_EXPORT("aEt4aNpeLwQ", sceNpCppWebApiUnknown146);
int APS5_VABI sceNpCppWebApiUnknown146(void) {
    NotImplemented_nid_no_patch("aEt4aNpeLwQ");
    return 0;
}

APS5_EXPORT("b+v7BB12y5I", sceNpCppWebApiUnknown147);
int APS5_VABI sceNpCppWebApiUnknown147(void) {
    NotImplemented_nid_no_patch("b+v7BB12y5I");
    return 0;
}

APS5_EXPORT("bTZjr816ME4", sceNpCppWebApiUnknown148);
int APS5_VABI sceNpCppWebApiUnknown148(void) {
    NotImplemented_nid_no_patch("bTZjr816ME4");
    return 0;
}

APS5_EXPORT("bjdJEldgxQk", sceNpCppWebApiUnknown149);
int APS5_VABI sceNpCppWebApiUnknown149(void) {
    NotImplemented_nid_no_patch("bjdJEldgxQk");
    return 0;
}

APS5_EXPORT("bvfjdByaA6Q", sceNpCppWebApiUnknown150);
int APS5_VABI sceNpCppWebApiUnknown150(void) {
    NotImplemented_nid_no_patch("bvfjdByaA6Q");
    return 0;
}

APS5_EXPORT("cMtWfMUFIY8", sceNpCppWebApiUnknown151);
int APS5_VABI sceNpCppWebApiUnknown151(void) {
    NotImplemented_nid_no_patch("cMtWfMUFIY8");
    return 0;
}

APS5_EXPORT("cYhnRR03jXU", sceNpCppWebApiUnknown152);
int APS5_VABI sceNpCppWebApiUnknown152(void) {
    NotImplemented_nid_no_patch("cYhnRR03jXU");
    return 0;
}

APS5_EXPORT("cfu8UjoKktY", sceNpCppWebApiUnknown153);
int APS5_VABI sceNpCppWebApiUnknown153(void) {
    NotImplemented_nid_no_patch("cfu8UjoKktY");
    return 0;
}

APS5_EXPORT("d3BbxfGmStE", sceNpCppWebApiUnknown154);
int APS5_VABI sceNpCppWebApiUnknown154(void) {
    NotImplemented_nid_no_patch("d3BbxfGmStE");
    return 0;
}

APS5_EXPORT("dv8KUvfjc8c", sceNpCppWebApiUnknown155);
int APS5_VABI sceNpCppWebApiUnknown155(void) {
    NotImplemented_nid_no_patch("dv8KUvfjc8c");
    return 0;
}

APS5_EXPORT("enuMll33T24", sceNpCppWebApiUnknown156);
int APS5_VABI sceNpCppWebApiUnknown156(void) {
    NotImplemented_nid_no_patch("enuMll33T24");
    return 0;
}

APS5_EXPORT("f9jTmCiIUVA", sceNpCppWebApiUnknown157);
int APS5_VABI sceNpCppWebApiUnknown157(void) {
    NotImplemented_nid_no_patch("f9jTmCiIUVA");
    return 0;
}

APS5_EXPORT("hZTfKpftpsU", sceNpCppWebApiUnknown158);
int APS5_VABI sceNpCppWebApiUnknown158(void) {
    NotImplemented_nid_no_patch("hZTfKpftpsU");
    return 0;
}

APS5_EXPORT("hj40eDtISJY", sceNpCppWebApiUnknown159);
int APS5_VABI sceNpCppWebApiUnknown159(void) {
    NotImplemented_nid_no_patch("hj40eDtISJY");
    return 0;
}

APS5_EXPORT("iLnckwH2WBQ", sceNpCppWebApiUnknown160);
int APS5_VABI sceNpCppWebApiUnknown160(void) {
    NotImplemented_nid_no_patch("iLnckwH2WBQ");
    return 0;
}

APS5_EXPORT("jWoi+KzyT8M", sceNpCppWebApiUnknown161);
int APS5_VABI sceNpCppWebApiUnknown161(void) {
    NotImplemented_nid_no_patch("jWoi+KzyT8M");
    return 0;
}

APS5_EXPORT("jxC6X62V4Yw", sceNpCppWebApiUnknown162);
int APS5_VABI sceNpCppWebApiUnknown162(void) {
    NotImplemented_nid_no_patch("jxC6X62V4Yw");
    return 0;
}

APS5_EXPORT("k7+6XfiVmCw", sceNpCppWebApiUnknown163);
int APS5_VABI sceNpCppWebApiUnknown163(void) {
    NotImplemented_nid_no_patch("k7+6XfiVmCw");
    return 0;
}

APS5_EXPORT("kOeBKp8TgFk", sceNpCppWebApiUnknown164);
int APS5_VABI sceNpCppWebApiUnknown164(void) {
    NotImplemented_nid_no_patch("kOeBKp8TgFk");
    return 0;
}

APS5_EXPORT("l3-83fvIqvM", sceNpCppWebApiUnknown165);
int APS5_VABI sceNpCppWebApiUnknown165(void) {
    NotImplemented_nid_no_patch("l3-83fvIqvM");
    return 0;
}

APS5_EXPORT("lje+7Q7rn5U", sceNpCppWebApiUnknown166);
int APS5_VABI sceNpCppWebApiUnknown166(void) {
    NotImplemented_nid_no_patch("lje+7Q7rn5U");
    return 0;
}

APS5_EXPORT("nbRU58b2L1E", sceNpCppWebApiUnknown167);
int APS5_VABI sceNpCppWebApiUnknown167(void) {
    NotImplemented_nid_no_patch("nbRU58b2L1E");
    return 0;
}

APS5_EXPORT("npMIfvNUuiQ", sceNpCppWebApiUnknown168);
int APS5_VABI sceNpCppWebApiUnknown168(void) {
    NotImplemented_nid_no_patch("npMIfvNUuiQ");
    return 0;
}

APS5_EXPORT("o7Rj82lRZ98", sceNpCppWebApiUnknown169);
int APS5_VABI sceNpCppWebApiUnknown169(void) {
    NotImplemented_nid_no_patch("o7Rj82lRZ98");
    return 0;
}

APS5_EXPORT("pDUQVO32lZY", sceNpCppWebApiUnknown170);
int APS5_VABI sceNpCppWebApiUnknown170(void) {
    NotImplemented_nid_no_patch("pDUQVO32lZY");
    return 0;
}

APS5_EXPORT("q0CBI7DJ0X8", sceNpCppWebApiUnknown171);
int APS5_VABI sceNpCppWebApiUnknown171(void) {
    NotImplemented_nid_no_patch("q0CBI7DJ0X8");
    return 0;
}

APS5_EXPORT("qKfj8jo1vcE", sceNpCppWebApiUnknown172);
int APS5_VABI sceNpCppWebApiUnknown172(void) {
    NotImplemented_nid_no_patch("qKfj8jo1vcE");
    return 0;
}

APS5_EXPORT("rYWJ3CJ8+6w", sceNpCppWebApiUnknown173);
int APS5_VABI sceNpCppWebApiUnknown173(void) {
    NotImplemented_nid_no_patch("rYWJ3CJ8+6w");
    return 0;
}

APS5_EXPORT("rq73eO5xSRc", sceNpCppWebApiUnknown174);
int APS5_VABI sceNpCppWebApiUnknown174(void) {
    NotImplemented_nid_no_patch("rq73eO5xSRc");
    return 0;
}

APS5_EXPORT("tnym874g8jE", sceNpCppWebApiUnknown175);
int APS5_VABI sceNpCppWebApiUnknown175(void) {
    NotImplemented_nid_no_patch("tnym874g8jE");
    return 0;
}

APS5_EXPORT("tzzWE9T5+j8", sceNpCppWebApiUnknown176);
int APS5_VABI sceNpCppWebApiUnknown176(void) {
    NotImplemented_nid_no_patch("tzzWE9T5+j8");
    return 0;
}

APS5_EXPORT("us+hb1r9BN4", sceNpCppWebApiUnknown177);
int APS5_VABI sceNpCppWebApiUnknown177(void) {
    NotImplemented_nid_no_patch("us+hb1r9BN4");
    return 0;
}

APS5_EXPORT("veRySWMcttw", sceNpCppWebApiUnknown178);
int APS5_VABI sceNpCppWebApiUnknown178(void) {
    NotImplemented_nid_no_patch("veRySWMcttw");
    return 0;
}

APS5_EXPORT("vpkkVwCPaxE", sceNpCppWebApiUnknown179);
int APS5_VABI sceNpCppWebApiUnknown179(void) {
    NotImplemented_nid_no_patch("vpkkVwCPaxE");
    return 0;
}

APS5_EXPORT("vzKNY2nY7Jk", sceNpCppWebApiUnknown180);
int APS5_VABI sceNpCppWebApiUnknown180(void) {
    NotImplemented_nid_no_patch("vzKNY2nY7Jk");
    return 0;
}

APS5_EXPORT("wi2nosL6l1k", sceNpCppWebApiUnknown181);
int APS5_VABI sceNpCppWebApiUnknown181(void) {
    NotImplemented_nid_no_patch("wi2nosL6l1k");
    return 0;
}

APS5_EXPORT("xC33v2DhfRM", sceNpCppWebApiUnknown182);
int APS5_VABI sceNpCppWebApiUnknown182(void) {
    NotImplemented_nid_no_patch("xC33v2DhfRM");
    return 0;
}

APS5_EXPORT("xg9A5nBRkB0", sceNpCppWebApiUnknown183);
int APS5_VABI sceNpCppWebApiUnknown183(void) {
    NotImplemented_nid_no_patch("xg9A5nBRkB0");
    return 0;
}

APS5_EXPORT("yOUuI64kBuc", sceNpCppWebApiUnknown184);
int APS5_VABI sceNpCppWebApiUnknown184(void) {
    NotImplemented_nid_no_patch("yOUuI64kBuc");
    return 0;
}

APS5_EXPORT("zXdVl2D7dUk", sceNpCppWebApiUnknown185);
int APS5_VABI sceNpCppWebApiUnknown185(void) {
    NotImplemented_nid_no_patch("zXdVl2D7dUk");
    return 0;
}
}
