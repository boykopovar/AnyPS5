#ifndef CORE_LIBS_PRX_LIBCURL_INCLUDE_INVENTORYEXPORTS_HPP
#define CORE_LIBS_PRX_LIBCURL_INCLUDE_INVENTORYEXPORTS_HPP

#include "SceTypes.hpp"
#include "prx/libcurl/include/CurlTypes.hpp"
#include <cstdio>
#include <ctime>

extern "C" {

using guest_curl_lock_callback = void (APS5_VABI*)(CURL*, curl_lock_data, curl_lock_access, void*);
using guest_curl_unlock_callback = void (APS5_VABI*)(CURL*, curl_lock_data, void*);

using guest_curl_formget_callback = size_t (APS5_VABI*)(void* arg, const char* buf, size_t len);
using guest_curl_read_callback = size_t (APS5_VABI*)(char* buffer, size_t size, size_t nitems, void* userdata);
using guest_curl_seek_callback = int (APS5_VABI*)(void* userdata, curl_off_t offset, int origin);
using guest_curl_free_callback = void (APS5_VABI*)(void* ptr);
using guest_curl_malloc_callback = void* (APS5_VABI*)(size_t size);
using guest_curl_realloc_callback = void* (APS5_VABI*)(void* ptr, size_t size);
using guest_curl_strdup_callback = char* (APS5_VABI*)(const char* str);
using guest_curl_calloc_callback = void* (APS5_VABI*)(size_t nmemb, size_t size);

char* APS5_VABI curl_pushheader_bynum_nid_postfix(struct curl_pushheaders* h, size_t num);
CURLFORMcode APS5_VABI curl_formadd_nid_postfix(struct guest_curl_httppost** httppost, struct guest_curl_httppost** last_post, ...);
CURLMcode APS5_VABI curl_multi_socket_action_nid_postfix(CURLM* multi_handle, int s, int ev_bitmask, int* running_handles);
CURLcode APS5_VABI curl_easy_recv_nid_postfix(CURL* curl, void* buffer, size_t buflen, size_t* n);
void APS5_VABI curl_mime_free_nid_postfix(curl_mime* mime);
char* APS5_VABI curl_escape_nid_postfix(const char* string, int length);
char* APS5_VABI curl_easy_escape_nid_postfix(CURL* handle, const char* string, int length);
int APS5_VABI curl_mvfprintf_nid_postfix(std::FILE* whereto, const char* format, VaList* args);
CURLSHcode APS5_VABI curl_share_cleanup_nid_postfix(CURLSH* share_handle);
CURLcode APS5_VABI curl_easy_perform_nid_postfix(CURL* handle);
CURLMcode APS5_VABI curl_multi_remove_handle_nid_postfix(CURLM* multi_handle, CURL* easy_handle);
int APS5_VABI curl_mvsprintf_nid_postfix(char* buffer, const char* format, VaList* args);
CURLsslset APS5_VABI curl_global_sslset_nid_postfix(curl_sslbackend id, const char* name, const guest_curl_ssl_backend*** avail);
CURLMcode APS5_VABI curl_multi_perform_nid_postfix(CURLM* multi_handle, int* running_handles);
char* APS5_VABI curl_version_nid_postfix();
void APS5_VABI curl_free_nid_postfix(void* p);
int APS5_VABI curl_mvsnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, VaList* args);
CURLMcode APS5_VABI curl_multi_socket_all_nid_postfix(CURLM* multi_handle, int* running_handles);
int APS5_VABI curl_strequal();
CURLUcode APS5_VABI curl_url_get_nid_postfix(CURLU* handle, CURLUPart what, char** part, unsigned int flags);
const char* APS5_VABI curl_multi_strerror_nid_postfix(CURLMcode error);
CURLcode APS5_VABI curl_easy_setopt_nid_postfix(CURL* handle, CURLoption option, ...);
CURLcode APS5_VABI curl_mime_data_nid_postfix(curl_mimepart* part, const char* data, size_t datasize);
int APS5_VABI curl_mfprintf_nid_postfix(std::FILE* whereto, const char* format, ...);
CURLMcode APS5_VABI curl_multi_poll_nid_postfix(CURLM* multi_handle, struct guest_curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms, int* ret);
void APS5_VABI curl_easy_cleanup_nid_postfix(CURL* handle);
CURLcode APS5_VABI curl_mime_name_nid_postfix(curl_mimepart* part, const char* name);
CURLcode APS5_VABI curl_global_init_nid_postfix(long long flags);
CURL* APS5_VABI curl_easy_duphandle_nid_postfix(CURL* handle);
CURLSH* APS5_VABI curl_share_init_nid_postfix();
char* APS5_VABI curl_getenv_nid_postfix(const char* variable);
int APS5_VABI curl_formget_nid_postfix(struct guest_curl_httppost* form, void* arg, guest_curl_formget_callback append);
CURLcode APS5_VABI curl_mime_type_nid_postfix(curl_mimepart* part, const char* mimetype);
int APS5_VABI curl_mprintf_nid_postfix(const char* format, ...);
struct guest_curl_slist* APS5_VABI curl_slist_append_nid_postfix(struct guest_curl_slist* list, const char* string);
CURLcode APS5_VABI curl_easy_send_nid_postfix(CURL* curl, const void* buffer, size_t buflen, size_t* n);
void APS5_VABI curl_easy_reset_nid_postfix(CURL* handle);
const char* APS5_VABI curl_easy_strerror_nid_postfix(CURLcode error);
CURL* APS5_VABI curl_easy_init_nid_postfix();
int APS5_VABI curl_strnequal();
CURLcode APS5_VABI curl_mime_data_cb_nid_postfix(curl_mimepart* part, curl_off_t datasize, guest_curl_read_callback readfunc, guest_curl_seek_callback seekfunc, guest_curl_free_callback freefunc, void* arg);
char* APS5_VABI curl_mvaprintf_nid_postfix(const char* format, VaList* args);
CURLU* APS5_VABI curl_url_nid_postfix();
int APS5_VABI curl_msnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, ...);
CURLMcode APS5_VABI curl_multi_socket_nid_postfix(CURLM* multi_handle, int s, int* running_handles);
CURLMcode APS5_VABI curl_multi_fdset_nid_postfix(CURLM* multi_handle, guest_fd_set* read_fd_set, guest_fd_set* write_fd_set, guest_fd_set* exc_fd_set, int* max_fd);
CURLMcode APS5_VABI curl_multi_wakeup_nid_postfix(CURLM* multi_handle);
CURLMcode APS5_VABI curl_multi_assign_nid_postfix(CURLM* multi_handle, int sockfd, void* sockptr);
const char* APS5_VABI curl_share_strerror_nid_postfix(CURLSHcode error);
CURLMcode APS5_VABI curl_multi_wait_nid_postfix(CURLM* multi_handle, struct guest_curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms, int* ret);
CURLUcode APS5_VABI curl_url_set_nid_postfix(CURLU* handle, CURLUPart what, const char* part, unsigned int flags);
const struct guest_curl_easyoption* APS5_VABI curl_easy_option_next_nid_postfix(const struct guest_curl_easyoption* prev);
void APS5_VABI curl_global_cleanup_nid_postfix();
CURLcode APS5_VABI curl_easy_getinfo_nid_postfix(CURL* handle, CURLINFO info, ...);
CURLSHcode APS5_VABI curl_share_setopt_nid_postfix(CURLSH* share_handle, CURLSHoption option, ...);
struct guest_CURLMsg* APS5_VABI curl_multi_info_read_nid_postfix(CURLM* multi_handle, int* msgs_in_queue);
CURLcode APS5_VABI curl_easy_upkeep_nid_postfix(CURL* handle);
char* APS5_VABI curl_unescape_nid_postfix(const char* string, int length);
CURLMcode APS5_VABI curl_multi_setopt_nid_postfix(CURLM* multi_handle, CURLMoption option, ...);
curl_mimepart* APS5_VABI curl_mime_addpart_nid_postfix(curl_mime* mime);
const struct guest_curl_easyoption* APS5_VABI curl_easy_option_by_name_nid_postfix(const char* name);
char* APS5_VABI curl_maprintf_nid_postfix(const char* format, ...);
void APS5_VABI curl_slist_free_all_nid_postfix(struct guest_curl_slist* list);
void APS5_VABI curl_url_cleanup_nid_postfix(CURLU* handle);
std::time_t APS5_VABI curl_getdate_nid_postfix(const char* p, const std::time_t* unused);
CURLcode APS5_VABI curl_mime_filedata_nid_postfix(curl_mimepart* part, const char* filename);
CURLMcode APS5_VABI curl_multi_timeout_nid_postfix(CURLM* multi_handle, int64_t* milliseconds);
char* APS5_VABI curl_easy_unescape_nid_postfix(CURL* handle, const char* string, int inlength, int* outlength);
CURLMcode APS5_VABI curl_multi_add_handle_nid_postfix(CURLM* multi_handle, CURL* easy_handle);
curl_mime* APS5_VABI curl_mime_init_nid_postfix(CURL* easy);
const struct guest_curl_easyoption* APS5_VABI curl_easy_option_by_id_nid_postfix(CURLoption id);
void APS5_VABI curl_formfree_nid_postfix(struct guest_curl_httppost* form);
CURLcode APS5_VABI curl_mime_subparts_nid_postfix(curl_mimepart* part, curl_mime* subparts);
struct guest_curl_version_info_data* APS5_VABI curl_version_info_nid_postfix(CURLversion age);
char* APS5_VABI curl_pushheader_byname_nid_postfix(struct curl_pushheaders* h, const char* name);
CURLcode APS5_VABI curl_mime_encoder_nid_postfix(curl_mimepart* part, const char* encoding);
int APS5_VABI curl_mvprintf_nid_postfix(const char* format, VaList* args);
CURLcode APS5_VABI curl_global_init_mem_nid_postfix(long long flags, guest_curl_malloc_callback m, guest_curl_free_callback f, guest_curl_realloc_callback r, guest_curl_strdup_callback s, guest_curl_calloc_callback c);
CURLcode APS5_VABI curl_mime_headers_nid_postfix(curl_mimepart* part, struct guest_curl_slist* headers, int take_ownership);
CURLU* APS5_VABI curl_url_dup_nid_postfix(CURLU* in);
CURLM* APS5_VABI curl_multi_init_nid_postfix();
CURLMcode APS5_VABI curl_multi_cleanup_nid_postfix(CURLM* multi_handle);
CURLcode APS5_VABI curl_easy_pause_nid_postfix(CURL* handle, int bitmask);
CURLcode APS5_VABI curl_mime_filename_nid_postfix(curl_mimepart* part, const char* filename);
int APS5_VABI curl_msprintf_nid_postfix(char* buffer, const char* format, ...);

}

#endif
