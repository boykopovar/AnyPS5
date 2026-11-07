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

alignas(8) std::array<std::uint8_t, 0x300> Object;

}

int main() {
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
