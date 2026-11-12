#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TValue, typename TBits>
static TValue FromBits(TBits bits) {
    TValue value{};
    static_assert(sizeof(TValue) == sizeof(TBits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int main() {
    constexpr short Negative = static_cast<short>(0x8000);
    const float floatInf = std::numeric_limits<float>::infinity();
    const float floatNaN = std::numeric_limits<float>::quiet_NaN();
    Require(_FDsign_nid_postfix(1.0f) == 0);
    Require(_FDsign_nid_postfix(0.0f) == 0);
    Require(_FDsign_nid_postfix(std::numeric_limits<float>::denorm_min()) == 0);
    Require(_FDsign_nid_postfix(std::numeric_limits<float>::max()) == 0);
    Require(_FDsign_nid_postfix(floatInf) == 0);
    Require(_FDsign_nid_postfix(FromBits<float>(std::uint32_t{0x7fc00000u})) == 0);
    Require(_FDsign_nid_postfix(FromBits<float>(std::uint32_t{0x7f800001u})) == 0);
    Require(_FDsign_nid_postfix(-1.0f) == Negative);
    Require(_FDsign_nid_postfix(-0.0f) == Negative);
    Require(_FDsign_nid_postfix(-std::numeric_limits<float>::denorm_min()) == Negative);
    Require(_FDsign_nid_postfix(std::numeric_limits<float>::lowest()) == Negative);
    Require(_FDsign_nid_postfix(-floatInf) == Negative);
    Require(_FDsign_nid_postfix(-floatNaN) == Negative);
    Require(_FDsign_nid_postfix(FromBits<float>(std::uint32_t{0xffc00000u})) == Negative);
    Require(_FDsign_nid_postfix(FromBits<float>(std::uint32_t{0xff800001u})) == Negative);

    const double doubleInf = std::numeric_limits<double>::infinity();
    const double doubleNaN = std::numeric_limits<double>::quiet_NaN();
    Require(_Dsign_nid_postfix(1.0) == 0);
    Require(_Dsign_nid_postfix(0.0) == 0);
    Require(_Dsign_nid_postfix(std::numeric_limits<double>::denorm_min()) == 0);
    Require(_Dsign_nid_postfix(std::numeric_limits<double>::max()) == 0);
    Require(_Dsign_nid_postfix(doubleInf) == 0);
    Require(_Dsign_nid_postfix(FromBits<double>(std::uint64_t{0x7ff8000000000000ull})) == 0);
    Require(_Dsign_nid_postfix(FromBits<double>(std::uint64_t{0x7ff0000000000001ull})) == 0);
    Require(_Dsign_nid_postfix(-1.0) == Negative);
    Require(_Dsign_nid_postfix(-0.0) == Negative);
    Require(_Dsign_nid_postfix(-std::numeric_limits<double>::denorm_min()) == Negative);
    Require(_Dsign_nid_postfix(std::numeric_limits<double>::lowest()) == Negative);
    Require(_Dsign_nid_postfix(-doubleInf) == Negative);
    Require(_Dsign_nid_postfix(-doubleNaN) == Negative);
    Require(_Dsign_nid_postfix(FromBits<double>(std::uint64_t{0xfff8000000000000ull})) == Negative);
    Require(_Dsign_nid_postfix(FromBits<double>(std::uint64_t{0xfff0000000000001ull})) == Negative);
    return 0;
}
