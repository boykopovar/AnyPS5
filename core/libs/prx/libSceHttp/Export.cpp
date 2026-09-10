#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceHttpAbortRequest(int request_id) {
 (void)request_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
 (void)id;
 (void)name;
 (void)value;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme, uint16_t port, int enable_keep_alive) {
 (void)tmpl_id;
 (void)server_name;
 (void)scheme;
 (void)port;
 (void)enable_keep_alive;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
 (void)tmpl_id;
 (void)url;
 (void)enable_keep_alive;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
 (void)http_ctx_id;
 (void)eh;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateRequest(int conn_id, int method, const char* path, uint64_t content_length) {
 (void)conn_id;
 (void)method;
 (void)path;
 (void)content_length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateRequestWithURL2(int conn_id, const char* method, const char* url, uint64_t content_length) {
 (void)conn_id;
 (void)method;
 (void)url;
 (void)content_length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
 (void)http_ctx_id;
 (void)user_agent;
 (void)http_ver;
 (void)is_auto_proxy_conf;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpDeleteConnection(int conn_id) {
 (void)conn_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpDeleteRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpDeleteTemplate(int tmpl_id) {
 (void)tmpl_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
 (void)http_ctx_id;
 (void)eh;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
 (void)request_id;
 (void)header;
 (void)header_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpGetResponseContentLength(int request_id, int* result, uint64_t* content_length) {
 (void)request_id;
 (void)result;
 (void)content_length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpGetStatusCode(int request_id, int* status_code) {
 (void)request_id;
 (void)status_code;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpInit_nid_postfix(int memid, int ssl_ctx_id, uint64_t pool_size) {
 (void)memid;
 (void)ssl_ctx_id;
 (void)pool_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpsDisableOption(int id, uint32_t ssl_flags) {
 (void)id;
 (void)ssl_flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSendRequest(int request_id, const void* post_data, size_t size) {
 (void)request_id;
 (void)post_data;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetAuthEnabled(int id, int enable) {
 (void)id;
 (void)enable;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetAutoRedirect(int id, int enable) {
 (void)id;
 (void)enable;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetConnectTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
 (void)id;
 (void)eh;
 (void)user_arg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetNonblock(int id, int enable) {
 (void)id;
 (void)enable;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetRecvTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetRequestContentLength(int request_id, uint64_t content_length) {
 (void)request_id;
 (void)content_length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetResolveRetry(int id, int32_t retry) {
 (void)id;
 (void)retry;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetResolveTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpSetSendTimeOut(int id, uint32_t usec) {
 (void)id;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpsSetMinSslVersion(int id, uint32_t ssl_version) {
 (void)id;
 (void)ssl_version;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) {
 (void)id;
 (void)cbfunc;
 (void)user_arg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpTerm_nid_postfix(int http_ctx_id) {
 (void)http_ctx_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpUnsetEpoll(int id) {
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpUriBuild(char* out, size_t* require, size_t prepare, const SceHttpUriElement* src_element, uint32_t option) {
 (void)out;
 (void)require;
 (void)prepare;
 (void)src_element;
 (void)option;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in) {
 (void)out;
 (void)require;
 (void)prepare;
 (void)in;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare) {
 (void)out;
 (void)src_url;
 (void)pool;
 (void)require;
 (void)prepare;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout) {
 (void)eh;
 (void)nbev;
 (void)maxevents;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
