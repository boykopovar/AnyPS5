#include "prx/libcurl/include/InventoryExports.hpp"
#include "prx/libcurl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cstring>
#include <limits>

extern "C" {

int APS5_VABI curl_pushheader_bynum() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_formadd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_socket_action() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_recv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_free() {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_escape(const char* string, int length) {
    return curl_easy_escape(nullptr, string, length);
}

char* APS5_VABI curl_easy_escape(void* handle, const char* string, int length) {
    (void)handle;
    if (length < 0) return nullptr;
    if (string == nullptr) NotImplemented_nid_no_patch(__func__);
    const auto size = length == 0 ? std::strlen(string) : static_cast<std::size_t>(length);
    if (size > (std::numeric_limits<std::size_t>::max() - 1) / 3) return nullptr;
    auto* output = static_cast<char*>(malloc_nid_postfix(size * 3 + 1));
    if (output == nullptr) return nullptr;
    constexpr char hex[] = "0123456789ABCDEF";
    auto* destination = output;
    for (std::size_t index = 0; index < size; ++index) {
        const auto byte = static_cast<unsigned char>(string[index]);
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' || byte == '_' || byte == '~') {
            *destination++ = static_cast<char>(byte);
        } else {
            *destination++ = '%';
            *destination++ = hex[byte >> 4];
            *destination++ = hex[byte & 15];
        }
    }
    *destination = '\0';
    return output;
}

int APS5_VABI curl_mvfprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_share_cleanup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_perform() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_remove_handle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvsprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_global_sslset() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_perform() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_version() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI curl_free(void* pointer) {
    free_nid_postfix(pointer);
}

int APS5_VABI curl_mvsnprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_socket_all() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_strequal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_url_get() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_strerror() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_setopt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_data() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mfprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_poll() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_cleanup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_name() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_global_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_duphandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_share_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_getenv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_formget() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_type() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mprintf() {
    NotImplemented_nid_no_patch(__func__);
}

CurlSlist* APS5_VABI curl_slist_append(CurlSlist* list, const char* string) {
    if (string == nullptr) NotImplemented_nid_no_patch(__func__);
    const auto size = std::strlen(string);
    if (size == std::numeric_limits<std::size_t>::max()) return nullptr;
    auto* copy = static_cast<char*>(malloc_nid_postfix(size + 1));
    if (copy == nullptr) return nullptr;
    std::memcpy(copy, string, size + 1);
    auto* node = static_cast<CurlSlist*>(malloc_nid_postfix(sizeof(CurlSlist)));
    if (node == nullptr) {
        free_nid_postfix(copy);
        return nullptr;
    }
    node->data = copy;
    node->next = nullptr;
    if (list == nullptr) return node;
    auto* tail = list;
    while (tail->next != nullptr) tail = tail->next;
    tail->next = node;
    return list;
}

int APS5_VABI curl_easy_send() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_reset() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_strerror() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_strnequal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_data_cb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvaprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_url() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_msnprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_socket() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_fdset() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_wakeup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_assign() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_share_strerror() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_wait() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_url_set() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_option_next() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_global_cleanup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_getinfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_share_setopt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_info_read() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_upkeep() {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_unescape(const char* string, int length) {
    return curl_easy_unescape(nullptr, string, length, nullptr);
}

int APS5_VABI curl_multi_setopt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_addpart() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_option_by_name() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_maprintf() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI curl_slist_free_all(CurlSlist* list) {
    while (list != nullptr) {
        auto* next = list->next;
        free_nid_postfix(list->data);
        free_nid_postfix(list);
        list = next;
    }
}

int APS5_VABI curl_url_cleanup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_getdate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_filedata() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_timeout() {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_easy_unescape(void* handle, const char* string, int length, int* outputLength) {
    (void)handle;
    if (length < 0) return nullptr;
    if (string == nullptr) NotImplemented_nid_no_patch(__func__);
    const auto size = length == 0 ? std::strlen(string) : static_cast<std::size_t>(length);
    if (size == std::numeric_limits<std::size_t>::max()) return nullptr;
    auto* output = static_cast<char*>(malloc_nid_postfix(size + 1));
    if (output == nullptr) return nullptr;
    const auto hex = [](unsigned char byte) {
        if (byte >= '0' && byte <= '9') return byte - '0';
        if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
        if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
        return -1;
    };
    std::size_t written = 0;
    for (std::size_t index = 0; index < size; ++index) {
        auto byte = static_cast<unsigned char>(string[index]);
        if (byte == '%' && size - index >= 3) {
            const int high = hex(static_cast<unsigned char>(string[index + 1]));
            const int low = hex(static_cast<unsigned char>(string[index + 2]));
            if (high >= 0 && low >= 0) {
                byte = static_cast<unsigned char>((high << 4) | low);
                index += 2;
            }
        }
        output[written++] = static_cast<char>(byte);
    }
    output[written] = '\0';
    if (outputLength != nullptr) {
        if (written > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            free_nid_postfix(output);
            return nullptr;
        }
        *outputLength = static_cast<int>(written);
    }
    return output;
}

int APS5_VABI curl_multi_add_handle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_option_by_id() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_formfree() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_subparts() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_version_info() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_pushheader_byname() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_encoder() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvprintf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_global_init_mem() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_headers() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_url_dup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_multi_cleanup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_easy_pause() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mime_filename() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_msprintf() {
    NotImplemented_nid_no_patch(__func__);
}

}
