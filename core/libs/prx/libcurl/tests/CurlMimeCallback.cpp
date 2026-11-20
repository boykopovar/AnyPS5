#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <source_location>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
using Socket = SOCKET;
static void CloseSocket(Socket socket) {
    closesocket(socket);
}
#else
using Socket = int;
static void CloseSocket(Socket socket) {
    close(socket);
}
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "failure at %u\n", location.line());
        std::exit(1);
    }
}

struct Source {
    std::string bytes;
    size_t position = 0;
    int reads = 0;
    int seeks = 0;
    std::int64_t lastSeekOffset = -1;
    int lastSeekOrigin = -1;
    int frees = 0;
    bool abortOnRead = false;
};

static size_t APS5_VABI SourceRead(char* buffer, size_t size, size_t count, void* opaque) {
    auto* source = static_cast<Source*>(opaque);
    const auto freesBefore = source->frees;
    ++source->reads;
    if (source->abortOnRead) {
        Require(source->frees == 0);
        return CURL_READFUNC_ABORT;
    }
    const auto length = std::min(size * count, source->bytes.size() - source->position);
    std::memcpy(buffer, source->bytes.data() + source->position, length);
    source->position += length;
    Require(source->frees == freesBefore);
    return length;
}

static int APS5_VABI SourceSeek(void* opaque, std::int64_t offset, int origin) {
    auto* source = static_cast<Source*>(opaque);
    ++source->seeks;
    source->lastSeekOffset = offset;
    source->lastSeekOrigin = origin;
    if (origin != SEEK_SET || offset < 0 || static_cast<size_t>(offset) > source->bytes.size()) return CURL_SEEKFUNC_FAIL;
    source->position = static_cast<size_t>(offset);
    return CURL_SEEKFUNC_OK;
}

static void APS5_VABI SourceFree(void* opaque) {
    ++static_cast<Source*>(opaque)->frees;
}

static curl_mimepart* AddCallbackPart(curl_mime* mime, Source& source) {
    auto* part = curl_mime_addpart_nid_postfix(mime);
    Require(part != nullptr);
    Require(curl_mime_name_nid_postfix(part, "field") == CURLE_OK);
    Require(curl_mime_data_cb_nid_postfix(part, static_cast<std::int64_t>(source.bytes.size()), SourceRead, SourceSeek, SourceFree, &source) == CURLE_OK);
    return part;
}

class Server {
public:
    unsigned short port = 0;

