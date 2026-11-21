#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_COOKIES_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_COOKIES_HPP

#include <cstddef>

void ClearHttpCookieJar(int contextId);
void RemoveHttpCookieJar(int contextId);
int ExportHttpCookies(int contextId, void* buffer, std::size_t size, std::size_t* requiredSize);
int ImportHttpCookies(int contextId, const void* buffer, std::size_t size);

#endif
