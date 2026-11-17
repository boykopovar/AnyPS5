#include "prx/libfontconfig/include/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    Require(FcConfigGetCurrent() == nullptr, "FcConfigGetCurrent returns no configuration");
    Require(FcConfigGetCurrent() == nullptr, "a second FcConfigGetCurrent still returns no configuration");
    Require(FcInitBringUptoDate() == 1, "FcInitBringUptoDate reports the configuration up to date");
    Require(FcInitBringUptoDate() == 1, "a second FcInitBringUptoDate still reports it up to date");
    Require(FcConfigGetCurrent() == nullptr, "FcInitBringUptoDate creates no current configuration");
    Require(FcInit() == 1, "FcInit reports the configuration initialized");
    Require(FcConfigGetCurrent() == nullptr, "FcInit creates no current configuration");
}
