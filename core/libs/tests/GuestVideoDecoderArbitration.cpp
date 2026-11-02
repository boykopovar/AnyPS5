#include "prx/libSceVideoDecoderArbitration/include/Api.hpp"
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
    Require(ThrowsMessage([] { sceVideoDecoderArbitrationEnable(); }, "not initialized"));
    Require(ThrowsMessage([] { sceVideoDecoderArbitrationEnableSuspendMode(); }, "not initialized"));
    Require(ThrowsMessage([] { sceVideoDecoderArbitrationAcceptEvent(); }, "not initialized"));
    Require(sceVideoDecoderArbitrationInitialize() == 0);
    Require(ThrowsMessage([] { sceVideoDecoderArbitrationInitialize(); }, "already initialized"));
    return 0;
}