    explicit Server(std::vector<std::string> serverResponses = {"HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n"})
        : responses(std::move(serverResponses)) {
        Require(listener != static_cast<Socket>(-1));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Require(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        Require(listen(listener, 4) == 0);
        socklen_t length = sizeof(address);
        Require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(address.sin_port);
        thread = std::thread([this] { Serve(); });
    }

    ~Server() {
        Requests();
        CloseSocket(listener);
    }

    const std::string& Request() {
        Requests();
        return requests.empty() ? emptyRequest : requests.front();
    }

    const std::vector<std::string>& Requests() {
        if (thread.joinable()) thread.join();
        return requests;
    }

private:
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::vector<std::string> responses;
    std::vector<std::string> requests;
    std::string emptyRequest;
    std::thread thread;

    static void SendAll(Socket client, const std::string& response) {
        std::size_t sent = 0;
        while (sent < response.size()) {
#ifdef _WIN32
            const auto count = send(client, response.data() + sent, static_cast<int>(response.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
            const auto count = send(client, response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
#else
            const auto count = send(client, response.data() + sent, response.size() - sent, 0);
#endif
            if (count <= 0) break;
            sent += static_cast<std::size_t>(count);
        }
    }

    void Serve() {
        for (const auto& response : responses) {
            fd_set readable;
            FD_ZERO(&readable);
#ifdef _WIN32
            constexpr int descriptorCount = 0;
#else
            Require(listener < FD_SETSIZE);
            const int descriptorCount = listener + 1;
#endif
            FD_SET(listener, &readable);
            timeval acceptTimeout{5, 0};
            if (select(descriptorCount, &readable, nullptr, nullptr, &acceptTimeout) <= 0) return;
            const Socket client = accept(listener, nullptr, nullptr);
            if (client == static_cast<Socket>(-1)) return;
#ifdef _WIN32
            const DWORD timeout = 5000;
            Require(setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0);
#else
            const timeval timeout{5, 0};
            Require(setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
#ifdef SO_NOSIGPIPE
            const int noSigPipe = 1;
            Require(setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe)) == 0);
#endif
#endif
            std::string request;
            char buffer[4096];
            std::size_t headerEnd = std::string::npos;
            std::size_t total = 0;
            while (true) {
                if (headerEnd == std::string::npos) {
                    headerEnd = request.find("\r\n\r\n");
                    if (headerEnd != std::string::npos) {
                        std::string headers = request.substr(0, headerEnd);
                        std::transform(headers.begin(), headers.end(), headers.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                        const auto field = headers.find("content-length:");
                        total = headerEnd + 4 + (field == std::string::npos ? 0 : std::strtoul(headers.c_str() + field + 15, nullptr, 10));
                    }
                }
                if (headerEnd != std::string::npos && request.size() >= total) {
                    SendAll(client, response);
                    requests.push_back(std::move(request));
                    break;
                }
                const auto received = recv(client, buffer, sizeof(buffer), 0);
                if (received <= 0) {
                    requests.push_back(std::move(request));
                    CloseSocket(client);
                    return;
                }
                request.append(buffer, static_cast<std::size_t>(received));
            }
            CloseSocket(client);
        }
    }
};

static void TestDuplicateKeepsCallbacksAfterOriginalMimeIsFreed() {
    Source source;
    source.bytes = "guest callback body";
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    AddCallbackPart(mime, source);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);

    auto* copy = curl_easy_duphandle_nid_postfix(easy);
    Require(copy != nullptr);
    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 1);

    Server server;
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";
    Require(curl_easy_setopt_nid_postfix(copy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(copy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(copy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(copy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(copy) == CURLE_OK);
    Require(source.reads > 0);
    Require(server.Request().find("\r\n\r\n" + source.bytes + "\r\n--") != std::string::npos);

    curl_easy_cleanup_nid_postfix(copy);
    Require(source.frees == 2);
    curl_easy_cleanup_nid_postfix(easy);
    Require(source.frees == 2);
}

static void TestReplacedContentFreesOnce() {
    Source source;
    source.bytes = "replaced";
    auto* mime = curl_mime_init_nid_postfix(nullptr);
    Require(mime != nullptr);
    auto* part = AddCallbackPart(mime, source);
    Require(curl_mime_data_nid_postfix(part, "x", 1) == CURLE_OK);
    Require(source.frees == 1);
    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 1);
}

static void TestSubpartsFreeTheirCallbacks() {
    Source source;
    source.bytes = "nested";
    auto* outer = curl_mime_init_nid_postfix(nullptr);
    auto* inner = curl_mime_init_nid_postfix(nullptr);
    Require(outer != nullptr && inner != nullptr);
    AddCallbackPart(inner, source);
    auto* part = curl_mime_addpart_nid_postfix(outer);
    Require(part != nullptr);
    Require(curl_mime_subparts_nid_postfix(part, inner) == CURLE_OK);
    Require(curl_mime_subparts_nid_postfix(part, inner) == CURLE_OK);
    Require(source.frees == 0);
    curl_mime_free_nid_postfix(outer);
    Require(source.frees == 1);
}

static void TestNullReadFunctionKeepsNoCallbacks() {
    Source source;
    auto* mime = curl_mime_init_nid_postfix(nullptr);
    Require(mime != nullptr);
    auto* part = curl_mime_addpart_nid_postfix(mime);
    Require(part != nullptr);
    Require(curl_mime_data_cb_nid_postfix(part, 4, nullptr, SourceSeek, SourceFree, &source) == CURLE_OK);
    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 0);
    Require(curl_mime_data_cb_nid_postfix(nullptr, 4, SourceRead, SourceSeek, SourceFree, &source) == CURLE_BAD_FUNCTION_ARGUMENT);
}

static void TestReadAbortAndSubsequentValidUpload() {
    Server server;
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";

    Source abortSource;
    abortSource.bytes = "payload to abort";
    abortSource.abortOnRead = true;

    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    AddCallbackPart(mime, abortSource);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);

    const auto abortResult = curl_easy_perform_nid_postfix(easy);
    Require(abortResult == CURLE_ABORTED_BY_CALLBACK);
    Require(abortSource.reads > 0);
    Require(abortSource.frees == 0);

    curl_mime_free_nid_postfix(mime);
    Require(abortSource.frees == 1);
    curl_easy_cleanup_nid_postfix(easy);
    Require(abortSource.frees == 1);

    Server validServer;
    const std::string validUrl = "http://127.0.0.1:" + std::to_string(validServer.port) + "/";
    Source validSource;
    validSource.bytes = "valid payload after abort";
    auto* validEasy = curl_easy_init_nid_postfix();
    Require(validEasy != nullptr);
    auto* validMime = curl_mime_init_nid_postfix(validEasy);
    Require(validMime != nullptr);
    AddCallbackPart(validMime, validSource);
    Require(curl_easy_setopt_nid_postfix(validEasy, CURLOPT_MIMEPOST, validMime) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(validEasy, CURLOPT_URL, validUrl.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(validEasy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(validEasy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(validEasy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);

    const auto validResult = curl_easy_perform_nid_postfix(validEasy);
    Require(validResult == CURLE_OK);
    Require(validSource.reads > 0);
    Require(validSource.frees == 0);
    Require(validServer.Request().find("\r\n\r\n" + validSource.bytes + "\r\n--") != std::string::npos);

    curl_mime_free_nid_postfix(validMime);
    Require(validSource.frees == 1);
    curl_easy_cleanup_nid_postfix(validEasy);
    Require(validSource.frees == 1);
}

struct HeaderAllocationTracker {
    std::mutex mutex;
    std::set<const void*> livePointers;

    void Record(const void* pointer) {
        if (!pointer) return;
        std::lock_guard lock(mutex);
        Require(livePointers.insert(pointer).second);
    }

    void Forget(const void* pointer) {
        if (!pointer) return;
        std::lock_guard lock(mutex);
        Require(livePointers.erase(pointer) == 1);
    }

    bool IsLive(const void* pointer) {
        std::lock_guard lock(mutex);
        return livePointers.contains(pointer);
    }

    std::set<const void*> Snapshot() {
        std::lock_guard lock(mutex);
        return livePointers;
    }
};

static HeaderAllocationTracker headerTracker;

static void* APS5_VABI TrackMalloc(size_t size) {
    void* pointer = std::malloc(size == 0 ? 1 : size);
    headerTracker.Record(pointer);
    return pointer;
}

static void APS5_VABI TrackFree(void* pointer) {
    headerTracker.Forget(pointer);
    std::free(pointer);
}

static void* APS5_VABI TrackRealloc(void* pointer, size_t size) {
    if (pointer == nullptr) {
        void* allocated = std::malloc(size == 0 ? 1 : size);
        headerTracker.Record(allocated);
        return allocated;
    }
    if (size == 0) {
        headerTracker.Forget(pointer);
        std::free(pointer);
        return nullptr;
    }
    headerTracker.Forget(pointer);
    void* reallocated = std::realloc(pointer, size);
    if (reallocated) {
        headerTracker.Record(reallocated);
    } else {
        headerTracker.Record(pointer);
    }
    return reallocated;
}

static char* APS5_VABI TrackStrdup(const char* value) {
    if (!value) return nullptr;
    const size_t length = std::strlen(value) + 1;
    char* copy = static_cast<char*>(std::malloc(length));
    if (copy) {
        std::memcpy(copy, value, length);
        headerTracker.Record(copy);
    }
    return copy;
}

static void* APS5_VABI TrackCalloc(size_t count, size_t size) {
    void* pointer = std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    headerTracker.Record(pointer);
    return pointer;
}

static std::vector<const void*> CollectSlistAllocations(const CurlSlist* list) {
    std::vector<const void*> pointers;
    for (const CurlSlist* node = list; node != nullptr; node = node->next) {
        pointers.push_back(node);
        pointers.push_back(node->data);
    }
    return pointers;
}

static bool AllPointersLive(const std::vector<const void*>& pointers) {
    for (const void* pointer : pointers) {
        if (!headerTracker.IsLive(pointer)) return false;
    }
    return true;
}

static bool NoPointersLive(const std::vector<const void*>& pointers) {
    for (const void* pointer : pointers) {
        if (headerTracker.IsLive(pointer)) return false;
    }
    return true;
}

static CurlSlist* BuildGuestHeaderList(const std::vector<const char*>& entries) {
    CurlSlist* list = nullptr;
    for (const char* entry : entries) {
        CurlSlist* next = curl_slist_append(list, entry);
        Require(next != nullptr);
        list = next;
    }
    return list;
}

static void ConfigureLoopbackMimeEasy(CURL* easy, curl_mime* mime) {
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);
}

static std::string PerformLoopbackMimeUpload(CURL* easy) {
    Server server;
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
    return server.Request();
}

static void Test307RedirectRewindsMimeCallback() {
    Source source;
    source.bytes = "redirected guest callback body";

    Server server({
        "HTTP/1.1 307 Temporary Redirect\r\nLocation: /redirected\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
    });
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/initial";

    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    AddCallbackPart(mime, source);
    ConfigureLoopbackMimeEasy(easy, mime);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_FOLLOWLOCATION, std::int64_t{1}) == CURLE_OK);

    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);

    std::int64_t responseCode = 0;
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_RESPONSE_CODE, &responseCode) == CURLE_OK);
    Require(responseCode == 200);
    std::int64_t redirectCount = 0;
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_REDIRECT_COUNT, &redirectCount) == CURLE_OK);
    Require(redirectCount == 1);

    Require(source.reads >= 2);
    Require(source.seeks == 1);
    Require(source.lastSeekOffset == 0);
    Require(source.lastSeekOrigin == SEEK_SET);
    Require(source.position == source.bytes.size());
    Require(source.frees == 0);

    const auto& requests = server.Requests();
    Require(requests.size() == 2);
    Require(requests[0].starts_with("POST /initial "));
    Require(requests[1].starts_with("POST /redirected "));
    const auto firstHeaderEnd = requests[0].find("\r\n\r\n");
    const auto secondHeaderEnd = requests[1].find("\r\n\r\n");
    Require(firstHeaderEnd != std::string::npos && secondHeaderEnd != std::string::npos);
    const auto firstBody = requests[0].substr(firstHeaderEnd + 4);
    const auto secondBody = requests[1].substr(secondHeaderEnd + 4);
    Require(firstBody == secondBody);
    Require(firstBody.find("\r\n\r\n" + source.bytes + "\r\n--") != std::string::npos);

    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 1);
    curl_easy_cleanup_nid_postfix(easy);
    Require(source.frees == 1);
}

