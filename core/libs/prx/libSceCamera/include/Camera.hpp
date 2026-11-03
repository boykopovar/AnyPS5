#pragma once

#include <cstdint>

inline constexpr int SCE_CAMERA_ERROR_PARAM = static_cast<int>(0x802E0000);
inline constexpr int SCE_CAMERA_ERROR_NOT_INIT = static_cast<int>(0x802E0002);
inline constexpr int SCE_CAMERA_ERROR_NOT_OPEN = static_cast<int>(0x802E0004);
inline constexpr int SCE_CAMERA_ERROR_FORMAT_UNKNOWN = static_cast<int>(0x802E0007);
inline constexpr int SCE_CAMERA_ERROR_NOT_CONNECTED = static_cast<int>(0x802E0010);
inline constexpr int SCE_CAMERA_ERROR_FATAL = static_cast<int>(0x802E00FF);

namespace Camera {

bool IsOpen();
bool IsOpen(std::int32_t handle);

}
