#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uprv_itou_nid_postfix), std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, std::uint32_t, std::uint32_t, std::int32_t)>);

constexpr char16_t Filler = 0xa5a5;

void Check(std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth, std::int32_t capacity, std::u16string_view expected, bool terminated) {
    std::array<char16_t, 48> buffer;
    buffer.fill(Filler);
    const std::int32_t length = uprv_itou_nid_postfix(buffer.data(), capacity, value, radix, minimumWidth);
    Require(length == static_cast<std::int32_t>(expected.size()));
    Require(std::u16string_view(buffer.data(), expected.size()) == expected);
    Require(buffer[expected.size()] == (terminated ? char16_t{0} : Filler));
    for (std::size_t index = expected.size() + 1; index < buffer.size(); ++index) Require(buffer[index] == Filler);
}

}

int main() {
    Check(0, 10, 0, 16, u"0", true);
    Check(255, 16, 0, 16, u"FF", true);
    Check(255, 10, 0, 16, u"255", true);
    Check(7, 10, 4, 16, u"0007", true);
    Check(35, 36, 0, 16, u"Z", true);
    Check(36, 36, 0, 16, u"10", true);
    Check(5, 1, 0, 3, u"000", false);
    Check(0xffffffffu, 2, 0, 40, u"11111111111111111111111111111111", true);
    Check(0xffffffffu, 10, 0, 40, u"4294967295", true);
    Check(12345, 10, 0, 2, u"45", false);
    Check(12345, 10, 0, 5, u"12345", false);
    Check(12345, 10, 0, 0, u"5", false);
    Check(12345, 10, 0, -3, u"5", false);
    Check(9, 10, 6, 2, u"000009", false);
    Check(45, 10, -2, 16, u"45", true);
    Check(40, 41, 0, 16, u"_", true);
    std::array<char16_t, 8> buffer{};
    RequireThrows([&] { uprv_itou_nid_postfix(buffer.data(), 8, 1, 0, 0); });
    RequireThrows([] { uprv_itou_nid_postfix(nullptr, 8, 1, 10, 0); });
    return 0;
}
