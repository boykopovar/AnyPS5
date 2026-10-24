#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceConvertKeycodeGetImeKeyboardType(std::int32_t, std::uint32_t*);
}

int main() {
#ifdef _WIN32
    if (_putenv_s("ANYPS5_LANGUAGE", "") != 0) return 2;
#else
    if (::unsetenv("ANYPS5_LANGUAGE") != 0) return 2;
#endif
    static std::array<std::byte, 64> process{};
    constexpr std::uint32_t sdkVersion = 0x09000000u;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    if (!ProcessParametersFixture::Install(process.data(), process.size())) return 2;
    std::uint32_t type = 0xFFFFFFFFu;
    return sceConvertKeycodeGetImeKeyboardType(0x10000000, &type) == 0 && type == 4 ? 0 : 1;
}
