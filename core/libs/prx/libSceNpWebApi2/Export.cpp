#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpWebApi2AbortRequest(int64_t request_id) {
 (void)request_id;
 return 0;
}

int sceNpWebApi2AddHttpRequestHeader(int64_t request_id, const char* field_name, const char* value) {
 (void)request_id;
 (void)field_name;
 (void)value;
 return 0;
}

void sceNpWebApi2CheckTimeout(void) {
}

int sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, int64_t* request_id) {
 (void)user_context_id;
 (void)api_group;
 (void)path;
 (void)method;
 (void)content_parameter;
 (void)request_id;
 return 0;
}

int sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
 (void)lib_ctx_id;
 (void)user_id;
 return 0;
}

int sceNpWebApi2DeleteRequest(int64_t request_id) {
 (void)request_id;
 return 0;
}

int sceNpWebApi2DeleteUserContext(int user_context_id) {
 (void)user_context_id;
 return 0;
}

int sceNpWebApi2GetHttpResponseHeaderValue(int64_t request_id, const char* field_name, char* value, size_t value_size) {
 (void)request_id;
 (void)field_name;
 (void)value;
 (void)value_size;
 return 0;
}

int sceNpWebApi2GetHttpResponseHeaderValueLength(int64_t request_id, const char* field_name, size_t* value_length) {
 (void)request_id;
 (void)field_name;
 (void)value_length;
 return 0;
}

int sceNpWebApi2Initialize(int lib_http_ctx_id, size_t pool_size) {
 (void)lib_http_ctx_id;
 (void)pool_size;
 return 0;
}

int sceNpWebApi2PushEventCreateFilter(int lib_ctx_id, int handle_id, const char* np_service_name, uint32_t np_service_label, const void* filter_param, size_t filter_param_num) {
 (void)lib_ctx_id;
 (void)handle_id;
 (void)np_service_name;
 (void)np_service_label;
 (void)filter_param;
 (void)filter_param_num;
 return 0;
}

int sceNpWebApi2PushEventCreateHandle(int lib_ctx_id) {
 (void)lib_ctx_id;
 return 0;
}

int sceNpWebApi2PushEventDeleteHandle(int lib_ctx_id, int handle_id) {
 (void)lib_ctx_id;
 (void)handle_id;
 return 0;
}

int sceNpWebApi2PushEventDeletePushContext(int user_context_id, const void* push_context_id) {
 (void)user_context_id;
 (void)push_context_id;
 return 0;
}

int sceNpWebApi2PushEventRegisterCallback(int user_context_id, int filter_id, void* callback, void* user_arg) {
 (void)user_context_id;
 (void)filter_id;
 (void)callback;
 (void)user_arg;
 return 0;
}

int sceNpWebApi2ReadData(int64_t request_id, void* data, size_t size) {
 (void)request_id;
 (void)data;
 (void)size;
 return 0;
}

int sceNpWebApi2SendRequest(int64_t request_id, const void* data, size_t data_size, NpWebApi2ResponseInformationOption* response_info_option) {
 (void)request_id;
 (void)data;
 (void)data_size;
 (void)response_info_option;
 return 0;
}

int sceNpWebApi2Terminate(int lib_ctx_id) {
 (void)lib_ctx_id;
 return 0;
}

}
