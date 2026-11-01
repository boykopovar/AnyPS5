#include "prx/libcurl/libcurl.hpp"
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

    char* esc = curl_easy_escape_nid_postfix(nullptr, "Hello World! 123", 16);
    Require(esc != nullptr);
    Require(std::strcmp(esc, "Hello%20World%21%20123") == 0);

    int outlen = 0;
    char* unesc = curl_easy_unescape_nid_postfix(nullptr, esc, 0, &outlen);
    Require(unesc != nullptr);
    Require(outlen == 16);
    Require(std::strcmp(unesc, "Hello World! 123") == 0);

    curl_free_nid_postfix(esc);
    curl_free_nid_postfix(unesc);

    char* old_esc = curl_escape_nid_postfix("Foo Bar", 7);
    Require(old_esc != nullptr);
    Require(std::strcmp(old_esc, "Foo%20Bar") == 0);

    char* old_unesc = curl_unescape_nid_postfix(old_esc, 0);
    Require(old_unesc != nullptr);
    Require(std::strcmp(old_unesc, "Foo Bar") == 0);

    curl_free_nid_postfix(old_esc);
    curl_free_nid_postfix(old_unesc);

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

    const char bytes[] = {'a', '\0', 'b', static_cast<char>(0xff)};
    char* binary = curl_easy_escape_nid_postfix(nullptr, bytes, sizeof(bytes));
    Require(binary && std::strcmp(binary, "a%00b%FF") == 0);
    int decodedLength = -1;
    char* decoded = curl_easy_unescape_nid_postfix(nullptr, binary, 0, &decodedLength);
    Require(decoded && decodedLength == sizeof(bytes));
    Require(std::memcmp(decoded, bytes, sizeof(bytes)) == 0);
    curl_free_nid_postfix(binary);
    curl_free_nid_postfix(decoded);
    Require(curl_version_info_nid_postfix(CURLVERSION_FIRST) == info);
    curl_global_cleanup_nid_postfix();
    return 0;
}
