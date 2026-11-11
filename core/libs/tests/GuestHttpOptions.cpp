#include "prx/libSceHttp/include/Http.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <thread>

namespace {

constexpr int BeforeInit = static_cast<int>(0x80431001u);
constexpr int InvalidId = static_cast<int>(0x80431100u);
constexpr int InvalidValue = static_cast<int>(0x804311FEu);
constexpr int UnknownId = 0x7fff0000;
constexpr int Network = static_cast<int>(0x80431063u);
constexpr int NetworkUnreachable = 51;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

int Get(int (APS5_VABI *getter)(int, int*), int id) {
    int value = -1;
    Require(getter(id, &value) == 0, "an option getter failed on a live object");
    return value;
}

void CheckAutoRedirect(int context) {
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetAutoRedirect, pattern) == 1, "a template does not follow redirects by default");
    Require(sceHttpSetAutoRedirect(pattern, 0) == 0, "sceHttpSetAutoRedirect failed on a template");
    Require(Get(sceHttpGetAutoRedirect, pattern) == 0, "sceHttpGetAutoRedirect does not return what was set");
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    Require(Get(sceHttpGetAutoRedirect, connection) == 0 && Get(sceHttpGetAutoRedirect, request) == 0, "a connection or request did not take its parent's redirect option");
    Require(sceHttpSetAutoRedirect(request, 7) == 0 && Get(sceHttpGetAutoRedirect, request) == 1, "a nonzero value did not enable redirects");
    Require(Get(sceHttpGetAutoRedirect, connection) == 0, "setting a request changed its connection");
    Require(sceHttpSetAutoRedirect(pattern, 1) == 0 && Get(sceHttpGetAutoRedirect, connection) == 0, "an existing connection followed a later template change");
    Require(sceHttpGetAutoRedirect(request, nullptr) == InvalidValue, "a null output was not refused");
    Require(sceHttpGetAutoRedirect(UnknownId, nullptr) == InvalidValue, "the output pointer is not checked before the id");
    int value = 0;
    Require(sceHttpGetAutoRedirect(UnknownId, &value) == InvalidId && sceHttpSetAutoRedirect(UnknownId, 1) == InvalidId, "an unknown id was not refused");
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpGetAutoRedirect(request, &value) == InvalidId, "a deleted request still has options");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

void CheckAuthEnabled(int context) {
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetAuthEnabled, pattern) == 0, "authentication is enabled by default");
    Require(sceHttpSetAuthEnabled(pattern, 1) == 0 && Get(sceHttpGetAuthEnabled, pattern) == 1, "sceHttpGetAuthEnabled did not return the set value");
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    Require(Get(sceHttpGetAuthEnabled, connection) == 1 && Get(sceHttpGetAuthEnabled, request) == 1, "a child did not inherit its parent's authentication option");
    Require(sceHttpSetAuthEnabled(request, 0) == 0 && Get(sceHttpGetAuthEnabled, request) == 0, "authentication could not be disabled on a request");
    Require(Get(sceHttpGetAuthEnabled, connection) == 1 && Get(sceHttpGetAuthEnabled, pattern) == 1, "setting a request changed its parent options");
    int value = -1;
    Require(sceHttpGetAuthEnabled(request, nullptr) == InvalidValue, "a null authentication output was not refused");
    Require(sceHttpGetAuthEnabled(UnknownId, nullptr) == InvalidValue, "the authentication output pointer is not checked before the id");
    Require(sceHttpGetAuthEnabled(UnknownId, &value) == InvalidId && sceHttpSetAuthEnabled(UnknownId, 1) == InvalidId, "an unknown id was not refused for authentication options");
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpGetAuthEnabled(request, &value) == InvalidId, "a deleted request still has authentication options");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

