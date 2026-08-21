#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceHttpAbortRequest(int request_id) {
 (void)request_id;
 return 0;
}

int sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
 (void)id;
 (void)name;
 (void)value;
 (void)mode;
 return 0;
}

int sceHttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme, uint16_t port, int enable_keep_alive) {
 (void)tmpl_id;
 (void)server_name;
 (void)scheme;
 (void)port;
 (void)enable_keep_alive;
 return 0;
}

int sceHttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
 (void)tmpl_id;
 (void)url;
 (void)enable_keep_alive;
 return 0;
}

int sceHttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
 (void)http_ctx_id;
 (void)eh;
 return 0;
}

int sceHttpCreateRequest(int conn_id, int method, const char* path, uint64_t content_length) {
 (void)conn_id;
 (void)method;
 (void)path;
 (void)content_length;
 return 0;
}

int sceHttpCreateRequestWithURL2(int conn_id, const char* method, const char* url, uint64_t content_length) {
 (void)conn_id;
 (void)method;
 (void)url;
 (void)content_length;
 return 0;
}

int sceHttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
 (void)http_ctx_id;
 (void)user_agent;
 (void)http_ver;
 (void)is_auto_proxy_conf;
 return 0;
}

int sceHttpDeleteConnection(int conn_id) {
 (void)conn_id;
 return 0;
}

int sceHttpDeleteRequest(int req_id) {
 (void)req_id;
 return 0;
}

int sceHttpDeleteTemplate(int tmpl_id) {
 (void)tmpl_id;
 return 0;
}

int sceHttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
 (void)http_ctx_id;
 (void)eh;
 return 0;
}

int sceHttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
 (void)request_id;
 (void)header;
 (void)header_size;
 return 0;
}

int sceHttpGetResponseContentLength(int request_id, int* result, uint64_t* content_length) {
 (void)request_id;
 (void)result;
 (void)content_length;
 return 0;
}

int sceHttpGetStatusCode(int request_id, int* status_code) {
 (void)request_id;
 (void)status_code;
 return 0;
}

int sceHttpInit_nid_postfix(int memid, int ssl_ctx_id, uint64_t pool_size) {
 (void)memid;
 (void)ssl_ctx_id;
 (void)pool_size;
 return 0;
}

int sceHttpsDisableOption(int id, uint32_t ssl_flags) {
 (void)id;
 (void)ssl_flags;
 return 0;
}

int sceHttpSendRequest(int request_id, const void* post_data, size_t size) {
 (void)request_id;
 (void)post_data;
 (void)size;
 return 0;
}

int sceHttpSetAuthEnabled(int id, int enable) {
 (void)id;
 (void)enable;
 return 0;
}

int sceHttpSetAutoRedirect(int id, int enable) {
 (void)id;
 (void)enable;
 return 0;
}

int sceHttpSetConnectTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 return 0;
}

int sceHttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
 (void)id;
 (void)eh;
 (void)user_arg;
 return 0;
}

int sceHttpSetNonblock(int id, int enable) {
 (void)id;
 (void)enable;
 return 0;
}

int sceHttpSetRecvTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 return 0;
}

int sceHttpSetRequestContentLength(int request_id, uint64_t content_length) {
 (void)request_id;
 (void)content_length;
 return 0;
}

int sceHttpSetResolveRetry(int id, int32_t retry) {
 (void)id;
 (void)retry;
 return 0;
}

int sceHttpSetResolveTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 return 0;
}

int sceHttpSetSendTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 return 0;
}

int sceHttpsSetMinSslVersion(int id, uint32_t ssl_version) {
 (void)id;
 (void)ssl_version;
 return 0;
}

int sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) {
 (void)id;
 (void)cbfunc;
 (void)user_arg;
 return 0;
}

int sceHttpTerm_nid_postfix(int http_ctx_id) {
 (void)http_ctx_id;
 return 0;
}

int sceHttpUnsetEpoll(int id) {
 (void)id;
 return 0;
}

int sceHttpUriBuild(char* out, size_t* require, size_t prepare, const SceHttpUriElement* src_element, uint32_t option) {
 (void)out;
 (void)require;
 (void)prepare;
 (void)src_element;
 (void)option;
 return 0;
}

int sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in) {
 (void)out;
 (void)require;
 (void)prepare;
 (void)in;
 return 0;
}

int sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare) {
 (void)out;
 (void)src_url;
 (void)pool;
 (void)require;
 (void)prepare;
 return 0;
}

int sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout) {
 (void)eh;
 (void)nbev;
 (void)maxevents;
 (void)timeout;
 return 0;
}

}
