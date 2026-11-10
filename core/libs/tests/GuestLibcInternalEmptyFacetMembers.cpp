#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibcInternal empty facet members: %s\n", message);
        std::abort();
    }
}

}

int main() {
    unsigned char object[0x80];
    unsigned char locinfo[0x80];
    std::memset(object, 0xa5, sizeof(object));
    std::memset(locinfo, 0x5a, sizeof(locinfo));
    unsigned char untouchedObject[sizeof(object)];
    unsigned char untouchedLocinfo[sizeof(locinfo)];
    std::memcpy(untouchedObject, object, sizeof(object));
    std::memcpy(untouchedLocinfo, locinfo, sizeof(locinfo));
    _ZNSt13bad_exceptionD1Ev(object);
    _ZNSt13bad_exceptionD2Ev(object);
    _ZNSt7codecvtIDic9_MbstatetED1Ev(object);
    _ZNSt7codecvtIcc9_MbstatetED1Ev(object);
    _ZNSt7codecvtIwc9_MbstatetED1Ev(object);
    _ZNSt7collateIcED1Ev(object);
    _ZNSt7collateIcED2Ev(object);
    _ZNSt7collateIwED1Ev(object);
    _ZNSt7collateIwED2Ev(object);
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev(object);
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev(object);
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev(object);
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev(object);
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev(object);
    _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev(object);
    _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev(object);
    _ZNSt8messagesIcE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt8messagesIcED1Ev(object);
    _ZNSt8messagesIcED2Ev(object);
    _ZNSt8messagesIwED1Ev(object);
    _ZNSt8messagesIwED2Ev(object);
    _ZNSt8numpunctIcE5_TidyEv(object);
    _ZNSt8numpunctIwE5_TidyEv(object);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_TidyEv(object);
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_TidyEv(object);
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev(object);
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev(object);
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev(object);
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev(object);
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev(object);
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev(object);
    _ZNSt9type_infoD1Ev(object);
    _ZNSt9type_infoD2Ev(object);
    _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev(object);
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev(object);
    _ZNSt8messagesIwE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev(object);
    _ZNSt7codecvtIcc9_MbstatetED2Ev(object);
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev(object);
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev(object);
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev(object);
    _ZNSt7codecvtIDsc9_MbstatetED1Ev(object);
    _ZNSt7codecvtIcc9_MbstatetE5_InitERKSt8_Locinfo(object, locinfo);
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev(object);
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(object, locinfo);
    Require(std::memcmp(object, untouchedObject, sizeof(object)) == 0, "an empty member wrote the object");
    Require(std::memcmp(locinfo, untouchedLocinfo, sizeof(locinfo)) == 0, "an _Init member wrote the _Locinfo");
    return 0;
}