void CheckCookieEnabled(int context) {
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetCookieEnabled, pattern) == 0, "a template enables cookies by default");
    Require(sceHttpSetCookieEnabled(pattern, 1) == 0 && Get(sceHttpGetCookieEnabled, pattern) == 1, "the cookie-enabled getter does not return what was set");
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    Require(Get(sceHttpGetCookieEnabled, connection) == 1 && Get(sceHttpGetCookieEnabled, request) == 1, "a child did not inherit its parent's cookie option");
    Require(sceHttpSetCookieEnabled(request, 0) == 0 && Get(sceHttpGetCookieEnabled, request) == 0, "the cookie option could not be disabled on a child");
    Require(Get(sceHttpGetCookieEnabled, connection) == 1 && Get(sceHttpGetCookieEnabled, pattern) == 1, "changing a child changed its parent or sibling");
    int value = -1;
    Require(sceHttpGetCookieEnabled(request, nullptr) == InvalidValue, "a null cookie output was not refused");
    Require(sceHttpSetCookieEnabled(pattern, -1) == InvalidValue && sceHttpSetCookieEnabled(pattern, 2) == InvalidValue, "an invalid cookie-enabled value was accepted");
    Require(sceHttpGetCookieEnabled(UnknownId, &value) == InvalidId && sceHttpSetCookieEnabled(UnknownId, 1) == InvalidId, "an unknown id was not refused for cookie options");
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpGetCookieEnabled(request, &value) == InvalidId, "a deleted request retained its cookie option");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

void CheckNonblock(int context) {
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetNonblock, pattern) == 0 && Get(sceHttpTryGetNonblock, pattern) == 0, "a template is nonblocking by default");
    Require(sceHttpSetNonblock(pattern, 1) == 0, "sceHttpSetNonblock failed on a template");
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    Require(Get(sceHttpGetNonblock, connection) == 1, "a connection did not take its template's nonblocking option");
    Require(sceHttpTrySetNonblock(connection, 0) == 0 && Get(sceHttpTryGetNonblock, connection) == 0 && Get(sceHttpGetNonblock, connection) == 0, "sceHttpTrySetNonblock did not change the option sceHttpGetNonblock reads");
    Require(Get(sceHttpGetNonblock, pattern) == 1, "setting a connection changed its template");
    int value = 0;
    Require(sceHttpGetNonblock(connection, nullptr) == InvalidValue && sceHttpTryGetNonblock(connection, nullptr) == InvalidValue, "a null output was not refused");
    Require(sceHttpGetNonblock(UnknownId, &value) == InvalidId && sceHttpTryGetNonblock(UnknownId, &value) == InvalidId, "an unknown id was not refused by the getters");
    Require(sceHttpSetNonblock(UnknownId, 1) == InvalidId && sceHttpTrySetNonblock(UnknownId, 1) == InvalidId, "an unknown id was not refused by the setters");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

void CheckAcceptEncodingGzip(int context) {
    const int plain = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetAcceptEncodingGZIPEnabled, plain) == 0, "a template asks for gzip by default");
    Require(sceHttpSetDefaultAcceptEncodingGZIPEnabled(context, 1) == 0, "sceHttpSetDefaultAcceptEncodingGZIPEnabled failed");
    const int compressed = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetAcceptEncodingGZIPEnabled, compressed) == 1, "a new template did not take its context's gzip default");
    Require(Get(sceHttpGetAcceptEncodingGZIPEnabled, plain) == 0, "the context default changed an existing template");
    const int connection = sceHttpCreateConnectionWithURL(compressed, "http://example.com/", 1);
    Require(sceHttpSetAcceptEncodingGZIPEnabled(connection, 0) == 0 && Get(sceHttpGetAcceptEncodingGZIPEnabled, connection) == 0, "sceHttpGetAcceptEncodingGZIPEnabled does not return what was set");
    Require(Get(sceHttpGetAcceptEncodingGZIPEnabled, compressed) == 1, "setting a connection changed its template");
    const int other = sceHttpInit_nid_postfix(1, 1, 0x10000);
    const int otherTemplate = sceHttpCreateTemplate(other, "anyps5-test", 2, 0);
    Require(Get(sceHttpGetAcceptEncodingGZIPEnabled, otherTemplate) == 0, "one context's gzip default reached another context");
    int value = 0;
    Require(sceHttpGetAcceptEncodingGZIPEnabled(connection, nullptr) == InvalidValue, "a null output was not refused");
    Require(sceHttpGetAcceptEncodingGZIPEnabled(UnknownId, &value) == InvalidId && sceHttpSetAcceptEncodingGZIPEnabled(UnknownId, 1) == InvalidId, "an unknown id was not refused");
    Require(sceHttpSetDefaultAcceptEncodingGZIPEnabled(UnknownId, 1) == InvalidId, "an unknown context was not refused");
    Require(sceHttpDeleteTemplate(otherTemplate) == 0 && sceHttpTerm_nid_postfix(other) == 0, "the second context did not close");
    Require(sceHttpSetDefaultAcceptEncodingGZIPEnabled(other, 1) == InvalidId, "a closed context still takes a gzip default");
    Require(sceHttpSetDefaultAcceptEncodingGZIPEnabled(context, 0) == 0, "the gzip default could not be reset");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(compressed) == 0 && sceHttpDeleteTemplate(plain) == 0, "delete failed");
}

