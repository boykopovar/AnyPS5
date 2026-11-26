#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

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

void CheckRankingRequest() {
    alignas(8) std::array<std::uint8_t, 0x40> context{};
    alignas(8) std::array<std::uint8_t, 0x40> initParams{};
    auto* library = reinterpret_cast<std::uint64_t*>(context.data());
    IntrusivePtr user{}, other{}, request{};
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE(nullptr, &user) == static_cast<int>(0x80553503));
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V128GetRankingRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_21GetRankingRequestBodyEEE(library, nullptr) == static_cast<int>(0x80553503));
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE(library, &user) == static_cast<int>(0x80553504));
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V128GetRankingRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_21GetRankingRequestBodyEEE(library, &request) == static_cast<int>(0x80553504));
    _ZN3sce2Np9CppWebApi6Common10InitParamsC1Ev(initParams.data());
    _ZN3sce2Np9CppWebApi6Common10LibContextC1Ev(library);
    Require(_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(initParams.data(), library) == 0);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE(library, &user) == 0 && _ZN3sce2Np9CppWebApi12Leaderboards2V111UserFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_4UserEEE(library, &other) == 0);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V128GetRankingRequestBodyFactory6createEPNS1_6Common10LibContextEPNS5_12IntrusivePtrINS3_21GetRankingRequestBodyEEE(library, &request) == 0);
    auto* userData = static_cast<std::uint8_t*>(user.object);
    auto* body = static_cast<std::uint8_t*>(request.object);
    Require(userData && body && user.deleter && request.deleter);
    Require(Read<void*>(userData, 8) == library && user.libContext == library);
    Require(Read<void*>(body, 8) == library && request.libContext == library);
    const std::uint64_t account = 0xfedcba9876543210ULL;
    const int negative = -7;
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V14User12setAccountIdERKm(userData, &account) == account);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V14User7setPcIdERKi(userData, &negative) == negative);
    Require(Read<std::uint64_t>(userData, 0x10) == account && userData[0x18] == 1);
    Require(Read<int>(userData, 0x1c) == negative && userData[0x20] == 1);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setGroupERKNS3_5GroupE(body, &negative) == negative && _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setLimitERKi(body, &negative) == negative);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody9setOffsetERKi(body, &negative) == negative && _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody18setStartSerialRankERKi(body, &negative) == negative);
    for (const auto offset : {0x18, 0x20, 0x30, 0x38})
        Require(Read<int>(body, offset) == negative && body[offset + 4] == 1);

    alignas(8) std::array<std::uint8_t, 0x40> vector{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEEC1EPNS2_10LibContextE(vector.data(), library);
    Require(Read<void*>(vector.data(), 0x18) == vector.data());
    Require(Read<void*>(vector.data(), 0x28) == vector.data());
    Require(Read<void*>(vector.data(), 0x38) == library);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setUsersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_4UserEEEEE(body, vector.data()) == 0 && body[0x78] == 1);
    auto* emptyCopy = Read<std::uint8_t*>(body, 0x60);
    Require(emptyCopy && Read<int>(emptyCopy, 0x30) == 1 && Read<void*>(emptyCopy, 0x38) == library);
    for (int i = 0; i < 16; ++i)
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEE8pushBackERKS8_(vector.data(), i % 2 == 0 ? &user : &other) == 0);
    auto* first = Read<IntrusivePtr*>(vector.data(), 0x18);
    Require(Read<IntrusivePtr*>(vector.data(), 0x20) - first == 16);
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEE8pushBackERKS8_(vector.data(), first) == 0);
    first = Read<IntrusivePtr*>(vector.data(), 0x18);
    Require(Read<IntrusivePtr*>(vector.data(), 0x20) - first == 32);
    Require(Read<IntrusivePtr*>(vector.data(), 0x28) - first == 17 && first[16].object == user.object);
    Require(Read<int>(userData, 0) == 10);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setUsersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_4UserEEEEE(body, vector.data()) == 0);
    auto* copy = Read<std::uint8_t*>(body, 0x60);
    auto* elements = Read<IntrusivePtr*>(copy, 0x18);
    Require(copy != vector.data() && elements != first && Read<void*>(body, 0x68) == nullptr);
    Require(Read<IntrusivePtr*>(copy, 0x20) - elements == 34);
    Require(Read<IntrusivePtr*>(copy, 0x28) - elements == 17 && elements[16].object == user.object);
    Require(Read<int>(userData, 0) == 19);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody8setUsersERKNS1_6Common6VectorINS5_12IntrusivePtrINS3_4UserEEEEE(body, vector.data()) == 0 && Read<int>(userData, 0) == 19);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V14UserEEEED1Ev(vector.data());
    Require(Read<int>(userData, 0) == 10);

    alignas(8) std::array<std::uint8_t, 0x10> liveParameter{};
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingC1Ev(liveParameter.data());
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking10initializeEPNS1_6Common10LibContextEi(liveParameter.data(), library, negative) == 0);
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(liveParameter.data(), &request);
    Require(Read<int>(body, 0) == 2);
    alignas(8) auto savedForCleanup = liveParameter;
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingD1Ev(liveParameter.data());
    Require(liveParameter == savedForCleanup && Read<int>(body, 0) == 2);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv(savedForCleanup.data()) == 0 && Read<int>(body, 0) == 1);

    alignas(8) std::array<std::uint8_t, 0x10> parameter{};
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingC1Ev(parameter.data());
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(parameter.data(), &request);
    Require(Read<int>(body, 0) == 1);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking10initializeEPNS1_6Common10LibContextEi(parameter.data(), library, negative) == 0);
    auto* state = Read<std::uint8_t*>(parameter.data(), 8);
    Require(Read<int>(state, 0) == negative && Read<void*>(state, 0x28) == library);
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(parameter.data(), &request);
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(parameter.data(), &request);
    Require(Read<void*>(state, 8) == request.object && state[0x20] == 1 && Read<int>(body, 0) == 2);
    IntrusivePtr empty{};
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(parameter.data(), &empty);
    Require(Read<void*>(state, 8) == nullptr && state[0x20] == 1 && Read<int>(body, 0) == 1);
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking24setgetRankingRequestBodyENS1_6Common12IntrusivePtrINS3_21GetRankingRequestBodyEEE(parameter.data(), &request);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEED1Ev(&request);
    Require(Read<int>(body, 0) == 1);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv(parameter.data()) == 0 && Read<int>(userData, 0) == 1);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRanking9terminateEv(parameter.data()) == static_cast<int>(0x80553510));
    const auto terminatedParameter = parameter;
    _ZN3sce2Np9CppWebApi12Leaderboards2V17ViewApi21ParameterToGetRankingD1Ev(parameter.data());
    Require(parameter == terminatedParameter && Read<int>(userData, 0) == 1);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev(&other);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEED1Ev(&user);
    Require(_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(library) == 0);
}

