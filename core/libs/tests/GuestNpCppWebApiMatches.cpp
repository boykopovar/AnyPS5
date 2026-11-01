#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

extern "C" {
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
void APS5_VABI _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEEC1EPNS2_10LibContextE(IntrusivePtr* self, void* libContext);
}

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

template <typename T>
T Read(const std::uint8_t* object, const std::size_t offset) {
    T value;
    std::memcpy(&value, object + offset, sizeof(value));
    return value;
}

template <typename T>
void Write(std::uint8_t* object, const std::size_t offset, const T value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

}

int main() {
    alignas(8) std::array<std::uint8_t, 0x200> request{};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer12setAccountIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 96 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 96 + 0x08), "value") == 0);
    Require(request[128] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer13setPlayerNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V111AddedPlayer9setTeamIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 136 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 136 + 0x08), "value") == 0);
    Require(request[168] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam11setTeamNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 48 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 48 + 0x08), "value") == 0);
    Require(request[80] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest9setZoneIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer12setAccountIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 96 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 96 + 0x08), "value") == 0);
    Require(request[128] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestMatchPlayer13setPlayerNameEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 56 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 56 + 0x08), "value") == 0);
    Require(request[88] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest9setZoneIdEPKc(request.data(), "value") == 0);
    Require(Read<const char*>(request.data(), 24 + 0x08) != nullptr && std::strcmp(Read<const char*>(request.data(), 24 + 0x08), "value") == 0);
    Require(request[56] == 1);
    std::memset(request.data(), 0, request.size());
    const int intValue = 0x1234;
    Require(_ZN3sce2Np9CppWebApi7Matches2V113RemovedPlayer9setReasonERKNS3_11LeaveReasonE(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 48) == intValue && request[52] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116JoinMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V117LeaveMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setExpirationTimeERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 92) == intValue && request[96] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCooperativeResultERKNS3_24RequestCooperativeResultE(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 20) == intValue && request[24] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setExpirationTimeERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 60) == intValue && request[64] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchStatusRequest17setNpServiceLabelERKi(request.data(), &intValue) == 0);
    Require(Read<std::int32_t>(request.data(), 16) == intValue && request[20] == 1);
    std::memset(request.data(), 0, request.size());
    const double doubleValue = 1.5;
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults8setScoreERKd(request.data(), &doubleValue) == 0);
    Require(Read<double>(request.data(), 56) == doubleValue && request[64] == 1);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestPlayerResults8setScoreERKd(request.data(), &doubleValue) == 0);
    Require(Read<double>(request.data(), 56) == doubleValue && request[64] == 1);
    alignas(8) std::array<std::uint8_t, 0x40> objectBody{};
    Write<std::int32_t>(objectBody.data(), 0x00, 1);
    const IntrusivePtr objectValue{objectBody.data(), nullptr, nullptr};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118CreateMatchRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 152) == objectBody.data() && request[176] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestMatchResults20setCompetitiveResultERKNS1_6Common12IntrusivePtrINS3_24RequestCompetitiveResultEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 32) == objectBody.data() && request[56] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120ReportResultsRequest18setMatchStatisticsERKNS1_6Common12IntrusivePtrINS3_22RequestMatchStatisticsEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 48) == objectBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124UpdateMatchDetailRequest15setInGameRosterERKNS1_6Common12IntrusivePtrINS3_19RequestInGameRosterEEE(request.data(), &objectValue) == 0);
    Require(Read<void*>(request.data(), 112) == objectBody.data() && request[136] == 1);
    Require(Read<std::int32_t>(objectBody.data(), 0x00) >= 2);
    alignas(8) std::array<std::uint8_t, 0x40> vectorBody{};
    Write<std::int32_t>(vectorBody.data(), 0x30, 1);
    const IntrusivePtr vectorValue{vectorBody.data(), nullptr, nullptr};
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 88) == vectorBody.data() && request[112] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 72) == vectorBody.data() && request[96] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 80) == vectorBody.data() && request[104] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 48) == vectorBody.data() && request[72] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE(request.data(), &vectorValue) == 0);
    Require(Read<void*>(request.data(), 16) == vectorBody.data() && request[40] == 1);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) >= 2);
    std::uint64_t vectorContext = 9;
    alignas(8) IntrusivePtr emptyMembers{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEEC1EPNS2_10LibContextE(&emptyMembers, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V116RequestMatchTeam10setMembersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_13RequestMemberEEEEE(request.data(), &emptyMembers) == 0);
    Require(Read<void*>(request.data(), 88) == nullptr && request[112] == 1);
    alignas(8) IntrusivePtr emptyTeamMemberResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V123RequestTeamMemberResultEEEEC1EPNS2_10LibContextE(&emptyTeamMemberResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V118RequestTeamResults20setTeamMemberResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_23RequestTeamMemberResultEEEEE(request.data(), &emptyTeamMemberResults) == 0);
    Require(Read<void*>(request.data(), 72) == nullptr && request[96] == 1);
    alignas(8) IntrusivePtr emptyPlayers{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEEC1EPNS2_10LibContextE(&emptyPlayers, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10setPlayersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestMatchPlayerEEEEE(request.data(), &emptyPlayers) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    alignas(8) IntrusivePtr emptyTeams{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEEC1EPNS2_10LibContextE(&emptyTeams, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V119RequestInGameRoster8setTeamsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_16RequestMatchTeamEEEEE(request.data(), &emptyTeams) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyTeamMemberStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V126RequestTeamMemberStatisticEEEEC1EPNS2_10LibContextE(&emptyTeamMemberStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic23setTeamMemberStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_26RequestTeamMemberStatisticEEEEE(request.data(), &emptyTeamMemberStatistics) == 0);
    Require(Read<void*>(request.data(), 80) == nullptr && request[104] == 1);
    alignas(8) IntrusivePtr emptyStats{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEEC1EPNS2_10LibContextE(&emptyStats, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V120RequestTeamStatistic8setStatsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_19AdditionalStatisticEEEEE(request.data(), &emptyStats) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyTeamStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEEC1EPNS2_10LibContextE(&emptyTeamStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics17setTeamStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestTeamStatisticEEEEE(request.data(), &emptyTeamStatistics) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyPlayerStatistics{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEEC1EPNS2_10LibContextE(&emptyPlayerStatistics, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V122RequestMatchStatistics19setPlayerStatisticsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_22RequestPlayerStatisticEEEEE(request.data(), &emptyPlayerStatistics) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    alignas(8) IntrusivePtr emptyTeamResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestTeamResultsEEEEC1EPNS2_10LibContextE(&emptyTeamResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult14setTeamResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_18RequestTeamResultsEEEEE(request.data(), &emptyTeamResults) == 0);
    Require(Read<void*>(request.data(), 48) == nullptr && request[72] == 1);
    alignas(8) IntrusivePtr emptyPlayerResults{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestPlayerResultsEEEEC1EPNS2_10LibContextE(&emptyPlayerResults, &vectorContext);
    std::memset(request.data(), 0, request.size());
    Require(_ZN3sce2Np9CppWebApi7Matches2V124RequestCompetitiveResult16setPlayerResultsERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_20RequestPlayerResultsEEEEE(request.data(), &emptyPlayerResults) == 0);
    Require(Read<void*>(request.data(), 16) == nullptr && request[40] == 1);
    std::memset(request.data(), 0, request.size());
    Write<std::uint8_t>(request.data(), 0x48, 1);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster10teamsIsSetEv(request.data()));
    Write<std::uint8_t>(request.data(), 0x28, 1);
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119RequestInGameRoster12playersIsSetEv(request.data()));
    Require(_ZNK3sce2Np9CppWebApi7Matches2V119CreateMatchResponse10getMatchIdEv(request.data()) == request.data() + 0x30);
    alignas(8) std::array<std::uint8_t, 0x10> joinParameter{};
    alignas(8) std::array<std::uint8_t, 0x40> joinBodyObject{};
    Write<std::int32_t>(joinBodyObject.data(), 0x00, 1);
    const IntrusivePtr joinBody{joinBodyObject.data(), nullptr, nullptr};
    std::uint64_t context = 1;
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(joinParameter.data(), &context, "room", &joinBody) == 0);
    Require(Read<void*>(joinParameter.data(), 0x08) != nullptr);
    Require(Read<std::int32_t>(joinBodyObject.data(), 0x00) == 2);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(joinParameter.data(), &context, "room", &joinBody) == static_cast<int>(0x8055350f));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv(joinParameter.data()) == 0);
    Require(Read<std::int32_t>(joinBodyObject.data(), 0x00) == 1);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch9terminateEv(joinParameter.data()) == static_cast<int>(0x80553510));

    alignas(8) std::array<std::uint8_t, 0x10> emptyParameter{};
    const IntrusivePtr emptyBody{nullptr, nullptr, nullptr};
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &context, "room", &emptyBody) == static_cast<int>(0x80553512));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), nullptr, "room", &joinBody) == static_cast<int>(0x80553503));
    std::uint64_t zero = 0;
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &zero, "room", &joinBody) == static_cast<int>(0x80553502));
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi20ParameterToJoinMatch10initializeEPNS1_6Common10LibContextEPKcNS6_12IntrusivePtrINS3_16JoinMatchRequestEEE(emptyParameter.data(), &context, nullptr, &joinBody) == static_cast<int>(0x80553503));

    alignas(8) std::array<std::uint8_t, 0x10> createParameter{};
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch10initializeEPNS1_6Common10LibContextENS6_12IntrusivePtrINS3_18CreateMatchRequestEEE(createParameter.data(), &context, &joinBody) == 0);
    Require(_ZN3sce2Np9CppWebApi7Matches2V18MatchApi22ParameterToCreateMatch9terminateEv(createParameter.data()) == 0);

    return 0;
}
