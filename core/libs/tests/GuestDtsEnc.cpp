#include "prx/libSceDtsEnc/libSceDtsEnc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

using EncoderParam = std::array<std::uint32_t, 5>;

EncoderParam With(EncoderParam param, std::size_t index, std::uint32_t value) {
    param[index] = value;
    return param;
}

int Create(const EncoderParam& param) {
    std::int32_t handle = 0x1234;
    std::int32_t status = 0x5678;
    const int result = sceDtsEncCreateEncoder(&handle, param.data(), &status);
    Require(handle == 0x1234 && status == 0x5678);
    return result;
}

} // namespace

int main() {
    constexpr int invalidHandle = static_cast<int>(0x80FA0001u);
    constexpr int notEncoding = static_cast<int>(0x80FA0009u);
    constexpr EncoderParam valid{0, 6, 48000, 1, 0};
    constexpr int invalidPointer = static_cast<int>(0x80FA0002u);
    std::int32_t handle = 0x1234;
    Require(sceDtsEncCreateEncoder(nullptr, valid.data(), nullptr) == invalidPointer);
    Require(sceDtsEncCreateEncoder(&handle, nullptr, nullptr) == invalidPointer);
    Require(handle == 0x1234);
    Require(Create(With(valid, 1, 2)) == static_cast<int>(0x80FA0003u));
    Require(Create(With(With(valid, 1, 2), 2, 44100)) == static_cast<int>(0x80FA0003u));
    Require(Create(With(valid, 2, 44100)) == static_cast<int>(0x80FA0004u));
    Require(Create(With(With(valid, 2, 44100), 3, 2)) == static_cast<int>(0x80FA0004u));
    Require(Create(With(valid, 3, 2)) == static_cast<int>(0x80FA0006u));
    Require(Create(With(With(valid, 3, 2), 4, 1)) == static_cast<int>(0x80FA0006u));
    Require(Create(With(valid, 4, 1)) == static_cast<int>(0x80FA0007u));
    for (int call = 0; call < 2; ++call) {
        Require(sceDtsEncDeleteEncoder() == invalidHandle);
        Require(sceDtsEncEncode() == invalidHandle);
        Require(sceDtsEncEncodeAsync() == invalidHandle);
        Require(sceDtsEncFlush() == invalidHandle);
        Require(sceDtsEncWaitEncode() == notEncoding);
        Require(sceDtsEncIsEncoding() == 0);
    }
    return 0;
}
