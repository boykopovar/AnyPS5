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

static void TestFailedPortUpdatePreservesUrl() {
    auto* u = curl_url_nid_postfix();
    Require(u != nullptr);
    Require(curl_url_set_nid_postfix(u, CURLUPART_URL, "https://example.com:8080/test/path", 0) == CURLUE_OK);

    char* port = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_PORT, &port, 0) == CURLUE_OK);
    Require(port != nullptr && std::strcmp(port, "8080") == 0);
    free_nid_postfix(port);

    const char* const invalidPorts[] = {"invalid", "0", "65536", "-1", "80foo", "99999"};
    for (const auto* invalidPort : invalidPorts) {
        Require(curl_url_set_nid_postfix(u, CURLUPART_PORT, invalidPort, 0) == CURLUE_BAD_PORT_NUMBER);
        char* currentPort = nullptr;
        Require(curl_url_get_nid_postfix(u, CURLUPART_PORT, &currentPort, 0) == CURLUE_OK);
        Require(currentPort != nullptr && std::strcmp(currentPort, "8080") == 0);
        free_nid_postfix(currentPort);

        char* currentUrl = nullptr;
        Require(curl_url_get_nid_postfix(u, CURLUPART_URL, &currentUrl, 0) == CURLUE_OK);
        Require(currentUrl != nullptr && std::strcmp(currentUrl, "https://example.com:8080/test/path") == 0);
        free_nid_postfix(currentUrl);
    }

    curl_url_cleanup_nid_postfix(u);
}

static void TestDuplicatePartIsolationAndFragmentRemoval() {
    auto* original = curl_url_nid_postfix();
    Require(original != nullptr);
    Require(curl_url_set_nid_postfix(original, CURLUPART_URL, "https://example.com:8080/orig/path?key=val#frag1", 0) == CURLUE_OK);

    auto* dup = curl_url_dup_nid_postfix(original);
    Require(dup != nullptr);

    Require(curl_url_set_nid_postfix(dup, CURLUPART_PATH, "/modified/path", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(dup, CURLUPART_QUERY, "newkey=newval", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(dup, CURLUPART_FRAGMENT, nullptr, 0) == CURLUE_OK);

    char* origPath = nullptr;
    Require(curl_url_get_nid_postfix(original, CURLUPART_PATH, &origPath, 0) == CURLUE_OK);
    Require(origPath != nullptr && std::strcmp(origPath, "/orig/path") == 0);
    free_nid_postfix(origPath);

    char* origQuery = nullptr;
    Require(curl_url_get_nid_postfix(original, CURLUPART_QUERY, &origQuery, 0) == CURLUE_OK);
    Require(origQuery != nullptr && std::strcmp(origQuery, "key=val") == 0);
    free_nid_postfix(origQuery);

    char* origFrag = nullptr;
    Require(curl_url_get_nid_postfix(original, CURLUPART_FRAGMENT, &origFrag, 0) == CURLUE_OK);
    Require(origFrag != nullptr && std::strcmp(origFrag, "frag1") == 0);
    free_nid_postfix(origFrag);

    char* origUrl = nullptr;
    Require(curl_url_get_nid_postfix(original, CURLUPART_URL, &origUrl, 0) == CURLUE_OK);
    Require(origUrl != nullptr && std::strcmp(origUrl, "https://example.com:8080/orig/path?key=val#frag1") == 0);
    free_nid_postfix(origUrl);

    char* dupPath = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_PATH, &dupPath, 0) == CURLUE_OK);
    Require(dupPath != nullptr && std::strcmp(dupPath, "/modified/path") == 0);
    free_nid_postfix(dupPath);

    char* dupQuery = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_QUERY, &dupQuery, 0) == CURLUE_OK);
    Require(dupQuery != nullptr && std::strcmp(dupQuery, "newkey=newval") == 0);
    free_nid_postfix(dupQuery);

    char* dupFrag = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_FRAGMENT, &dupFrag, 0) == CURLUE_NO_FRAGMENT);

    char* dupUrl = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_URL, &dupUrl, 0) == CURLUE_OK);
    Require(dupUrl != nullptr && std::strcmp(dupUrl, "https://example.com:8080/modified/path?newkey=newval") == 0);
    free_nid_postfix(dupUrl);

    Require(curl_url_set_nid_postfix(dup, CURLUPART_FRAGMENT, "", 0) == CURLUE_OK);
    char* emptyFrag = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_FRAGMENT, &emptyFrag, 0) == CURLUE_OK);
    Require(emptyFrag != nullptr && std::strcmp(emptyFrag, "") == 0);
    free_nid_postfix(emptyFrag);

    char* emptyFragUrl = nullptr;
    Require(curl_url_get_nid_postfix(dup, CURLUPART_URL, &emptyFragUrl, 0) == CURLUE_OK);
    Require(emptyFragUrl != nullptr && std::strcmp(emptyFragUrl, "https://example.com:8080/modified/path?newkey=newval#") == 0);
    free_nid_postfix(emptyFragUrl);

    Require(curl_url_get_nid_postfix(original, CURLUPART_FRAGMENT, &origFrag, 0) == CURLUE_OK);
    Require(origFrag != nullptr && std::strcmp(origFrag, "frag1") == 0);
    free_nid_postfix(origFrag);

    curl_url_cleanup_nid_postfix(dup);
    curl_url_cleanup_nid_postfix(original);
}

