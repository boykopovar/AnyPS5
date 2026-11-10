#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static int callbackCount;

static void APS5_VABI Lock(CURL* easy, curl_lock_data data, curl_lock_access access, void* argument) {
    Require(easy == nullptr && data == CURL_LOCK_DATA_SHARE && access == CURL_LOCK_ACCESS_SINGLE);
    Require(argument == &callbackCount);
    ++callbackCount;
}

static void APS5_VABI Unlock(CURL* easy, curl_lock_data data, void* argument) {
    Require(easy == nullptr && data == CURL_LOCK_DATA_SHARE);
    Require(argument == &callbackCount);
    ++callbackCount;
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    const guest_curl_ssl_backend** backends = nullptr;
    Require(curl_global_sslset_nid_postfix(CURLSSLBACKEND_MBEDTLS, nullptr, &backends) == CURLSSLSET_OK);
    Require(backends && backends[0] && backends[0]->id == CURLSSLBACKEND_MBEDTLS);
    Require(backends[1] == nullptr);
    Require(curl_pushheader_bynum_nid_postfix(nullptr, 0) == nullptr);
    Require(curl_pushheader_byname_nid_postfix(nullptr, "x-test") == nullptr);
    CURL* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_URL_MALFORMAT);
    Require(curl_easy_upkeep_nid_postfix(easy) == CURLE_OK);
    char buffer[8]{};
    size_t count = 99;
    Require(curl_easy_recv_nid_postfix(easy, buffer, sizeof(buffer), &count) == CURLE_UNSUPPORTED_PROTOCOL);
    Require(curl_easy_send_nid_postfix(easy, buffer, sizeof(buffer), &count) == CURLE_UNSUPPORTED_PROTOCOL);
    Require(curl_easy_pause_nid_postfix(easy, CURLPAUSE_CONT) == CURLE_BAD_FUNCTION_ARGUMENT);
    auto* duplicate = curl_easy_duphandle_nid_postfix(easy);
    Require(duplicate != nullptr && duplicate != easy);
    curl_easy_reset_nid_postfix(duplicate);
    curl_easy_cleanup_nid_postfix(duplicate);
    auto* share = curl_share_init_nid_postfix();
    Require(share != nullptr);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_UNSHARE, CURL_LOCK_DATA_COOKIE) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_USERDATA, &callbackCount) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_LOCKFUNC, Lock) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_UNLOCKFUNC, Unlock) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_LAST) == CURLSHE_BAD_OPTION);
    Require(curl_share_cleanup_nid_postfix(share) == CURLSHE_OK);
    Require(callbackCount == 2);
    Require(curl_share_cleanup_nid_postfix(nullptr) == CURLSHE_INVALID);
    Require(curl_share_setopt_nid_postfix(nullptr, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) == CURLSHE_INVALID);
    auto* multi = curl_multi_init_nid_postfix();
    Require(multi != nullptr);
    int running = 99;
    Require(curl_multi_perform_nid_postfix(multi, &running) == CURLM_OK && running == 0);
    int64_t timeout = 99;
    Require(curl_multi_timeout_nid_postfix(multi, &timeout) == CURLM_OK && timeout == -1);
    Require(curl_multi_timeout_nid_postfix(multi, nullptr) == CURLM_BAD_FUNCTION_ARGUMENT);
    Require(curl_multi_add_handle_nid_postfix(multi, easy) == CURLM_OK);
    Require(curl_multi_add_handle_nid_postfix(multi, easy) == CURLM_ADDED_ALREADY);
    Require(curl_multi_perform_nid_postfix(multi, &running) == CURLM_OK && running == 0);
    int remaining = 99;
    auto* message = curl_multi_info_read_nid_postfix(multi, &remaining);
    Require(message && message->msg == CURLMSG_DONE && message->easy_handle == easy);
    Require(message->data.result == CURLE_URL_MALFORMAT);
    Require(remaining == 0);
    Require(curl_multi_info_read_nid_postfix(multi, &remaining) == nullptr);
    Require(curl_multi_remove_handle_nid_postfix(multi, easy) == CURLM_OK);
    Require(curl_multi_socket_all_nid_postfix(multi, &running) == CURLM_OK && running == 0);
    Require(curl_multi_wakeup_nid_postfix(multi) == CURLM_OK);
    Require(curl_multi_cleanup_nid_postfix(multi) == CURLM_OK);
    curl_easy_cleanup_nid_postfix(easy);
    curl_easy_cleanup_nid_postfix(nullptr);
    curl_global_cleanup_nid_postfix();
}
