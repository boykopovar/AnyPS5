#include "prx/libSceNpPartner001/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

}

int main() {
    bool subscribed = true;
    Require(Throws([] { sceNpEAAccessTerminate(); }));
    Require(Throws([&] { sceNpHasEAAccessSubscription(1, &subscribed); }));
    Require(sceNpEAAccessInitialize() == 0);
    Require(Throws([] { sceNpEAAccessInitialize(); }));
    Require(Throws([] { sceNpHasEAAccessSubscription(1, nullptr); }));
    Require(sceNpHasEAAccessSubscription(1, &subscribed) == 0);
    Require(!subscribed);
    subscribed = true;
    Require(sceNpHasEAAccessSubscription(0, &subscribed) == 0);
    Require(!subscribed);
    Require(sceNpEAAccessTerminate() == 0);
    Require(Throws([] { sceNpEAAccessTerminate(); }));
    Require(Throws([&] { sceNpHasEAAccessSubscription(1, &subscribed); }));
    Require(sceNpEAAccessInitialize() == 0);
    Require(sceNpEAAccessTerminate() == 0);
}
