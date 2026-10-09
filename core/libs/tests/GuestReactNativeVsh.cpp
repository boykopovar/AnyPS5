#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <exception>
#include <initializer_list>

extern "C" {
int APS5_VABI RemotePlayGetConnectionStatus(int, int*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    for (int user : {-1, 0, 1, 0x10000}) {
        int status = 0x5a5a5a5a;
        Require(RemotePlayGetConnectionStatus(user, &status) == 0);
        Require(status == 0);
    }
    bool rejected = false;
    try {
        RemotePlayGetConnectionStatus(1, nullptr);
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}
