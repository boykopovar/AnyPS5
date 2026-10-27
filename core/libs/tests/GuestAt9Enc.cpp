#include "prx/libSceAt9Enc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename Call>
bool Throws(Call call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    int32_t memSize = 0;
    int32_t internalResult = 0;

    Require(Throws([&] { return sceAt9EncClearContext(nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncCreateEncoder(nullptr, nullptr, 0, &internalResult); }));
    Require(Throws([&] { return sceAt9EncEncode(nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncFlush(nullptr, nullptr, 0, nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncQueryMemSize(nullptr, &memSize, &internalResult); }));

    Require(memSize == 0);
    Require(internalResult == 0);
}
