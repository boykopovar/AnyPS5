#include "prx/libSceAvSetting/include/Api.hpp"
#include "prx/libSceAvSetting/include/InventoryExports.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
    std::array<std::uint8_t, 48> bytes;
    bytes.fill(0xa5);
    sceAvControlSetVideoOutSource(reinterpret_cast<std::int32_t*>(bytes.data() + 8), 0x12345678);
    auto word = [&](std::size_t offset) {
        std::int32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    if (word(8) != 0x12345678) std::abort();
    if (word(4) != static_cast<std::int32_t>(0xa5a5a5a5u) || word(12) != static_cast<std::int32_t>(0xa5a5a5a5u)) std::abort();
    sceAvControlSetVideoOutSource(nullptr, 1);
    if (word(8) != 0x12345678) std::abort();
    bytes.fill(0xa5);
    libSceAvSettingUnknown_A143jTEYkNw_nid_no_patch(bytes.data());
    if (word(0) != 0 || word(4) != 0 || word(8) != static_cast<std::int32_t>(0xa5a5a5a5u)) std::abort();
    bytes.fill(0xa5);
    libSceAvSettingUnknown_mp2s52Yr4pc_nid_no_patch(bytes.data() + 4);
    if (word(0) != static_cast<std::int32_t>(0xa5a5a5a5u) || word(4) != 0 || word(8) != 0) std::abort();
    bytes.fill(0xa5);
    libSceAvSettingUnknown_cK_plus_dKi9GYYA_nid_no_patch(bytes.data() + 16);
    if (word(12) != static_cast<std::int32_t>(0xa5a5a5a5u) || word(16) != 0 || word(20) != 0 || word(24) != 0 || word(28) != 0) std::abort();
    if (word(32) != static_cast<std::int32_t>(0xa5a5a5a5u)) std::abort();
    libSceAvSettingUnknown_A143jTEYkNw_nid_no_patch(nullptr);
    libSceAvSettingUnknown_mp2s52Yr4pc_nid_no_patch(nullptr);
    libSceAvSettingUnknown_cK_plus_dKi9GYYA_nid_no_patch(nullptr);
    return 0;
}
