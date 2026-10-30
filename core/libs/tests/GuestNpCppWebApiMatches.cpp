#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

constexpr int kNotSignedIn = static_cast<int>(0x80552907);
constexpr int kAlreadyInitialized = static_cast<int>(0x8055350f);
constexpr int kNotInitialized = static_cast<int>(0x80553510);
constexpr std::size_t kParameterSize = 0x10;
constexpr std::size_t kGuardSize = 0x10;
constexpr unsigned char kFill = 0xA5;

struct Storage {
    alignas(8) unsigned char bytes[kParameterSize + kGuardSize];
};

bool GuardIntact(const Storage& storage) {
    for (std::size_t index = kParameterSize; index < sizeof(storage.bytes); ++index) {
        if (storage.bytes[index] != kFill) return false;
    }
    return true;
}

using SimpleFn = int (APS5_VABI*)(void*);
using InitFn = int (APS5_VABI*)(void*, const std::uint64_t*);

const SimpleFn kSignedOut[] = {
    _ZN3sce2Np9CppWebApi7Matches2V118AddedPlayerFactory6createEPNS1_6Common10LibContextEPKcNS3_10PlayerTypeEPNS5_12IntrusivePtrINS3_11AddedPlayerEEE,
    _ZN3sce2Np9CppWebApi7Matches2V120RemovedPlayerFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RemovedPlayerEEE,
    _ZN3sce2Np9CppWebApi7Matches2V120RequestMemberFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_13RequestMemberEEE,
    _ZN3sce2Np9CppWebApi7Matches2V123JoinMatchRequestFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_11AddedPlayerEEEEEPNS9_INS3_16JoinMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V123RequestMatchTeamFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_16RequestMatchTeamEEE,
    _ZN3sce2Np9CppWebApi7Matches2V124LeaveMatchRequestFactory6createEPNS1_6Common10LibContextERKNS5_6VectorINS5_12IntrusivePtrINS3_13RemovedPlayerEEEEEPNS9_INS3_17LeaveMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V125CreateMatchRequestFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_18CreateMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V125RequestMatchPlayerFactory6createEPNS1_6Common10LibContextEPKcNS3_10PlayerTypeEPNS5_12IntrusivePtrINS3_18RequestMatchPlayerEEE,
    _ZN3sce2Np9CppWebApi7Matches2V125RequestTeamResultsFactory6createEPNS1_6Common10LibContextEPKciPNS5_12IntrusivePtrINS3_18RequestTeamResultsEEE,
    _ZN3sce2Np9CppWebApi7Matches2V126AdditionalStatisticFactory6createEPNS1_6Common10LibContextEPKcS9_PNS5_12IntrusivePtrINS3_19AdditionalStatisticEEE,
    _ZN3sce2Np9CppWebApi7Matches2V126RequestInGameRosterFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_19RequestInGameRosterEEE,
    _ZN3sce2Np9CppWebApi7Matches2V126RequestMatchResultsFactory6createEPNS1_6Common10LibContextENS3_14ResultsVersionEPNS5_12IntrusivePtrINS3_19RequestMatchResultsEEE,
    _ZN3sce2Np9CppWebApi7Matches2V127ReportResultsRequestFactory6createEPNS1_6Common10LibContextENS5_12IntrusivePtrINS3_19RequestMatchResultsEEEPNS8_INS3_20ReportResultsRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V127RequestPlayerResultsFactory6createEPNS1_6Common10LibContextEPKciPNS5_12IntrusivePtrINS3_20RequestPlayerResultsEEE,
    _ZN3sce2Np9CppWebApi7Matches2V127RequestTeamStatisticFactory6createEPNS1_6Common10LibContextEPKcPNS5_12IntrusivePtrINS3_20RequestTeamStatisticEEE,
    _ZN3sce2Np9CppWebApi7Matches2V129RequestMatchStatisticsFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_22RequestMatchStatisticsEEE,
    _ZN3sce2Np9CppWebApi7Matches2V129RequestPlayerStatisticFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEEPNSB_INS3_22RequestPlayerStatisticEEE,
    _ZN3sce2Np9CppWebApi7Matches2V130RequestTeamMemberResultFactory6createEPNS1_6Common10LibContextEPKcdPNS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEE,
    _ZN3sce2Np9CppWebApi7Matches2V131RequestCompetitiveResultFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24RequestCompetitiveResultEEE,
    _ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchDetailRequestFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_24UpdateMatchDetailRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V131UpdateMatchStatusRequestFactory6createEPNS1_6Common10LibContextENS3_12UpdateStatusEPNS5_12IntrusivePtrINS3_24UpdateMatchStatusRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V133RequestTeamMemberStatisticFactory6createEPNS1_6Common10LibContextEPKcRKNS5_6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEEPNSB_INS3_26RequestTeamMemberStatisticEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi10leaveMatchEiRKNS4_21ParameterToLeaveMatchERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi11createMatchEiRKNS4_22ParameterToCreateMatchERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_19CreateMatchResponseEEENSA_INS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi13reportResultsEiRKNS4_24ParameterToReportResultsERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi17updateMatchDetailEiRKNS4_28ParameterToUpdateMatchDetailERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi17updateMatchStatusEiRKNS4_28ParameterToUpdateMatchStatusERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi9joinMatchEiRKNS4_20ParameterToJoinMatchERNS1_6Common11TransactionINS8_15DefaultResponseENS8_12IntrusivePtrINS8_18ResponseHeaderBaseEEEEE,
};
const SimpleFn kAccessors[] = {
    _ZNK3sce2Np9CppWebApi7Matches2V119CreateMatchResponse10getMatchIdEv,
    _ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10teamsIsSetEv,
    _ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster12playersIsSetEv,
};
const SimpleFn kSetters[] = {
    _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer12setAccountIdEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer13setPlayerNameEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer9setTeamIdEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V113RemovedPlayer9setReasonERKNS3_11LeaveReasonE,
    _ZN3sce2Np9CppWebApi7Matches2V116JoinMatchRequest17setNpServiceLabelERKi,
    _ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam11setTeamNameEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V117LeaveMatchRequest17setNpServiceLabelERKi,
    _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE,
    _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setExpirationTimeERKi,
    _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setNpServiceLabelERKi,
    _ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest9setZoneIdEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer12setAccountIdEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer13setPlayerNameEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults8setScoreERKd,
    _ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCompetitiveResultERKNS1_6Common12IntrusivePtrINS3_24RequestCompetitiveResultEEE,
    _ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCooperativeResultERKNS3_24RequestCooperativeResultE,
    _ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest17setNpServiceLabelERKi,
    _ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest18setMatchStatisticsERKNS1_6Common12IntrusivePtrINS3_22RequestMatchStatisticsEEE,
    _ZN3sce2Np9CppWebApi7Matches2V120RequestPlayerResults8setScoreERKd,
    _ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE,
    _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE,
    _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setExpirationTimeERKi,
    _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setNpServiceLabelERKi,
    _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest9setZoneIdEPKc,
    _ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchStatusRequest17setNpServiceLabelERKi,
};
const SimpleFn kConstructors[] = {
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatchC1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatchC1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatchC1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResultsC1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetailC1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatusC1Ev,
};
const SimpleFn kDestructors[] = {
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatchD1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatchD1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatchD1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResultsD1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetailD1Ev,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatusD1Ev,
};
const SimpleFn kTerminators[] = {
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatch9terminateEv,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch9terminateEv,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResults9terminateEv,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatus9terminateEv,
};
const InitFn kInitializers[] = {
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi21ParameterToLeaveMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_17LeaveMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_18CreateMatchRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi24ParameterToReportResults10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_20ReportResultsRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchDetail10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_24UpdateMatchDetailRequestEEE,
    _ZN3sce2Np9CppWebApi7Matches2V18MatchApi28ParameterToUpdateMatchStatus10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_24UpdateMatchStatusRequestEEE,
};

