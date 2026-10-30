#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
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

using SimpleFn = int (APS5_VABI*)(void*);

const SimpleFn kSetters[] = {
    _ZN3sce2Np9CppWebApi12Leaderboards2V121GetRankingRequestBody17setNpServiceLabelERKj,
    _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody15setWaitsForDataERKb,
    _ZN3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17setNpServiceLabelERKj,
    _ZN3sce2Np9CppWebApi12Leaderboards2V19BoardsApi29ParameterToGetBoardDefinition9setfieldsEPKc,
    _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer14setkeepHtmlTagEb,
    _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer20setuseCurrencySymbolEb,
};
const SimpleFn kAccessors[] = {
    _ZN3sce2Np9CppWebApi13InGameCatalog2V59Container7getSkusEv,
    _ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody12entriesIsSetEv,
    _ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody18getTotalEntryCountEv,
    _ZNK3sce2Np9CppWebApi12Leaderboards2V122RecordScoreRequestBody17needsTmpRankIsSetEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku10getEndDateEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V53Sku12endDateIsSetEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container13childrenIsSetEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDescriptionEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getDisplayNameEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container14getReleaseDateEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container16releaseDateIsSetEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container5getIdEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container7getTypeEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container8getLabelEv,
    _ZNK3sce2Np9CppWebApi13InGameCatalog2V59Container9skusIsSetEv,
};
template <typename TCall>
void RequireThrows(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
}

}

int main() {
    for (const auto fn : kSetters) Require(fn(nullptr) == 0);
    for (const auto fn : kAccessors) RequireThrows([&] { (void)fn(nullptr); });
    return 0;
}
