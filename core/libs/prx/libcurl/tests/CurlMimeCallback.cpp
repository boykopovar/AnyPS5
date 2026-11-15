#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
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
#include <source_location>
#include <string>
#include <thread>

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
    int frees = 0;
};

static size_t APS5_VABI SourceRead(char* buffer, size_t size, size_t count, void* opaque) {
    auto* source = static_cast<Source*>(opaque);
    ++source->reads;
    const auto length = std::min(size * count, source->bytes.size() - source->position);
    std::memcpy(buffer, source->bytes.data() + source->position, length);
    source->position += length;
    return length;
}

static int APS5_VABI SourceSeek(void* opaque, std::int64_t offset, int origin) {
    auto* source = static_cast<Source*>(opaque);
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

struct Server {
    Socket listener;
    unsigned short port = 0;
    std::string request;
    std::thread thread;

    Server() {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Require(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        Require(listen(listener, 1) == 0);
        socklen_t length = sizeof(address);
        Require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(address.sin_port);
        thread = std::thread([this] { Serve(); });
    }

    void Serve() {
        const Socket client = accept(listener, nullptr, nullptr);
        if (client == static_cast<Socket>(-1)) return;
        char buffer[4096];
        size_t headerEnd = std::string::npos;
        size_t total = 0;
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
            if (headerEnd != std::string::npos && request.size() >= total) break;
            const auto received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) break;
            request.append(buffer, static_cast<size_t>(received));
        }
        const char response[] = "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(client, response, sizeof(response) - 1, 0);
        CloseSocket(client);
    }

    ~Server() {
        if (thread.joinable()) thread.join();
        CloseSocket(listener);
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
    Require(server.request.find("\r\n\r\n" + source.bytes + "\r\n--") != std::string::npos);

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
    curl_global_cleanup_nid_postfix();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
