#include "prx/libSceShareUtility/libSceShareUtility.hpp"

#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int AdvanceResult = static_cast<int>(0x81170001u);

using Query = int (APS5_VABI *)();

} // namespace

int main() {
    for (int iter = 0; iter < 2; ++iter) {
        for (const Query query : std::initializer_list<Query>{
             sceShareUtilityAdvanceFromFileWithContentIdList,
             sceShareUtilityAdvanceFromFileWithTitleIdList,
             sceShareUtilityAdvanceGetServiceInfo,
             sceShareUtilityAdvanceQueryServiceInfo,
             sceShareUtilityAdvanceQueryServiceInfoA,
             sceShareUtilityAdvanceValidateContents,
         }) {
        if (query() != AdvanceResult) std::abort();
        }
    }
}