static void TestMimeHeadersBorrowed() {
    const auto baseline = headerTracker.Snapshot();

    Source source;
    source.bytes = "borrowed header body";
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    auto* part = AddCallbackPart(mime, source);
    ConfigureLoopbackMimeEasy(easy, mime);

    CurlSlist* firstList = BuildGuestHeaderList({"X-Borrowed-One: first", "X-Borrowed-Two: alpha"});
    const auto firstPointers = CollectSlistAllocations(firstList);
    Require(AllPointersLive(firstPointers));

    Require(curl_mime_headers_nid_postfix(part, firstList, 0) == CURLE_OK);
    const std::string firstRequest = PerformLoopbackMimeUpload(easy);
    Require(firstRequest.find("X-Borrowed-One: first\r\n") != std::string::npos);
    Require(firstRequest.find("X-Borrowed-Two: alpha\r\n") != std::string::npos);
    Require(firstRequest.find("\r\n\r\n" + source.bytes + "\r\n--") != std::string::npos);
    Require(AllPointersLive(firstPointers));

    CurlSlist* secondList = BuildGuestHeaderList({"X-Borrowed-One: second"});
    const auto secondPointers = CollectSlistAllocations(secondList);
    Require(AllPointersLive(secondPointers));

    Require(curl_mime_headers_nid_postfix(part, secondList, 0) == CURLE_OK);
    Require(AllPointersLive(firstPointers));
    curl_slist_free_all(firstList);
    Require(NoPointersLive(firstPointers));

    const std::string secondRequest = PerformLoopbackMimeUpload(easy);
    Require(secondRequest.find("X-Borrowed-One: second\r\n") != std::string::npos);
    Require(secondRequest.find("X-Borrowed-One: first") == std::string::npos);
    Require(secondRequest.find("X-Borrowed-Two: alpha") == std::string::npos);
    Require(AllPointersLive(secondPointers));

    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 1);
    Require(AllPointersLive(secondPointers));

    curl_slist_free_all(secondList);
    Require(NoPointersLive(secondPointers));

    curl_easy_cleanup_nid_postfix(easy);
    Require(headerTracker.Snapshot() == baseline);
}

