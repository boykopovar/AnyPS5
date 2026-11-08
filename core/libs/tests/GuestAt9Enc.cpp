#include "prx/libSceAt9Enc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include <string>

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

template <typename Call>
bool ThrowsMessage(Call call, const char* name) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(name) != std::string::npos;
    }
    return false;
}

}

int main() {
    int32_t memSize = 0;
    int32_t internalResult = 0;

    Require(ThrowsMessage([&] { return sceAt9EncClearContext(nullptr, &internalResult); }, "sceAt9EncClearContext"));
    Require(ThrowsMessage([&] { return sceAt9EncCreateEncoder(nullptr, nullptr, 0, &internalResult); }, "sceAt9EncCreateEncoder"));
    Require(ThrowsMessage([&] { return sceAt9EncEncode(nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr, &internalResult); }, "sceAt9EncEncode"));
    Require(ThrowsMessage([&] { return sceAt9EncFlush(nullptr, nullptr, 0, nullptr, &internalResult); }, "sceAt9EncFlush"));
    Require(ThrowsMessage([&] { return sceAt9EncQueryMemSize(nullptr, &memSize, &internalResult); }, "sceAt9EncQueryMemSize"));

    Require(ThrowsMessage([&] { return sceAt9EncClearContext(nullptr, nullptr); }, "sceAt9EncClearContext"));
    Require(ThrowsMessage([&] { return sceAt9EncCreateEncoder(nullptr, nullptr, 0, nullptr); }, "sceAt9EncCreateEncoder"));
    Require(ThrowsMessage([&] { return sceAt9EncEncode(nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr); }, "sceAt9EncEncode"));
    Require(ThrowsMessage([&] { return sceAt9EncFlush(nullptr, nullptr, 0, nullptr, nullptr); }, "sceAt9EncFlush"));
    Require(ThrowsMessage([&] { return sceAt9EncQueryMemSize(nullptr, nullptr, nullptr); }, "sceAt9EncQueryMemSize"));

    Require(memSize == 0);
    Require(internalResult == 0);
}
