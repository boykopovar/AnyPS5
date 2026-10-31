#include "prx/libSceContentDelete/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
std::string LogicErrorMessage(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return "invalid argument";
    } catch (const std::logic_error& error) {
        return error.what();
    }
    return "returned";
}

// True when ByIdListForSf, given a non-null pointer and count >= 0x124,
// passes validation and reaches the unpatched body (runtime_error).
bool ReachesUnpatchedBody(const std::int64_t* contentIds, std::size_t count) {
    try {
        sceContentDeleteByIdListForSf(contentIds, count);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "sceContentDeleteByIdListForSf not implemented";
    } catch (...) {
        return false;
    }
    return false;
}

}

int main() {
    // Not initialized: every delete path reports NotInitialized (0x809D5002).
    Require(sceContentDeleteTerminate() == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteInitialize(nullptr) == static_cast<int>(0x809D5001u));
    Require(sceContentDeleteById(1) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("/user/av_contents/photo/a.png") == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath(nullptr) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByIdListForSf(nullptr, 0) == static_cast<int>(0x809D5002u));

    ContentDeleteInitParam param{};
    param.heap_size = 0x10000;
    Require(sceContentDeleteInitialize(&param) == static_cast<int>(0x809D5001u));
    param.heap_size = 0x4000;
    Require(sceContentDeleteInitialize(&param) == 0);
    Require(sceContentDeleteInitialize(&param) == static_cast<int>(0x809D5003u));
    Require(sceContentDeleteInitialize(nullptr) == static_cast<int>(0x809D5001u));
    Require(sceContentDeleteByPath("") == static_cast<int>(0x809D5001u));

    // ByIdListForSf validation (initialized state): null/zero count -> InvalidArgument (0x809D5001).
    std::int64_t dummyId = 1;
    Require(sceContentDeleteByIdListForSf(nullptr, 0) == static_cast<int>(0x809D5001u));
    Require(sceContentDeleteByIdListForSf(&dummyId, 0) == static_cast<int>(0x809D5001u));
    // count == 1 passes the threshold (count >= 1) and reaches the unpatched body (runtime_error).
    Require(ReachesUnpatchedBody(&dummyId, 1));
    // A larger list (count > 1) also reaches the unpatched body.
    std::int64_t longIds[8];
    for (std::size_t i = 0; i < 8; ++i) longIds[i] = static_cast<std::int64_t>(i + 1);
    Require(ReachesUnpatchedBody(longIds, 8));

    Require(LogicErrorMessage([] { sceContentDeleteByPath(nullptr); }) == "invalid argument");

    Require(sceContentDeleteTerminate() == 0);
    Require(sceContentDeleteTerminate() == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteById(1) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath(nullptr) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("/user/av_contents/photo/a.png") == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("") == static_cast<int>(0x809D5002u));
    return 0;
}
