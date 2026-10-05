#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_HTTPERRORS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_HTTPERRORS_HPP

inline constexpr int ERROR_OUT_OF_MEMORY = static_cast<int>(0x80431022);
inline constexpr int ERROR_NETWORK = static_cast<int>(0x80431063);
inline constexpr int ERROR_INVALID_VALUE = static_cast<int>(0x804311FE);
inline constexpr int ERROR_INVALID_URL = static_cast<int>(0x80433060);
inline constexpr int ERROR_PARSE_HTTP_INVALID_RESPONSE = static_cast<int>(0x80432060);
inline constexpr int ERROR_PARSE_HTTP_INVALID_VALUE = static_cast<int>(0x804321FE);

#endif
