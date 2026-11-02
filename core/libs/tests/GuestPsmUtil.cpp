#include "prx/libScePsmUtil/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::logic_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

}

int main() {
    Require(ThrowsMessage([] { scePsmUtilFinalize(); }, "not initialized"));
    Require(ThrowsMessage([] { scePsmUtilGetPsmIniFile(); }, "not initialized"));
    Require(ThrowsMessage([] { scePsmUtilRegisterCallbacks(); }, "not initialized"));

    Require(scePsmUtilInitialize() == 0);
    Require(ThrowsMessage([] { scePsmUtilInitialize(); }, "already initialized"));

    Require(scePsmUtilFinalize() == 0);

    Require(ThrowsMessage([] { scePsmUtilFinalize(); }, "not initialized"));
    Require(ThrowsMessage([] { scePsmUtilGetPsmIniFile(); }, "not initialized"));
    Require(ThrowsMessage([] { scePsmUtilRegisterCallbacks(); }, "not initialized"));

    Require(scePsmUtilInitialize() == 0);
    Require(ThrowsMessage([] { scePsmUtilInitialize(); }, "already initialized"));
    Require(scePsmUtilFinalize() == 0);
    Require(ThrowsMessage([] { scePsmUtilFinalize(); }, "not initialized"));

    return 0;
}
