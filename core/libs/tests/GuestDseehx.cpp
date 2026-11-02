#include "prx/libSceDseehx/include/Api.hpp"
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
    Require(ThrowsMessage([] { sceDseehxFlush(); }, "sceDseehxFlush: not initialized"));
    Require(ThrowsMessage([] { sceDseehxConvert(); }, "sceDseehxConvert: not initialized"));

    Require(sceDseehxInit() == 0);

    Require(ThrowsMessage([] { sceDseehxInit(); }, "sceDseehxInit: already initialized"));
    Require(ThrowsMessage([] { sceDseehxExtInit(); }, "sceDseehxExtInit: already initialized"));

    Require(sceDseehxFlush() == 0);
    Require(sceDseehxFlush() == 0);

    return 0;
}
