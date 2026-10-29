#include "prx/libSceHmd2/include/Api.hpp"
#include "prx/libSceHmd2/Hmd2.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace {

constexpr std::int32_t InvalidHandle = static_cast<std::int32_t>(0x8a720003);
constexpr std::int32_t ParameterNull = static_cast<std::int32_t>(0x8a720008);
constexpr std::int32_t ParameterInvalid = static_cast<std::int32_t>(0x8a720009);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

template<class T>
T Read(const unsigned char* bytes, std::size_t offset) {
    T value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

bool NegativeZero(float value) {
    return value == 0.0f && std::signbit(value);
}

void CheckGazeResult() {
    Hmd2::GazeResultParameter param {sizeof(Hmd2::GazeResultParameter), 0x0e010000, 0};
    unsigned char result[0x38];
    std::memset(result, 0x5a, sizeof(result));
    const std::uint32_t size = 0x38;
    std::memcpy(result, &size, sizeof(size));

    Require(sceHmd2GazeGetResult(nullptr, reinterpret_cast<Hmd2::GazeResult*>(result)) == ParameterNull, "null param");
    Require(sceHmd2GazeGetResult(&param, nullptr) == ParameterNull, "null result");
    Hmd2::GazeResultParameter wrongParam = param;
    wrongParam.size = 0x18;
    Require(sceHmd2GazeGetResult(&wrongParam, reinterpret_cast<Hmd2::GazeResult*>(result)) == ParameterInvalid, "param size");
    const std::uint32_t wrongSize = 0x18;
    std::memcpy(result, &wrongSize, sizeof(wrongSize));
    Require(sceHmd2GazeGetResult(&param, reinterpret_cast<Hmd2::GazeResult*>(result)) == ParameterInvalid, "result size");
    Require(Read<std::uint32_t>(result, 4) == 0x5a5a5a5a, "result untouched after a size error");

    std::memcpy(result, &size, sizeof(size));
    Require(sceHmd2GazeGetResult(&param, reinterpret_cast<Hmd2::GazeResult*>(result)) == InvalidHandle, "unopened handle");
    Require(Read<std::uint32_t>(result, 0) == 0x38, "size kept");
    Require(Read<std::int32_t>(result, 4) == -1, "+4 is -1");
    for (const std::size_t offset : {0x8u, 0xcu, 0x10u, 0x20u, 0x24u, 0x28u, 0x2cu, 0x30u, 0x34u})
        Require(Read<std::uint32_t>(result, offset) == 0, "cleared word");
    Require(NegativeZero(Read<float>(result, 0x14)), "+0x14 is -0.0");
    Require(Read<std::uint32_t>(result, 0x18) == 0, "+0x18 is 0.0");
    Require(Read<float>(result, 0x1c) == -1.0f, "+0x1c is -1.0");

    Hmd2::GazeResultParameter zero {sizeof(Hmd2::GazeResultParameter), 0, 0};
    Require(sceHmd2GazeGetResult(&zero, reinterpret_cast<Hmd2::GazeResult*>(result)) == InvalidHandle, "handle 0");
}

void CheckFoveatedResult() {
    Hmd2::GazeResultParameter param {sizeof(Hmd2::GazeResultParameter), 0x0e010000, 0};
    unsigned char result[0x18];
    std::memset(result, 0x5a, sizeof(result));
    const std::uint32_t size = 0x18;
    std::memcpy(result, &size, sizeof(size));

    Require(sceHmd2GazeGetResultForFoveatedRendering(nullptr, reinterpret_cast<Hmd2::FoveatedRenderingResult*>(result)) == ParameterNull, "foveated null param");
    Require(sceHmd2GazeGetResultForFoveatedRendering(&param, nullptr) == ParameterNull, "foveated null result");
    unsigned char large[0x38] = {};
    const std::uint32_t largeSize = 0x38;
    std::memcpy(large, &largeSize, sizeof(largeSize));
    Require(sceHmd2GazeGetResultForFoveatedRendering(&param, reinterpret_cast<Hmd2::FoveatedRenderingResult*>(large)) == ParameterInvalid, "foveated result size");

    Require(sceHmd2GazeGetResultForFoveatedRendering(&param, reinterpret_cast<Hmd2::FoveatedRenderingResult*>(result)) == InvalidHandle, "foveated unopened handle");
    Require(Read<std::uint32_t>(result, 0) == 0x18, "foveated size kept");
    Require(Read<std::int32_t>(result, 4) == -1, "foveated +4 is -1");
    Require(NegativeZero(Read<float>(result, 8)), "foveated +8 is -0.0");
    Require(Read<std::uint32_t>(result, 0xc) == 0, "foveated +0xc is 0.0");
    Require(Read<float>(result, 0x10) == -1.0f, "foveated +0x10 is -1.0");
    Require(Read<std::uint32_t>(result, 0x14) == 0x5a5a5a5a, "foveated +0x14 untouched");
}

}

int main() {
    Require(sceHmd2Initialize(nullptr) != 0, "initialize reports no headset");
    CheckGazeResult();
    CheckFoveatedResult();
    std::puts("guest Hmd2 gaze results without a headset: PASS");
    return 0;
}
