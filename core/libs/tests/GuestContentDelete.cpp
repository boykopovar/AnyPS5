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

template <typename TCall>
bool ReportsNotImplemented(const char* name, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

}

int main() {
    Require(sceContentDeleteTerminate() == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteInitialize(nullptr) == static_cast<int>(0x809D5001u));
    Require(sceContentDeleteById(1) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("/user/av_contents/photo/a.png") == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath(nullptr) == static_cast<int>(0x809D5002u));

    ContentDeleteInitParam param{};
    for (const std::uint32_t badHeap : {0u, 0x1000u, 0x3fffu, 0x4001u, 0x10000u}) {
        param.heap_size = badHeap;
        Require(sceContentDeleteInitialize(&param) == static_cast<int>(0x809D5001u));
    }
    param.heap_size = 0x4000;
    Require(sceContentDeleteInitialize(&param) == 0);
    Require(sceContentDeleteInitialize(&param) == static_cast<int>(0x809D5003u));
    Require(sceContentDeleteInitialize(nullptr) == static_cast<int>(0x809D5001u));
    Require(sceContentDeleteByPath("") == static_cast<int>(0x809D5001u));

    Require(LogicErrorMessage([] { sceContentDeleteByPath(nullptr); }) == "invalid argument");
    Require(ReportsNotImplemented("sceContentDeleteById", [] { sceContentDeleteById(1); }));
    Require(ReportsNotImplemented("sceContentDeleteByPath", [] { sceContentDeleteByPath("/user/av_contents/photo/a.png"); }));
    Require(ReportsNotImplemented("sceContentDeleteByIdListForSf", [] { std::int64_t id = 1; sceContentDeleteByIdListForSf(&id, 1); }));
    Require(ReportsNotImplemented("sceContentDeleteGlobalMstat", [] { sceContentDeleteGlobalMstat(); }));
    Require(ReportsNotImplemented("sceContentDeleteMemoryInitialize", [] { sceContentDeleteMemoryInitialize(); }));
    Require(ReportsNotImplemented("sceContentDeleteMemoryTerminate", [] { sceContentDeleteMemoryTerminate(); }));

    Require(sceContentDeleteTerminate() == 0);
    Require(sceContentDeleteTerminate() == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteById(1) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath(nullptr) == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("/user/av_contents/photo/a.png") == static_cast<int>(0x809D5002u));
    Require(sceContentDeleteByPath("") == static_cast<int>(0x809D5002u));
    return 0;
}
