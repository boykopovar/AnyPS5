#include "prx/libSceAc3Enc/libSceAc3Enc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

using EncoderParam = std::array<std::uint32_t, 6>;

EncoderParam With(EncoderParam param, std::size_t index, std::uint32_t value) {
    param[index] = value;
    return param;
}

int Create(const EncoderParam& param) {
    std::int32_t handle = 0x1234;
    std::int32_t status = 0x5678;
    const int result = sceAc3EncCreateEncoder(&handle, param.data(), &status);
    Require(handle == 0x1234 && status == 0x5678);
    return result;
}

} // namespace

int main() {
    constexpr int invalidHandle = static_cast<int>(0x80E00001u);
    constexpr int notEncoding = static_cast<int>(0x80E00009u);
    constexpr EncoderParam valid{0, 6, 48000, 640000, 1, 0};
    constexpr int invalidPointer = static_cast<int>(0x80E00002u);
    std::int32_t handle = 0x1234;
    Require(sceAc3EncCreateEncoder(nullptr, valid.data(), nullptr) == invalidPointer);
    Require(sceAc3EncCreateEncoder(&handle, nullptr, nullptr) == invalidPointer);
    Require(handle == 0x1234);
    Require(Create(With(valid, 1, 2)) == static_cast<int>(0x80E00003u));
    Require(Create(With(With(valid, 1, 2), 2, 44100)) == static_cast<int>(0x80E00003u));
    Require(Create(With(valid, 2, 44100)) == static_cast<int>(0x80E00004u));
    Require(Create(With(With(valid, 2, 44100), 3, 320000)) == static_cast<int>(0x80E00004u));
    Require(Create(With(valid, 3, 320000)) == static_cast<int>(0x80E00005u));
    Require(Create(With(With(valid, 3, 320000), 4, 2)) == static_cast<int>(0x80E00005u));
    Require(Create(With(valid, 4, 2)) == static_cast<int>(0x80E00006u));
    Require(Create(With(With(valid, 4, 2), 5, 1)) == static_cast<int>(0x80E00006u));
    Require(Create(With(valid, 5, 1)) == static_cast<int>(0x80E00007u));
    for (int call = 0; call < 2; ++call) {
        Require(sceAc3EncDeleteEncoder() == invalidHandle);
        Require(sceAc3EncEncode() == invalidHandle);
        Require(sceAc3EncEncodeAsync() == invalidHandle);
        Require(sceAc3EncFlush() == invalidHandle);
        Require(sceAc3EncWaitEncode() == notEncoding);
        Require(sceAc3EncIsEncoding() == 0);
    }
    return 0;
}
