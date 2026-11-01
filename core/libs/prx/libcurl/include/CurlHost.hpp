#ifndef CORE_LIBS_PRX_LIBCURL_INCLUDE_CURLHOST_HPP
#define CORE_LIBS_PRX_LIBCURL_INCLUDE_CURLHOST_HPP

#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#define CURL_ALLOW_OLD_MULTI_SOCKET
#define curl_strequal host_curl_strequal
#define curl_strnequal host_curl_strnequal
#include <curl/curl.h>
#include <curl/mprintf.h>
#undef curl_strequal
#undef curl_strnequal
#undef curl_easy_setopt
#undef curl_easy_getinfo
#undef curl_share_setopt
#undef curl_multi_setopt

#endif