static void TestReadBuffersLifetimeUntilFreed() {
    auto* u = curl_url_nid_postfix();
    Require(u != nullptr);
    Require(curl_url_set_nid_postfix(u, CURLUPART_URL, "https://user:pass@example.com:9443/deep/path?query=one#frag", 0) == CURLUE_OK);

    char* scheme = nullptr;
    char* user = nullptr;
    char* password = nullptr;
    char* host = nullptr;
    char* port = nullptr;
    char* path = nullptr;
    char* query = nullptr;
    char* frag = nullptr;
    char* fullUrl = nullptr;

    Require(curl_url_get_nid_postfix(u, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_USER, &user, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_PASSWORD, &password, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_HOST, &host, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_PORT, &port, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_PATH, &path, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &query, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_FRAGMENT, &frag, 0) == CURLUE_OK);
    Require(curl_url_get_nid_postfix(u, CURLUPART_URL, &fullUrl, 0) == CURLUE_OK);

    Require(curl_url_set_nid_postfix(u, CURLUPART_SCHEME, "http", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_USER, "otheruser", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_PASSWORD, "otherpass", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_HOST, "otherhost.org", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_PORT, "80", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_PATH, "/new", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_QUERY, "other=two", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(u, CURLUPART_FRAGMENT, "newfrag", 0) == CURLUE_OK);

    curl_url_cleanup_nid_postfix(u);

    Require(scheme != nullptr && std::strcmp(scheme, "https") == 0);
    Require(user != nullptr && std::strcmp(user, "user") == 0);
    Require(password != nullptr && std::strcmp(password, "pass") == 0);
    Require(host != nullptr && std::strcmp(host, "example.com") == 0);
    Require(port != nullptr && std::strcmp(port, "9443") == 0);
    Require(path != nullptr && std::strcmp(path, "/deep/path") == 0);
    Require(query != nullptr && std::strcmp(query, "query=one") == 0);
    Require(frag != nullptr && std::strcmp(frag, "frag") == 0);
    Require(fullUrl != nullptr && std::strcmp(fullUrl, "https://user:pass@example.com:9443/deep/path?query=one#frag") == 0);

    free_nid_postfix(scheme);
    free_nid_postfix(user);
    free_nid_postfix(password);
    free_nid_postfix(host);
    free_nid_postfix(port);
    free_nid_postfix(path);
    free_nid_postfix(query);
    free_nid_postfix(frag);
    free_nid_postfix(fullUrl);
}

static void TestAppendQueryWithUrlencode() {
    auto* u = curl_url_nid_postfix();
    Require(u != nullptr);
    Require(curl_url_set_nid_postfix(u, CURLUPART_URL, "https://example.com/api", 0) == CURLUE_OK);

    char* initialQuery = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &initialQuery, 0) == CURLUE_NO_QUERY);

    Require(curl_url_set_nid_postfix(u, CURLUPART_QUERY, "tag=hello world&category=a/b=c", CURLU_APPENDQUERY | CURLU_URLENCODE) == CURLUE_OK);

    char* url1 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_URL, &url1, 0) == CURLUE_OK);
    Require(url1 != nullptr && std::strcmp(url1, "https://example.com/api?tag=hello+world%26category%3da%2fb%3dc") == 0);
    free_nid_postfix(url1);

    char* rawQuery1 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &rawQuery1, 0) == CURLUE_OK);
    Require(rawQuery1 != nullptr && std::strcmp(rawQuery1, "tag=hello+world%26category%3da%2fb%3dc") == 0);
    free_nid_postfix(rawQuery1);

    char* decodedQuery1 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &decodedQuery1, CURLU_URLDECODE) == CURLUE_OK);
    Require(decodedQuery1 != nullptr && std::strcmp(decodedQuery1, "tag=hello world&category=a/b=c") == 0);
    free_nid_postfix(decodedQuery1);

    Require(curl_url_set_nid_postfix(u, CURLUPART_QUERY, "filter=x & y=z", CURLU_APPENDQUERY | CURLU_URLENCODE) == CURLUE_OK);

    char* url2 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_URL, &url2, 0) == CURLUE_OK);
    Require(url2 != nullptr && std::strcmp(url2, "https://example.com/api?tag=hello+world%26category%3da%2fb%3dc&filter=x+%26+y%3dz") == 0);
    free_nid_postfix(url2);

    char* rawQuery2 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &rawQuery2, 0) == CURLUE_OK);
    Require(rawQuery2 != nullptr && std::strcmp(rawQuery2, "tag=hello+world%26category%3da%2fb%3dc&filter=x+%26+y%3dz") == 0);
    free_nid_postfix(rawQuery2);

    char* decodedQuery2 = nullptr;
    Require(curl_url_get_nid_postfix(u, CURLUPART_QUERY, &decodedQuery2, CURLU_URLDECODE) == CURLUE_OK);
    Require(decodedQuery2 != nullptr && std::strcmp(decodedQuery2, "tag=hello world&category=a/b=c&filter=x & y=z") == 0);
    free_nid_postfix(decodedQuery2);

    curl_url_cleanup_nid_postfix(u);
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
    TestFailedPortUpdatePreservesUrl();
    TestDuplicatePartIsolationAndFragmentRemoval();
    TestReadBuffersLifetimeUntilFreed();
    TestAppendQueryWithUrlencode();
    return 0;
}
