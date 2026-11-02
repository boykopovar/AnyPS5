#ifndef CORE_LIBS_PRX_LIBSCENOTIFICATION_NOTIFICATION_HPP
#define CORE_LIBS_PRX_LIBSCENOTIFICATION_NOTIFICATION_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Notification {

inline constexpr std::int32_t InvalidMessage = static_cast<std::int32_t>(0x81980001u);

inline constexpr std::size_t IdCapacity = 0x20;
inline constexpr std::size_t MessageCapacity = 0x1800;

}

extern "C" {
int APS5_VABI sceNotificationSend(int type, std::uint8_t flag, std::uint8_t* message);
int APS5_VABI sceNotificationSendById(int type, std::uint8_t flag, std::uint8_t* id, std::uint8_t* jsonData);
int APS5_VABI sceNotificationShowPsButtonPersistentBanner(std::uint8_t* jsonParam);
void APS5_VABI sceNotificationHidePsButtonPersistentBanner();
}

#endif