static void TestMimeHeadersOwned() {
    const auto baseline = headerTracker.Snapshot();

    Source source;
    source.bytes = "owned header body";
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    auto* part = AddCallbackPart(mime, source);
    ConfigureLoopbackMimeEasy(easy, mime);

    CurlSlist* firstList = BuildGuestHeaderList({"X-Owned-One: first", "X-Owned-Two: gamma"});
    const auto firstPointers = CollectSlistAllocations(firstList);
    Require(AllPointersLive(firstPointers));

    Require(curl_mime_headers_nid_postfix(part, firstList, 1) == CURLE_OK);
    const std::string firstRequest = PerformLoopbackMimeUpload(easy);
    Require(firstRequest.find("X-Owned-One: first\r\n") != std::string::npos);
    Require(firstRequest.find("X-Owned-Two: gamma\r\n") != std::string::npos);
    Require(firstRequest.find("\r\n\r\n" + source.bytes + "\r\n--") != std::string::npos);
    Require(AllPointersLive(firstPointers));

    CurlSlist* secondList = BuildGuestHeaderList({"X-Owned-One: second"});
    const auto secondPointers = CollectSlistAllocations(secondList);
    Require(AllPointersLive(secondPointers));

    Require(curl_mime_headers_nid_postfix(part, secondList, 1) == CURLE_OK);
    Require(NoPointersLive(firstPointers));
    Require(AllPointersLive(secondPointers));

    const std::string secondRequest = PerformLoopbackMimeUpload(easy);
    Require(secondRequest.find("X-Owned-One: second\r\n") != std::string::npos);
    Require(secondRequest.find("X-Owned-One: first") == std::string::npos);
    Require(secondRequest.find("X-Owned-Two: gamma") == std::string::npos);
    Require(AllPointersLive(secondPointers));

    curl_mime_free_nid_postfix(mime);
    Require(source.frees == 1);
    Require(NoPointersLive(secondPointers));

    curl_easy_cleanup_nid_postfix(easy);
    Require(headerTracker.Snapshot() == baseline);
}

int main() {
#ifdef _WIN32
    WSADATA data;
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    TestDuplicateKeepsCallbacksAfterOriginalMimeIsFreed();
    TestReplacedContentFreesOnce();
    TestSubpartsFreeTheirCallbacks();
    TestNullReadFunctionKeepsNoCallbacks();
    TestReadAbortAndSubsequentValidUpload();
    Test307RedirectRewindsMimeCallback();
    curl_global_cleanup_nid_postfix();

    Require(curl_global_init_mem_nid_postfix(
        CURL_GLOBAL_DEFAULT, TrackMalloc, TrackFree, TrackRealloc, TrackStrdup, TrackCalloc) == CURLE_OK);
    TestMimeHeadersBorrowed();
    TestMimeHeadersOwned();
    curl_global_cleanup_nid_postfix();
    Require(headerTracker.Snapshot().empty());
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