template <typename TCall>
void RequireThrows(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
}

}

int main() {
    for (const auto fn : kSignedOut) Require(fn(nullptr) == kNotSignedIn);
    for (const auto fn : kAccessors) RequireThrows([&] { (void)fn(nullptr); });
    for (const auto fn : kSetters) Require(fn(nullptr) == 0);

    for (const auto fn : kConstructors) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        Require(fn(storage.bytes) == 0);
        Require(GuardIntact(storage));
    }
    for (const auto fn : kDestructors) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        Require(fn(storage.bytes) == 0);
        Require(GuardIntact(storage));
    }
    for (const auto fn : kTerminators) {
        Storage storage;
        std::memset(storage.bytes, 0, sizeof(storage.bytes));
        Require(fn(storage.bytes) == kNotInitialized);
    }
    for (const auto fn : kInitializers) {
        Storage storage;
        std::memset(storage.bytes, kFill, sizeof(storage.bytes));
        std::memset(storage.bytes, 0, kParameterSize);
        std::uint64_t context = 1;
        Require(fn(storage.bytes, &context) == 0);
        Require(fn(storage.bytes, &context) == kAlreadyInitialized);
        Require(kTerminators[0](storage.bytes) == 0);
        Require(kTerminators[0](storage.bytes) == kNotInitialized);
        Require(GuardIntact(storage));
    }
    return 0;
}
