#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const char* ver = curl_version_nid_postfix();
    Require(ver != nullptr);
    Require(std::strncmp(ver, "libcurl/", 8) == 0);

    const auto* info = curl_version_info_nid_postfix(CURLVERSION_NOW);
    Require(info != nullptr);
    Require(info->version != nullptr);
    Require(info->ssl_version != nullptr);
    Require(std::strstr(info->ssl_version, "mbedTLS") != nullptr);
    Require(info->protocols != nullptr);

    bool has_http = false;
    bool has_file = false;
    for (const char* const* p = info->protocols; *p != nullptr; ++p) {
        if (std::strcmp(*p, "http") == 0) has_http = true;
        if (std::strcmp(*p, "file") == 0) has_file = true;
    }
    Require(has_http);
    Require(has_file);

    const std::time_t date1 = curl_getdate_nid_postfix("Sun, 06 Nov 1994 08:49:37 GMT", nullptr);
    Require(date1 == 784111777);

    const std::time_t date2 = curl_getdate_nid_postfix("Sunday, 06-Nov-94 08:49:37 GMT", nullptr);
    Require(date2 == 784111777);

    const std::time_t date3 = curl_getdate_nid_postfix("Sun Nov  6 08:49:37 1994", nullptr);
    Require(date3 == 784111777);

    const std::time_t invalid_date = curl_getdate_nid_postfix("Not a valid date string", nullptr);
    Require(invalid_date == -1);

    Require(std::strcmp(curl_easy_strerror_nid_postfix(CURLE_OK), "No error") == 0);
    Require(curl_easy_strerror_nid_postfix(CURLE_UNSUPPORTED_PROTOCOL) != nullptr);

    Require(std::strcmp(curl_multi_strerror_nid_postfix(CURLM_OK), "No error") == 0);
    Require(curl_multi_strerror_nid_postfix(CURLM_BAD_HANDLE) != nullptr);

    Require(std::strcmp(curl_share_strerror_nid_postfix(CURLSHE_OK), "No error") == 0);
    Require(curl_share_strerror_nid_postfix(CURLSHE_BAD_OPTION) != nullptr);

    char* non_env = curl_getenv_nid_postfix("NONEXISTENT_VAR_ANYPS5_TEST_12345");
    Require(non_env == nullptr);

    Require(curl_version_info_nid_postfix(CURLVERSION_FIRST) == info);

    Require(curl_easy_escape(nullptr, "test", -1) == nullptr);
    char* escaped = curl_easy_escape(nullptr, "hello world!", 0);
    Require(escaped != nullptr);
    Require(std::strcmp(escaped, "hello%20world%21") == 0);
    int unescapedLength = 0;
    Require(curl_easy_unescape(nullptr, "test", -1, nullptr) == nullptr);
    char* unescaped = curl_easy_unescape(nullptr, escaped, 0, &unescapedLength);
    Require(unescaped != nullptr);
    Require(std::strcmp(unescaped, "hello world!") == 0);
    Require(unescapedLength == 12);
    curl_free(escaped);
    curl_free(unescaped);
    curl_free(nullptr);

    auto* list = curl_slist_append(nullptr, "alpha");
    Require(list != nullptr);
    list = curl_slist_append(list, "beta");
    Require(list != nullptr);
    Require(std::strcmp(list->data, "alpha") == 0);
    Require(list->next != nullptr);
    Require(std::strcmp(list->next->data, "beta") == 0);
    curl_slist_free_all(list);
    curl_slist_free_all(nullptr);

    curl_global_cleanup_nid_postfix();
    return 0;
}
