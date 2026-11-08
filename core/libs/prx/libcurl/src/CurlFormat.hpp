#ifndef CORE_LIBS_PRX_LIBCURL_SRC_CURLFORMAT_HPP
#define CORE_LIBS_PRX_LIBCURL_SRC_CURLFORMAT_HPP

#include <cstddef>

namespace CurlFormat {

int FormatBounded(char* buffer, std::size_t maxLength, const char* format, const void* args);
int FormatUnbounded(char* buffer, const char* format, const void* args);
char* FormatAllocated(const char* format, const void* args);

}

#endif
