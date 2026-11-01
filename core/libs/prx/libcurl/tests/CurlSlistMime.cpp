#include "prx/libcurl/libcurl.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    guest_curl_slist* list = nullptr;
    const char* entries[] = {"Header: Value", "Accept: text/plain", "", "Header: second", "X-Test: \xc3\xa9"};
    for (const char* entry : entries) {
        auto* result = curl_slist_append_nid_postfix(list, entry);
        Require(result != nullptr);
        if (list) Require(result == list);
        list = result;
    }
    auto* node = list;
    for (const char* entry : entries) {
        Require(node != nullptr && std::strcmp(node->data, entry) == 0);
        node = node->next;
    }
    Require(node == nullptr);
    curl_slist_free_all_nid_postfix(list);
    curl_slist_free_all_nid_postfix(nullptr);

    CURL* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    curl_mime* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    curl_mimepart* part = curl_mime_addpart_nid_postfix(mime);
    Require(part != nullptr);
    Require(curl_mime_name_nid_postfix(part, "field") == CURLE_OK);
    const char bytes[] = {'a', '\0', 'b'};
    Require(curl_mime_data_nid_postfix(part, bytes, sizeof(bytes)) == CURLE_OK);
    Require(curl_mime_type_nid_postfix(part, "application/octet-stream") == CURLE_OK);
    Require(curl_mime_filename_nid_postfix(part, "upload.bin") == CURLE_OK);
    for (const char* encoder : {"binary", "8bit", "7bit", "base64", "quoted-printable"})
        Require(curl_mime_encoder_nid_postfix(part, encoder) == CURLE_OK);
    Require(curl_mime_encoder_nid_postfix(part, "unknown") == CURLE_BAD_FUNCTION_ARGUMENT);
    auto* owned = curl_slist_append_nid_postfix(nullptr, "X-Mime: yes");
    Require(owned != nullptr);
    Require(curl_mime_headers_nid_postfix(part, owned, 1) == CURLE_OK);
    Require(curl_mime_headers_nid_postfix(part, nullptr, 0) == CURLE_OK);
    auto* sub = curl_mime_init_nid_postfix(easy);
    Require(sub != nullptr);
    auto* subpart = curl_mime_addpart_nid_postfix(sub);
    Require(subpart != nullptr);
    Require(curl_mime_data_nid_postfix(subpart, "nested", CURL_ZERO_TERMINATED) == CURLE_OK);
    auto* container = curl_mime_addpart_nid_postfix(mime);
    Require(container != nullptr);
    Require(curl_mime_subparts_nid_postfix(container, sub) == CURLE_OK);
    const char* filename = "curl-mime-test.tmp";
    auto* file = std::fopen(filename, "wb");
    Require(file != nullptr);
    Require(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    Require(std::fclose(file) == 0);
    Require(curl_mime_filedata_nid_postfix(part, filename) == CURLE_OK);
    Require(curl_mime_filedata_nid_postfix(part, nullptr) == CURLE_OK);
    Require(std::remove(filename) == 0);
    Require(curl_mime_data_nid_postfix(nullptr, "x", 1) == CURLE_BAD_FUNCTION_ARGUMENT);
    Require(curl_mime_subparts_nid_postfix(nullptr, nullptr) == CURLE_BAD_FUNCTION_ARGUMENT);
    curl_mime_free_nid_postfix(mime);
    curl_mime_free_nid_postfix(nullptr);
    curl_easy_cleanup_nid_postfix(easy);
    curl_global_cleanup_nid_postfix();
}
