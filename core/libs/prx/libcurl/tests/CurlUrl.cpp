#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    struct UrlCase {
        const char* input;
        const char* expected;
    };
    const UrlCase cases[] = {
        {"https://example.com", "https://example.com/"},
        {"http://example.com:80/a%2Fb?x=1#frag", "http://example.com:80/a%2Fb?x=1#frag"},
        {"http://[::1]:8080/", "http://[::1]:8080/"},
        {"http://example.com/a/../b", "http://example.com/b"},
        {"http://example.com/a/./b", "http://example.com/a/b"},
        {"file:///tmp/test", "file:///tmp/test"},
        {"https://example.com:443", "https://example.com:443/"},
        {"http://user:pass@example.com/", "http://user:pass@example.com/"},
        {"https://example.com/?a=b&c=d", "https://example.com/?a=b&c=d"},
        {"http://127.0.0.1/", "http://127.0.0.1/"},
        {"http://example.com/path#fragment", "http://example.com/path#fragment"},
        {"https://example.com/a%20b", "https://example.com/a%20b"},
    };
    for (const auto& item : cases) {
        auto* handle = curl_url_nid_postfix();
        Require(handle != nullptr);
        Require(curl_url_set_nid_postfix(handle, CURLUPART_URL, item.input, 0) == CURLUE_OK);
        char* actual = nullptr;
        Require(curl_url_get_nid_postfix(handle, CURLUPART_URL, &actual, 0) == CURLUE_OK);
        Require(actual != nullptr && std::strcmp(actual, item.expected) == 0);
        free_nid_postfix(actual);
        curl_url_cleanup_nid_postfix(handle);
    }
    CURLU* u = curl_url_nid_postfix();
    Require(u != nullptr);

    const auto set_rc = curl_url_set_nid_postfix(u, CURLUPART_URL, "https://example.com:8443/test/path?foo=bar#section", 0);
    Require(set_rc == CURLUE_OK);

    char* scheme = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK);
    Require(scheme != nullptr);
    Require(std::strcmp(scheme, "https") == 0);
    free_nid_postfix(scheme);

    char* host = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_HOST, &host, 0) == CURLUE_OK);
    Require(host != nullptr);
    Require(std::strcmp(host, "example.com") == 0);
    free_nid_postfix(host);

    char* port = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_PORT, &port, 0) == CURLUE_OK);
    Require(port != nullptr);
    Require(std::strcmp(port, "8443") == 0);
    free_nid_postfix(port);

    char* path = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_PATH, &path, 0) == CURLUE_OK);
    Require(path != nullptr);
    Require(std::strcmp(path, "/test/path") == 0);
    free_nid_postfix(path);

    char* query = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &query, 0) == CURLUE_OK);
    Require(query != nullptr);
    Require(std::strcmp(query, "foo=bar") == 0);
    free_nid_postfix(query);

    char* frag = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_FRAGMENT, &frag, 0) == CURLUE_OK);
    Require(frag != nullptr);
    Require(std::strcmp(frag, "section") == 0);
    free_nid_postfix(frag);

    Require(curl_url_set_nid_postfix(u, CURLUPART_QUERY, "baz=qux", CURLU_APPENDQUERY) == CURLUE_OK);
    char* updated_query = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &updated_query, 0) == CURLUE_OK);
    Require(updated_query != nullptr);
    Require(std::strcmp(updated_query, "foo=bar&baz=qux") == 0);
    free_nid_postfix(updated_query);

    CURLU* dup = curl_url_dup_nid_postfix(u);
    Require(dup != nullptr);

    char* dup_url = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_URL, &dup_url, 0) == CURLUE_OK);
    Require(dup_url != nullptr);
    Require(std::strcmp(dup_url, "https://example.com:8443/test/path?foo=bar&baz=qux#section") == 0);
    free_nid_postfix(dup_url);

    curl_url_cleanup_nid_postfix(dup);
    curl_url_cleanup_nid_postfix(u);

    CURLU* u2 = curl_url_nid_postfix();
    Require(u2 != nullptr);
    Require(curl_url_set_nid_postfix(u2, CURLUPART_SCHEME, "http", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u2, CURLUPART_USER, "testuser", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u2, CURLUPART_PASSWORD, "testpass", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u2, CURLUPART_HOST, "localhost", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u2, CURLUPART_PATH, "/index.html", 0) == CURLUE_OK);

    char* built_url = nullptr;
    Require(curl_url_get_nid_postfix(u2, CURLUPART_URL, &built_url, 0) == CURLUE_OK);
    Require(built_url != nullptr);
    Require(std::strcmp(built_url, "http://testuser:testpass@localhost/index.html") == 0);
    free_nid_postfix(built_url);

    curl_url_cleanup_nid_postfix(u2);
    return 0;
}
