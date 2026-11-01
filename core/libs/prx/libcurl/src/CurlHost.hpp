#ifndef CORE_LIBS_PRX_LIBCURL_SRC_CURLHOST_HPP
#define CORE_LIBS_PRX_LIBCURL_SRC_CURLHOST_HPP

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

#include "prx/libcurl/include/CurlTypes.hpp"

static_assert(sizeof(curl_blob) == sizeof(guest_curl_blob));
static_assert(offsetof(curl_blob, data) == offsetof(guest_curl_blob, data));
static_assert(offsetof(curl_blob, len) == offsetof(guest_curl_blob, len));
static_assert(offsetof(curl_blob, flags) == offsetof(guest_curl_blob, flags));
static_assert(sizeof(curl_khkey) == 24);
static_assert(offsetof(curl_khkey, key) == 0);
static_assert(offsetof(curl_khkey, len) == 8);
static_assert(offsetof(curl_khkey, keytype) == 16);
static_assert(sizeof(curl_index) == 16);
static_assert(offsetof(curl_index, index) == 0);
static_assert(offsetof(curl_index, total) == 8);
static_assert(sizeof(curl_tlssessioninfo) == 16);
static_assert(offsetof(curl_tlssessioninfo, backend) == 0);
static_assert(offsetof(curl_tlssessioninfo, internals) == 8);
static_assert(sizeof(curl_easyoption) == sizeof(guest_curl_easyoption));
static_assert(offsetof(curl_easyoption, name) == 0);
static_assert(offsetof(curl_easyoption, id) == 8);
static_assert(offsetof(curl_easyoption, type) == 12);
static_assert(offsetof(curl_easyoption, flags) == 16);
static_assert(sizeof(curl_slist) == sizeof(guest_curl_slist));
static_assert(offsetof(curl_slist, data) == 0);
static_assert(offsetof(curl_slist, next) == 8);
static_assert(sizeof(CURLMsg) == sizeof(guest_CURLMsg));
static_assert(offsetof(CURLMsg, msg) == 0);
static_assert(offsetof(CURLMsg, easy_handle) == 8);
static_assert(offsetof(CURLMsg, data) == 16);
static_assert(sizeof(curl_ssl_backend) == 16);
static_assert(offsetof(curl_ssl_backend, id) == 0);
static_assert(offsetof(curl_ssl_backend, name) == 8);

#endif
