#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static int callbackCount;

struct MultiTimerFixture {
    CURLM* first = nullptr;
    CURLM* second = nullptr;
    void* firstOpaque = nullptr;
    void* secondOpaque = nullptr;
    int firstCalls = 0;
    int secondCalls = 0;
    int64_t firstTimeout = -2;
    int64_t secondTimeout = -2;
};

static MultiTimerFixture* activeFixture = nullptr;

static int APS5_VABI RecordMultiTimer(CURLM* multi, int64_t timeout, void* opaque) {
    Require(activeFixture != nullptr);
    Require(timeout >= -1);
    if (multi == activeFixture->first) {
        Require(opaque == activeFixture->firstOpaque);
        ++activeFixture->firstCalls;
        activeFixture->firstTimeout = timeout;
    } else if (multi == activeFixture->second) {
        Require(opaque == activeFixture->secondOpaque);
        ++activeFixture->secondCalls;
        activeFixture->secondTimeout = timeout;
    } else {
        Require(false);
    }
    return 0;
}

static void ExerciseMultiTimer(CURLM* multi, int& calls, bool enabled) {
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    const int before = calls;
    Require(curl_multi_add_handle_nid_postfix(multi, easy) == CURLM_OK);
    if (enabled) Require(calls > before);
    else Require(calls == before);
    int running = -1;
    Require(curl_multi_perform_nid_postfix(multi, &running) == CURLM_OK && running == 0);
    if (enabled) Require(calls > before);
    else Require(calls == before);
    int remaining = -1;
    auto* message = curl_multi_info_read_nid_postfix(multi, &remaining);
    Require(message && message->msg == CURLMSG_DONE && message->easy_handle == easy);
    Require(message->data.result == CURLE_URL_MALFORMAT);
    Require(remaining == 0);
    Require(curl_multi_remove_handle_nid_postfix(multi, easy) == CURLM_OK);
    curl_easy_cleanup_nid_postfix(easy);
}

static void TestMultiTimerContextRebind() {
    MultiTimerFixture fixture;
    activeFixture = &fixture;
    int initialOpaque = 0;
    int reboundOpaque = 0;
    int otherOpaque = 0;
    fixture.first = curl_multi_init_nid_postfix();
    fixture.second = curl_multi_init_nid_postfix();
    Require(fixture.first != nullptr && fixture.second != nullptr && fixture.first != fixture.second);
    fixture.firstOpaque = &initialOpaque;
    fixture.secondOpaque = &otherOpaque;
    Require(curl_multi_setopt_nid_postfix(fixture.first, CURLMOPT_TIMERFUNCTION, RecordMultiTimer) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(fixture.first, CURLMOPT_TIMERDATA, &initialOpaque) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(fixture.second, CURLMOPT_TIMERFUNCTION, RecordMultiTimer) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(fixture.second, CURLMOPT_TIMERDATA, &otherOpaque) == CURLM_OK);
    ExerciseMultiTimer(fixture.first, fixture.firstCalls, true);
    ExerciseMultiTimer(fixture.second, fixture.secondCalls, true);
    Require(fixture.firstCalls > 0 && fixture.secondCalls > 0);
    Require(fixture.firstTimeout == -1 && fixture.secondTimeout == -1);
    const int secondBeforeRebind = fixture.secondCalls;
    fixture.firstOpaque = &reboundOpaque;
    Require(curl_multi_setopt_nid_postfix(fixture.first, CURLMOPT_TIMERDATA, &reboundOpaque) == CURLM_OK);
    ExerciseMultiTimer(fixture.first, fixture.firstCalls, true);
    Require(fixture.secondCalls == secondBeforeRebind);
    ExerciseMultiTimer(fixture.second, fixture.secondCalls, true);
    Require(fixture.secondCalls > secondBeforeRebind);
    const int firstBeforeDisable = fixture.firstCalls;
    Require(curl_multi_setopt_nid_postfix(fixture.first, CURLMOPT_TIMERFUNCTION, nullptr) == CURLM_OK);
    ExerciseMultiTimer(fixture.first, fixture.firstCalls, false);
    Require(fixture.firstCalls == firstBeforeDisable);
    const int secondBeforeDisable = fixture.secondCalls;
    ExerciseMultiTimer(fixture.second, fixture.secondCalls, true);
    Require(fixture.secondCalls > secondBeforeDisable);
    Require(curl_multi_cleanup_nid_postfix(fixture.second) == CURLM_OK);
    Require(curl_multi_cleanup_nid_postfix(fixture.first) == CURLM_OK);
    activeFixture = nullptr;
}

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

static int APS5_VABI DenyPush(CURL*, CURL*, size_t, struct curl_pushheaders*, void*) {
    return CURL_PUSH_DENY;
}

static int convertCount;

static CURLcode APS5_VABI Convert(char*, size_t) {
    ++convertCount;
    return CURLE_CONV_FAILED;
}

extern "C" int Push_nid_no_patch(CURL*, CURL*, size_t, struct curl_pushheaders*, void*);

struct ShareCookieState {
    int shareLocks = 0;
    int shareUnlocks = 0;
    int cookieLocks = 0;
    int cookieUnlocks = 0;
    int otherLocks = 0;
    int otherUnlocks = 0;
    CURL* first = nullptr;
    CURL* second = nullptr;
};