alignas(8) std::array<std::uint8_t, 0x300> Object;

}

int main() {
    CheckRankingRequest();
    Object.fill(0x5a);
    const auto* model = Object.data();

    Write<std::uint8_t>(Object.data(), 0x48, 1);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody12entriesIsSetEv(model));
    Write<std::uint8_t>(Object.data(), 0x48, 0);
    Require(!_ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody12entriesIsSetEv(model));

    Write<std::int32_t>(Object.data(), 0x58, 0x12345678);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody18getTotalEntryCountEv(model) == 0x12345678);

    Write<std::uint8_t>(Object.data(), 0x42, 1);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17needsTmpRankIsSetEv(model));

    Write<std::int32_t>(Object.data(), 0x30, -2);
    Write<std::uint32_t>(Object.data(), 0x34, 0x89abcdefu);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V123RecordScoreResponseBody10getTmpRankEv(model) == -2);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V123RecordScoreResponseBody16getTmpSerialRankEv(model) == 0x89abcdefu);

    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10getEndDateEv(model) == model + 0x88);
    Write<std::uint8_t>(Object.data(), 0xa8, 1);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv(model));

    Write<std::uint8_t>(Object.data(), 0x240, 1);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container13childrenIsSetEv(model));
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDescriptionEv(model) == model + 0x158);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDisplayNameEv(model) == model + 0x88);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getReleaseDateEv(model) == model + 0xa8);
    Write<std::uint8_t>(Object.data(), 0xc8, 1);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv(model));
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container5getIdEv(model) == model + 0x38);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container7getTypeEv(model) == model + 0x1e8);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container8getLabelEv(model) == model + 0x58);
    Write<std::uint8_t>(Object.data(), 0x220, 1);
    Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv(model));

    alignas(8) std::array<std::uint8_t, 0x300> container{};
    alignas(8) std::array<std::uint8_t, 0x40> vectorBody{};
    Write<std::int32_t>(vectorBody.data(), 0x30, 1);
    const IntrusivePtr vector{vectorBody.data(), nullptr, nullptr};
    std::memcpy(container.data() + 0x208, &vector, sizeof(vector));
    const IntrusivePtr skus = _ZN3sce2Np9CppWebApi13InGameCatalog2V59Container7getSkusEv(container.data());
    Require(skus.object == vectorBody.data() && skus.deleter == nullptr && skus.libContext == nullptr);
    Require(Read<std::int32_t>(vectorBody.data(), 0x30) == 2);

    alignas(8) std::array<std::uint8_t, 0x100> rankingBody{};
    const std::uint32_t label = 7;
    _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody17setNpServiceLabelERKj(rankingBody.data(), &label);
    Require(Read<std::uint32_t>(rankingBody.data(), 0x28) == label && rankingBody[0x2c] == 1);

    alignas(8) std::array<std::uint8_t, 0x100> scoreBody{};
    const bool waits = true;
    _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody15setWaitsForDataERKb(scoreBody.data(), &waits);
    Require(scoreBody[0x79] == 1 && scoreBody[0x7a] == 1);
    _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17setNpServiceLabelERKj(scoreBody.data(), &label);
    Require(Read<std::uint32_t>(scoreBody.data(), 0x44) == label && scoreBody[0x48] == 1);

    alignas(8) std::array<std::uint8_t, 0x10> boardParameter{};
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9setfieldsEPKc(boardParameter.data(), "x") == static_cast<int>(0x80553502));
    std::uint64_t context = 1;
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition10initializeEPNS1_6Common10LibContextEi(boardParameter.data(), &context, 42) == 0);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9setfieldsEPKc(boardParameter.data(), "abc") == 0);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9setfieldsEPKc(boardParameter.data(), nullptr) == static_cast<int>(0x80553503));
    void* boardImplementation = Read<void*>(boardParameter.data(), 0x08);
    const auto* boardFields = reinterpret_cast<const CommonString*>(static_cast<const std::uint8_t*>(boardImplementation) + 0x08);
    Require(boardFields->buffer != nullptr && std::strcmp(boardFields->buffer, "abc") == 0);
    Require(Read<std::int32_t>(static_cast<const std::uint8_t*>(boardImplementation), 0x00) == 42);
    Require(static_cast<const std::uint8_t*>(boardImplementation)[0x28] == 1);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition10initializeEPNS1_6Common10LibContextEi(boardParameter.data(), &context, 42) == static_cast<int>(0x8055350f));
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9terminateEv(boardParameter.data()) == 0);
    Require(_ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9terminateEv(boardParameter.data()) == static_cast<int>(0x80553510));

    alignas(8) std::array<std::uint8_t, 0x10> containerParameter{};
    Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer10initializeEPNS1_6Common10LibContextE(containerParameter.data(), &context) == 0);
    _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer14setkeepHtmlTagEb(containerParameter.data(), true);
    _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer20setuseCurrencySymbolEb(containerParameter.data(), false);
    void* containerImplementation = Read<void*>(containerParameter.data(), 0x08);
    Require(static_cast<const std::uint8_t*>(containerImplementation)[0x29] == 1);
    Require(static_cast<const std::uint8_t*>(containerImplementation)[0x2a] == 1);
    Require(static_cast<const std::uint8_t*>(containerImplementation)[0x53] == 0);
    Require(static_cast<const std::uint8_t*>(containerImplementation)[0x54] == 1);
    Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer9terminateEv(containerParameter.data()) == 0);
    Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer9terminateEv(containerParameter.data()) == static_cast<int>(0x80553510));

    std::uint64_t zero = 0;
    Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer10initializeEPNS1_6Common10LibContextE(containerParameter.data(), nullptr) == static_cast<int>(0x80553503));
    Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer10initializeEPNS1_6Common10LibContextE(containerParameter.data(), &zero) == static_cast<int>(0x80553502));
    return 0;
}
