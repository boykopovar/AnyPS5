#include "prx/libSceIcu/TimeZoneOffset.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cstdint>
#include <stdexcept>

extern "C" std::int32_t APS5_VABI uprv_timezone_nid_postfix() {
    const std::int64_t now = libc_time_nid_postfix(nullptr);
    GuestTm local{};
    GuestTm universal{};
    if (!localtime_r_nid_postfix(&now, &local) || !gmtime_r_nid_postfix(&now, &universal))
        throw std::runtime_error("uprv_timezone: guest time conversion failed");
    const bool daylight = local.tm_isdst != 0;
    const std::int64_t localSeconds = libc_mktime_nid_postfix(&local);
    const std::int64_t universalSeconds = libc_mktime_nid_postfix(&universal);
    return static_cast<std::int32_t>(universalSeconds - localSeconds + (daylight ? 3600 : 0));
}