static void APS5_VABI ShareCookieLock(CURL* easy, curl_lock_data data, curl_lock_access access, void* argument) {
    auto* state = static_cast<ShareCookieState*>(argument);
    Require(state != nullptr);
    Require(access == CURL_LOCK_ACCESS_SINGLE || access == CURL_LOCK_ACCESS_SHARED);
    if (data == CURL_LOCK_DATA_SHARE) {
        Require(easy == nullptr || easy == state->first || easy == state->second);
        ++state->shareLocks;
    } else if (data == CURL_LOCK_DATA_COOKIE) {
        Require(easy == state->first || easy == state->second);
        ++state->cookieLocks;
    } else {
        ++state->otherLocks;
    }
}

static void APS5_VABI ShareCookieUnlock(CURL* easy, curl_lock_data data, void* argument) {
    auto* state = static_cast<ShareCookieState*>(argument);
    Require(state != nullptr);
    if (data == CURL_LOCK_DATA_SHARE) {
        Require(easy == nullptr || easy == state->first || easy == state->second);
        ++state->shareUnlocks;
    } else if (data == CURL_LOCK_DATA_COOKIE) {
        Require(easy == state->first || easy == state->second);
        ++state->cookieUnlocks;
    } else {
        ++state->otherUnlocks;
    }
}

static void TestShareCookieLifetime() {
    auto* share = curl_share_init_nid_postfix();
    Require(share != nullptr);
    ShareCookieState state{};
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_USERDATA, &state) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_LOCKFUNC, ShareCookieLock) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_UNLOCKFUNC, ShareCookieUnlock) == CURLSHE_OK);
    Require(curl_share_setopt_nid_postfix(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) == CURLSHE_OK);

    CURL* first = curl_easy_init_nid_postfix();
    CURL* second = curl_easy_init_nid_postfix();
    Require(first != nullptr && second != nullptr && first != second);
    state.first = first;
    state.second = second;

    Require(curl_easy_setopt_nid_postfix(first, CURLOPT_SHARE, share) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(second, CURLOPT_SHARE, share) == CURLE_OK);
    Require(state.shareLocks >= 2 && state.shareLocks == state.shareUnlocks);

    Require(curl_share_cleanup_nid_postfix(share) == CURLSHE_IN_USE);
    Require(state.shareLocks == state.shareUnlocks);

    const char* cookieLine = "example.com\tFALSE\t/\tFALSE\t0\tsession\tabc123";
    Require(curl_easy_setopt_nid_postfix(first, CURLOPT_COOKIELIST, cookieLine) == CURLE_OK);
    Require(state.cookieLocks >= 1 && state.cookieLocks == state.cookieUnlocks);

    const int cookieLocksBeforeRead = state.cookieLocks;
    CurlSlist* cookies = nullptr;
    Require(curl_easy_getinfo_nid_postfix(second, CURLINFO_COOKIELIST, &cookies) == CURLE_OK);
    Require(cookies != nullptr);
    Require(cookies->data != nullptr && std::strcmp(cookies->data, cookieLine) == 0);
    Require(cookies->next == nullptr);
    curl_slist_free_all(cookies);
    Require(state.cookieLocks > cookieLocksBeforeRead);
    Require(state.cookieLocks == state.cookieUnlocks);

    Require(curl_easy_setopt_nid_postfix(first, CURLOPT_SHARE, nullptr) == CURLE_OK);
    Require(curl_share_cleanup_nid_postfix(share) == CURLSHE_IN_USE);
    Require(state.shareLocks == state.shareUnlocks);

    CurlSlist* detachedCookies = nullptr;
    Require(curl_easy_getinfo_nid_postfix(first, CURLINFO_COOKIELIST, &detachedCookies) == CURLE_OK);
    Require(detachedCookies == nullptr);
    curl_slist_free_all(detachedCookies);

    Require(curl_easy_setopt_nid_postfix(second, CURLOPT_SHARE, nullptr) == CURLE_OK);
    Require(curl_share_cleanup_nid_postfix(share) == CURLSHE_OK);
    Require(state.shareLocks == state.shareUnlocks);
    Require(state.cookieLocks == state.cookieUnlocks);
    Require(state.otherLocks == state.otherUnlocks);

    curl_easy_cleanup_nid_postfix(second);
    curl_easy_cleanup_nid_postfix(first);
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
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CONV_FROM_NETWORK_FUNCTION, Convert) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CONV_TO_NETWORK_FUNCTION, Convert) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CONV_FROM_UTF8_FUNCTION, Convert) == CURLE_OK);
    auto* duplicate = curl_easy_duphandle_nid_postfix(easy);
    Require(duplicate != nullptr && duplicate != easy);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_URL_MALFORMAT);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CONV_FROM_NETWORK_FUNCTION, nullptr) == CURLE_OK);
    Require(convertCount == 0);
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
    errno = 71;
    Require(curl_multi_timeout_nid_postfix(multi, reinterpret_cast<int64_t*>(0x10)) == CURLM_BAD_FUNCTION_ARGUMENT && errno == 71);
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
    Require(curl_multi_setopt_nid_postfix(nullptr, CURLMOPT_PUSHFUNCTION, DenyPush) == CURLM_BAD_HANDLE);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_PUSHFUNCTION, DenyPush) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_PUSHDATA, &callbackCount) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_PUSHFUNCTION, nullptr) == CURLM_OK);
    Require(curl_multi_cleanup_nid_postfix(multi) == CURLM_OK);
    curl_easy_cleanup_nid_postfix(easy);
    Require(Push_nid_no_patch(easy, nullptr, 0, nullptr, nullptr) == CURL_PUSH_DENY);
    curl_easy_cleanup_nid_postfix(nullptr);
    TestShareCookieLifetime();
    TestMultiTimerContextRebind();
    curl_global_cleanup_nid_postfix();
}
