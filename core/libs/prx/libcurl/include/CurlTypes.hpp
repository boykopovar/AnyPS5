#ifndef CORE_LIBS_PRX_LIBCURL_INCLUDE_CURLTYPES_HPP
#define CORE_LIBS_PRX_LIBCURL_INCLUDE_CURLTYPES_HPP

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <ctime>
#include "CurlHost.hpp"

struct guest_curl_waitfd {
    int fd;
    short events;
    short revents;
};

static_assert(sizeof(guest_curl_waitfd) == 8);
static_assert(offsetof(guest_curl_waitfd, fd) == 0);
static_assert(offsetof(guest_curl_waitfd, events) == 4);
static_assert(offsetof(guest_curl_waitfd, revents) == 6);

struct guest_CURLMsg {
    CURLMSG msg;
    CURL* easy_handle;
    union {
        void* whatever;
        CURLcode result;
    } data;
};

static_assert(sizeof(guest_CURLMsg) == 24);
static_assert(offsetof(guest_CURLMsg, msg) == 0);
static_assert(offsetof(guest_CURLMsg, easy_handle) == 8);
static_assert(offsetof(guest_CURLMsg, data) == 16);

struct guest_curl_version_info_data {
    CURLversion age;
    const char* version;
    unsigned int version_num;
    const char* host;
    int features;
    const char* ssl_version;
    int64_t ssl_version_num;
    const char* libz_version;
    const char* const* protocols;
    const char* ares;
    int ares_num;
    const char* libidn;
    int iconv_ver_num;
    const char* libssh_version;
    unsigned int brotli_ver_num;
    const char* brotli_version;
    unsigned int nghttp2_ver_num;
    const char* nghttp2_version;
    const char* quic_version;
    const char* cainfo;
    const char* capath;
    unsigned int zstd_ver_num;
    const char* zstd_version;
    const char* hyper_version;
    const char* gsasl_version;
};

static_assert(sizeof(guest_curl_version_info_data) == 200);
static_assert(offsetof(guest_curl_version_info_data, age) == 0);
static_assert(offsetof(guest_curl_version_info_data, version) == 8);
static_assert(offsetof(guest_curl_version_info_data, version_num) == 16);
static_assert(offsetof(guest_curl_version_info_data, host) == 24);
static_assert(offsetof(guest_curl_version_info_data, features) == 32);
static_assert(offsetof(guest_curl_version_info_data, ssl_version) == 40);
static_assert(offsetof(guest_curl_version_info_data, ssl_version_num) == 48);
static_assert(offsetof(guest_curl_version_info_data, libz_version) == 56);
static_assert(offsetof(guest_curl_version_info_data, protocols) == 64);
static_assert(offsetof(guest_curl_version_info_data, ares) == 72);
static_assert(offsetof(guest_curl_version_info_data, ares_num) == 80);
static_assert(offsetof(guest_curl_version_info_data, libidn) == 88);
static_assert(offsetof(guest_curl_version_info_data, iconv_ver_num) == 96);
static_assert(offsetof(guest_curl_version_info_data, libssh_version) == 104);
static_assert(offsetof(guest_curl_version_info_data, brotli_ver_num) == 112);
static_assert(offsetof(guest_curl_version_info_data, brotli_version) == 120);
static_assert(offsetof(guest_curl_version_info_data, nghttp2_ver_num) == 128);
static_assert(offsetof(guest_curl_version_info_data, nghttp2_version) == 136);
static_assert(offsetof(guest_curl_version_info_data, quic_version) == 144);
static_assert(offsetof(guest_curl_version_info_data, cainfo) == 152);
static_assert(offsetof(guest_curl_version_info_data, capath) == 160);
static_assert(offsetof(guest_curl_version_info_data, zstd_ver_num) == 168);
static_assert(offsetof(guest_curl_version_info_data, zstd_version) == 176);

struct guest_curl_easyoption {
    const char* name;
    CURLoption id;
    curl_easytype type;
    unsigned int flags;
};

static_assert(sizeof(guest_curl_easyoption) == 24);
static_assert(offsetof(guest_curl_easyoption, name) == 0);
static_assert(offsetof(guest_curl_easyoption, id) == 8);
static_assert(offsetof(guest_curl_easyoption, type) == 12);
static_assert(offsetof(guest_curl_easyoption, flags) == 16);

