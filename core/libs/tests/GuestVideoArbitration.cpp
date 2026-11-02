#include "prx/libSceVideoArbitration/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
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
    Require(ThrowsMessage([] { sceVideoArbitrationEnable(); }, "not initialized"));
    Require(ThrowsMessage([] { sceVideoArbitrationAcceptEvent(); }, "not initialized"));
    Require(sceVideoArbitrationInitialize() == 0);
    Require(ThrowsMessage([] { sceVideoArbitrationInitialize(); }, "already initialized"));
    return 0;
}
