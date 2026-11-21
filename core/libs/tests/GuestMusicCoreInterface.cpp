#include "prx/libSceMusicCoreInterface/libSceMusicCoreInterface.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidArgument = static_cast<int>(0x81795002u);
constexpr int notInitialized = static_cast<int>(0x81795003u);

}

int main() {
    const std::array<std::uint64_t, 4> table{};
    Require(sceMusicCoreIfSetFunctionTable(nullptr) == invalidArgument);
    Require(sceMusicCoreIfSetInternalFunctionTable(nullptr) == invalidArgument);
    Require(sceMusicCoreIfSetFunctionTableExt2(nullptr) == invalidArgument);
    Require(sceMusicCoreIfSetFunctionTableUnlimited(nullptr) == invalidArgument);
    Require(sceMusicCoreIfSetFunctionTable(table.data()) == notInitialized);
    Require(sceMusicCoreIfSetInternalFunctionTable(table.data()) == notInitialized);
    Require(sceMusicCoreIfSetFunctionTableExt2(table.data()) == notInitialized);
    Require(sceMusicCoreIfSetFunctionTableUnlimited(table.data()) == notInitialized);
    Require(sceMusicCoreIfMainLoop() == notInitialized);

    std::array<std::uint64_t, 2> param{};
    Require(sceMusicCoreIfInitializeInterface(nullptr, param.data()) == invalidArgument);
    Require(sceMusicCoreIfInitializeInterface(table.data(), nullptr) == invalidArgument);
    for (const std::uint64_t size : std::array<std::uint64_t, 4>{0, 0xF, 0x11, 0x1000000010ull}) {
        param[0] = size;
        Require(sceMusicCoreIfInitializeInterface(table.data(), param.data()) == invalidArgument);
    }
    Require(sceMusicCoreIfInternalInitializeInterface(nullptr, table.data()) == invalidArgument);
    Require(sceMusicCoreIfInternalInitializeInterface("music", nullptr) == invalidArgument);
    Require(sceMusicCoreIfMainLoop() == notInitialized);
    return 0;
}