struct guest_curl_blob {
    void* data;
    size_t len;
    unsigned int flags;
};

static_assert(sizeof(guest_curl_blob) == 24);
static_assert(offsetof(guest_curl_blob, data) == 0);
static_assert(offsetof(guest_curl_blob, len) == 8);
static_assert(offsetof(guest_curl_blob, flags) == 16);

struct guest_curl_slist {
    char* data;
    struct guest_curl_slist* next;
};

static_assert(sizeof(guest_curl_slist) == 16);
static_assert(offsetof(guest_curl_slist, data) == 0);
static_assert(offsetof(guest_curl_slist, next) == 8);

struct guest_curl_httppost {
    struct guest_curl_httppost* next;
    char* name;
    int64_t namelength;
    char* contents;
    int64_t contentslength;
    char* buffer;
    int64_t bufferlength;
    char* contenttype;
    struct guest_curl_slist* contentheader;
    struct guest_curl_httppost* more;
    int64_t flags;
    char* showfilename;
    void* userp;
    int64_t contentlen;
};

static_assert(sizeof(guest_curl_httppost) == 112);
static_assert(offsetof(guest_curl_httppost, next) == 0);
static_assert(offsetof(guest_curl_httppost, name) == 8);
static_assert(offsetof(guest_curl_httppost, namelength) == 16);
static_assert(offsetof(guest_curl_httppost, contents) == 24);
static_assert(offsetof(guest_curl_httppost, contentslength) == 32);
static_assert(offsetof(guest_curl_httppost, buffer) == 40);
static_assert(offsetof(guest_curl_httppost, bufferlength) == 48);
static_assert(offsetof(guest_curl_httppost, contenttype) == 56);
static_assert(offsetof(guest_curl_httppost, contentheader) == 64);
static_assert(offsetof(guest_curl_httppost, more) == 72);
static_assert(offsetof(guest_curl_httppost, flags) == 80);
static_assert(offsetof(guest_curl_httppost, showfilename) == 88);
static_assert(offsetof(guest_curl_httppost, userp) == 96);
static_assert(offsetof(guest_curl_httppost, contentlen) == 104);

struct guest_curl_forms {
    CURLformoption option;
    const char* value;
};

static_assert(sizeof(guest_curl_forms) == 16);
static_assert(offsetof(guest_curl_forms, option) == 0);
static_assert(offsetof(guest_curl_forms, value) == 8);

struct guest_curl_sockaddr {
    int family;
    int socktype;
    int protocol;
    unsigned int addrlen;
    struct {
        std::uint8_t length;
        std::uint8_t family;
        char data[14];
    } addr;
};

static_assert(sizeof(guest_curl_sockaddr) == 32);
static_assert(offsetof(guest_curl_sockaddr, family) == 0);
static_assert(offsetof(guest_curl_sockaddr, socktype) == 4);
static_assert(offsetof(guest_curl_sockaddr, protocol) == 8);
static_assert(offsetof(guest_curl_sockaddr, addrlen) == 12);
static_assert(offsetof(guest_curl_sockaddr, addr) == 16);

struct guest_fd_set {
    std::uint64_t fds_bits[1024 / 64];
};

static_assert(sizeof(guest_fd_set) == 128);

static_assert(offsetof(guest_curl_version_info_data, hyper_version) == 184);
static_assert(offsetof(guest_curl_version_info_data, gsasl_version) == 192);
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
static_assert(sizeof(curl_off_t) == 8);
static_assert(sizeof(std::time_t) == 8);
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

struct guest_curl_ssl_backend {
    curl_sslbackend id;
    const char* name;
};

static_assert(sizeof(guest_curl_ssl_backend) == 16);
static_assert(offsetof(guest_curl_ssl_backend, id) == 0);
static_assert(offsetof(guest_curl_ssl_backend, name) == 8);
static_assert(sizeof(curl_ssl_backend) == 16);
static_assert(offsetof(curl_ssl_backend, id) == 0);
static_assert(offsetof(curl_ssl_backend, name) == 8);

#endif