void CheckEpoll(int context) {
    HttpEpollHandle epoll = nullptr;
    Require(sceHttpCreateEpoll(context, &epoll) == 0 && epoll != nullptr, "sceHttpCreateEpoll failed");
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    int marker = 0;
    HttpEpollHandle bound = epoll;
    void* argument = &marker;
    Require(sceHttpGetEpoll(request, &bound, &argument) == 0 && bound == nullptr && argument == nullptr, "a new request is bound to an epoll");
    Require(sceHttpSetEpoll(request, epoll, &marker) == 0, "sceHttpSetEpoll failed");
    Require(sceHttpGetEpoll(request, &bound, &argument) == 0 && bound == epoll && argument == &marker, "sceHttpGetEpoll does not return the binding that was set");
    bound = nullptr;
    Require(sceHttpGetEpoll(request, &bound, nullptr) == 0 && bound == epoll, "sceHttpGetEpoll needs the argument output");
    Require(sceHttpGetEpoll(connection, &bound, &argument) == 0 && bound == nullptr, "binding a request bound its connection");
    Require(sceHttpUnsetEpoll(request) == 0 && sceHttpGetEpoll(request, &bound, &argument) == 0 && bound == nullptr && argument == nullptr, "sceHttpUnsetEpoll left the binding");
    Require(sceHttpGetEpoll(request, nullptr, &argument) == InvalidValue, "a null epoll output was not refused");
    Require(sceHttpGetEpoll(UnknownId, &bound, &argument) == InvalidId && sceHttpSetEpoll(UnknownId, epoll, nullptr) == InvalidId && sceHttpUnsetEpoll(UnknownId) == InvalidId, "an unknown id was not refused");
    const auto unknownEpoll = reinterpret_cast<HttpEpollHandle>(std::uintptr_t{0x1234});
    Require(sceHttpSetEpoll(request, nullptr, &marker) == InvalidId && sceHttpSetEpoll(request, unknownEpoll, &marker) == InvalidId, "sceHttpSetEpoll took a handle that is not an epoll");
    Require(sceHttpDestroyEpoll(context, epoll) == 0, "sceHttpDestroyEpoll failed");
    Require(sceHttpSetEpoll(request, epoll, &marker) == InvalidId, "sceHttpSetEpoll took a destroyed epoll");
    Require(sceHttpGetEpoll(request, &bound, &argument) == 0 && bound == nullptr, "a refused sceHttpSetEpoll changed the binding");
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

int LastErrno(int request) {
    int value = -1;
    Require(sceHttpGetLastErrno(request, &value) == 0, "sceHttpGetLastErrno failed");
    return value;
}

void CheckSendFailureIsObservable(int context) {
    HttpEpollHandle epoll = nullptr;
    Require(sceHttpCreateEpoll(context, &epoll) == 0, "sceHttpCreateEpoll failed");
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    int marker = 0;
    HttpNBEvent events[2]{};

    const int blocking = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    Require(LastErrno(blocking) == 0, "a request that was never sent has an errno");
    Require(sceHttpSetEpoll(blocking, epoll, &marker) == 0, "sceHttpSetEpoll failed");
    Require(sceHttpSendRequest(blocking, nullptr, 0) == Network, "a blocking send did not fail with the network error");
    Require(LastErrno(blocking) == NetworkUnreachable, "a failed send did not record its errno");
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0, "a blocking request queued an epoll event");

    const int unbound = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/b", 0);
    Require(sceHttpSetNonblock(unbound, 1) == 0 && sceHttpSendRequest(unbound, nullptr, 0) == Network, "a nonblocking send without an epoll did not fail");
    Require(LastErrno(unbound) == NetworkUnreachable, "a nonblocking send without an epoll did not record its errno");
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0, "an unbound request queued an epoll event");

    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/c", 0);
    Require(sceHttpSetNonblock(request, 1) == 0 && sceHttpSetEpoll(request, epoll, &marker) == 0, "binding a nonblocking request failed");
    Require(sceHttpSendRequest(request, nullptr, 0) == Network, "a nonblocking send did not fail with the network error");
    Require(LastErrno(request) == NetworkUnreachable, "a nonblocking send did not record its errno");
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 1, "a failed nonblocking send queued no epoll event");
    Require(events[0].events == (HTTP_NB_EVENT_SOCK_ERR | HTTP_NB_EVENT_HUP) && events[0].eventDetail == NetworkUnreachable, "the epoll event does not describe the failure");
    Require(events[0].id == request && events[0].userArg == &marker, "the epoll event does not name the request and its user argument");
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0, "an epoll event was delivered twice");

    Require(sceHttpSendRequest(request, nullptr, 0) == Network && sceHttpSendRequest(request, nullptr, 0) == Network, "a repeated send did not fail");
    Require(sceHttpWaitRequest(epoll, events, 1, 0) == 1 && sceHttpWaitRequest(epoll, events, 2, 0) == 1 && sceHttpWaitRequest(epoll, events, 2, 0) == 0, "maxevents did not bound the events delivered");

    int woken = -1;
    HttpNBEvent waited{};
    std::thread waiter([&] { woken = sceHttpWaitRequest(epoll, &waited, 1, -1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(woken == -1, "a wait returned before any event");
    Require(sceHttpSendRequest(request, nullptr, 0) == Network, "send failed while a wait was blocked");
    waiter.join();
    Require(woken == 1 && waited.id == request, "a blocked wait was not woken by the failed send");

    Require(sceHttpSendRequest(request, nullptr, 0) == Network, "send failed before the request was deleted");
    Require(sceHttpDeleteRequest(request) == 0 && LastErrno(request) == 0, "deleting a request kept its errno");
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0, "deleting a request kept its epoll event");

    Require(sceHttpSendRequest(UnknownId, nullptr, 0) == Network && LastErrno(UnknownId) == 0, "an unknown request recorded an errno");
    Require(sceHttpDestroyEpoll(context, epoll) == 0, "sceHttpDestroyEpoll failed");
    Require(sceHttpDeleteRequest(blocking) == 0 && sceHttpDeleteRequest(unbound) == 0, "delete failed");
    Require(sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

void CheckTermEndsOptions() {
    const int context = sceHttpInit_nid_postfix(1, 1, 0x10000);
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    Require(sceHttpTerm_nid_postfix(context) == 0, "sceHttpTerm failed");
    int value = -1;
    HttpEpollHandle bound = nullptr;
    Require(sceHttpGetAutoRedirect(pattern, &value) == BeforeInit && sceHttpSetAutoRedirect(pattern, 0) == BeforeInit, "the redirect option outlived the last sceHttpTerm");
    Require(sceHttpGetAuthEnabled(pattern, &value) == BeforeInit && sceHttpSetAuthEnabled(pattern, 0) == BeforeInit, "the authentication option outlived the last sceHttpTerm");
    Require(sceHttpGetCookieEnabled(pattern, &value) == BeforeInit && sceHttpSetCookieEnabled(pattern, 0) == BeforeInit, "the cookie option outlived the last sceHttpTerm");
    Require(sceHttpGetNonblock(connection, &value) == BeforeInit && sceHttpTryGetNonblock(connection, &value) == BeforeInit, "the nonblocking getters work after the last sceHttpTerm");
    Require(sceHttpSetNonblock(connection, 1) == BeforeInit && sceHttpTrySetNonblock(connection, 1) == BeforeInit, "the nonblocking setters work after the last sceHttpTerm");
    Require(sceHttpGetAcceptEncodingGZIPEnabled(pattern, &value) == BeforeInit && sceHttpSetAcceptEncodingGZIPEnabled(pattern, 1) == BeforeInit, "the gzip option outlived the last sceHttpTerm");
    Require(sceHttpGetEpoll(connection, &bound, nullptr) == BeforeInit && sceHttpUnsetEpoll(connection) == BeforeInit, "the epoll binding outlived the last sceHttpTerm");
    Require(sceHttpCacheRedirectedConnectionEnabled(connection, 1) == BeforeInit, "sceHttpCacheRedirectedConnectionEnabled works after the last sceHttpTerm");
    Require(sceHttpSetProxy(connection, 1, 0, "proxy.example", 3128) == BeforeInit, "sceHttpSetProxy works after the last sceHttpTerm");
    Require(sceHttpsEnableOptionPrivate(connection, 0x4000u) == BeforeInit && sceHttpsDisableOptionPrivate(connection, 0x4000u) == BeforeInit, "the private SSL options work after the last sceHttpTerm");
    Require(sceHttpGetNonblock(connection, nullptr) == BeforeInit, "a null output is checked before initialisation");
    Require(value == -1 && bound == nullptr, "a refused getter wrote its output");
    const int next = sceHttpInit_nid_postfix(1, 1, 0x10000);
    Require(sceHttpSetAutoRedirect(pattern, 0) == InvalidId && sceHttpGetNonblock(connection, &value) == InvalidId, "an object outlived the last sceHttpTerm into a new sceHttpInit");
    Require(sceHttpTerm_nid_postfix(next) == 0, "sceHttpTerm failed");
}

bool RefusesNull(int context, int* netPoolId, int* sslContextId) {
    try {
        sceHttpGetRegisteredCtxIds(context, netPoolId, sslContextId);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckRegisteredCtxIds() {
    int netPoolId = -1;
    int sslContextId = -1;
    Require(sceHttpGetRegisteredCtxIds(UnknownId, &netPoolId, &sslContextId) == BeforeInit, "sceHttpGetRegisteredCtxIds answered before sceHttpInit");
    const int first = sceHttpInit_nid_postfix(3, 7, 0x10000);
    const int second = sceHttpInit_nid_postfix(5, 9, 0x20000);
    Require(sceHttpGetRegisteredCtxIds(first, &netPoolId, &sslContextId) == 0 && netPoolId == 3 && sslContextId == 7, "the ids given to the first sceHttpInit were not reported");
    Require(sceHttpGetRegisteredCtxIds(second, &netPoolId, &sslContextId) == 0 && netPoolId == 5 && sslContextId == 9, "the ids given to the second sceHttpInit were not reported");
    Require(RefusesNull(second, nullptr, &sslContextId) && RefusesNull(second, &netPoolId, nullptr), "a null output was taken");
    netPoolId = -1;
    sslContextId = -1;
    Require(sceHttpGetRegisteredCtxIds(UnknownId, &netPoolId, &sslContextId) == InvalidId, "sceHttpGetRegisteredCtxIds took an unknown id");
    Require(sceHttpTerm_nid_postfix(first) == 0, "sceHttpTerm failed");
    Require(sceHttpGetRegisteredCtxIds(first, &netPoolId, &sslContextId) == InvalidId, "a terminated context still reported its ids");
    Require(netPoolId == -1 && sslContextId == -1, "a refused query wrote its outputs");
    Require(sceHttpTerm_nid_postfix(second) == 0, "sceHttpTerm failed");
    Require(sceHttpGetRegisteredCtxIds(second, &netPoolId, &sslContextId) == BeforeInit, "sceHttpGetRegisteredCtxIds answered after the last sceHttpTerm");
}

void CheckUnobservedOptions(int context) {
    const int pattern = sceHttpCreateTemplate(context, "anyps5-test", 2, 0);
    const int connection = sceHttpCreateConnectionWithURL(pattern, "http://example.com/", 1);
    const int request = sceHttpCreateRequestWithURL(connection, 0, "http://example.com/a", 0);
    Require(sceHttpAddRequestHeader(request, "X-Test", "value", 0) == 0 && sceHttpRemoveRequestHeader(request, "X-Test") == 0, "offline request header calls failed");
    Require(sceHttpRemoveRequestHeader(UnknownId, nullptr) == 0, "offline header removal started validating arguments");
    Require(sceHttpCacheRedirectedConnectionEnabled(pattern, 0) == 0 && sceHttpCacheRedirectedConnectionEnabled(request, 1) == 0, "sceHttpCacheRedirectedConnectionEnabled refused a live object");
    Require(sceHttpCacheRedirectedConnectionEnabled(UnknownId, 1) == InvalidId, "sceHttpCacheRedirectedConnectionEnabled took an unknown id");
    for (const auto option : {sceHttpsEnableOptionPrivate, sceHttpsDisableOptionPrivate}) {
        Require(option(connection, 0x2dffu) == 0 && option(connection, 0) == 0, "a private SSL option within the mask was refused");
        Require(option(connection, 0x0100u) == 0 && option(connection, 0x2000u) == 0, "a private-only SSL flag was refused");
        Require(option(connection, 0x0200u) == InvalidValue && option(connection, 0x4000u) == InvalidValue, "an SSL flag outside the private mask was taken");
        Require(option(UnknownId, 0x0200u) == InvalidValue, "the SSL flags are not checked before the id");
        Require(option(UnknownId, 0x01u) == InvalidId, "a private SSL option took an unknown id");
    }
    for (const int id : {pattern, connection, request}) {
        Require(sceHttpSetProxy(id, 1, 0, "proxy.example", 3128) == 0, "sceHttpSetProxy refused a live object");
    }
    Require(sceHttpSetProxy(connection, 0, 0, nullptr, 0) == InvalidValue, "sceHttpSetProxy took a null host");
    Require(sceHttpSetProxy(UnknownId, 0, 0, nullptr, 0) == InvalidValue, "the proxy host is not checked before the id");
    Require(sceHttpSetProxy(UnknownId, 1, 1, "proxy.example", 8080) == InvalidId, "sceHttpSetProxy took an unknown id");
    Require(sceHttpAbortRequestForce(request) == 0, "sceHttpAbortRequestForce failed on a live request");
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpDeleteConnection(connection) == 0 && sceHttpDeleteTemplate(pattern) == 0, "delete failed");
}

}

int main() {
    const int context = sceHttpInit_nid_postfix(1, 1, 0x10000);
    Require(context > 0, "sceHttpInit failed");
    CheckAutoRedirect(context);
    CheckAuthEnabled(context);
    CheckCookieEnabled(context);
    CheckNonblock(context);
    CheckAcceptEncodingGzip(context);
    CheckEpoll(context);
    CheckSendFailureIsObservable(context);
    CheckUnobservedOptions(context);
    Require(sceHttpTerm_nid_postfix(context) == 0, "sceHttpTerm failed");
    Require(sceHttpSetDefaultAcceptEncodingGZIPEnabled(context, 1) == BeforeInit, "a gzip default was taken before sceHttpInit");
    CheckTermEndsOptions();
    CheckRegisteredCtxIds();
    std::puts("HTTP option tests passed");
    return 0;
}
