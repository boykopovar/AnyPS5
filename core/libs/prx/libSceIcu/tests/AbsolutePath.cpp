#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&uprv_pathIsAbsolute_nid_postfix), std::int8_t (APS5_VABI*)(const char*)>);

}

int main() {
    Require(uprv_pathIsAbsolute_nid_postfix(nullptr) == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("/") == 1);
    Require(uprv_pathIsAbsolute_nid_postfix("/app0/icudt68l.dat") == 1);
    Require(uprv_pathIsAbsolute_nid_postfix("//") == 1);
    Require(uprv_pathIsAbsolute_nid_postfix("app0/data") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("./data") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix(" /data") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("\\data") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("C:\\data") == 0);
    Require(uprv_pathIsAbsolute_nid_postfix("c:/data") == 0);
    return 0;
}
