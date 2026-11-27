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
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <source_location>
#include <string>
#include <thread>
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

#ifdef _WIN32
extern "C" int CurlHostNonblock_nid_no_patch(curl_socket_t socket, int nonblocking);
extern "C" int CurlSocketNonblock_nid_no_patch(curl_socket_t socket, int nonblocking) {
    return CurlHostNonblock_nid_no_patch(socket, nonblocking);
}
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "failure at %u\n", location.line());
        std::exit(1);
    }
}

static size_t APS5_VABI Write(char* buffer, size_t size, size_t count, void* opaque) {
    static_cast<std::string*>(opaque)->append(buffer, size * count);
    return size * count;
}

struct Input {
    std::string bytes;
    size_t position = 0;
};

static size_t APS5_VABI Read(char* buffer, size_t size, size_t count, void* opaque) {
    auto* input = static_cast<Input*>(opaque);
    auto length = std::min(size * count, input->bytes.size() - input->position);
    std::memcpy(buffer, input->bytes.data() + input->position, length);
    input->position += length;
    return length;
}

static int APS5_VABI Abort(void* opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    ++*static_cast<int*>(opaque);
    return 1;
}

static int APS5_VABI Timer(CURLM*, int64_t timeout, void* opaque) {
    Require(timeout >= -1);
    ++*static_cast<int*>(opaque);
    return 0;
}

static std::string StreamBytes(std::FILE* stream) {
    std::fflush(stream);
    std::rewind(stream);
    std::string result;
    char buffer[256];
    while (auto count = std::fread(buffer, 1, sizeof(buffer), stream)) result.append(buffer, count);
    return result;
}

static std::string FileUrl(const std::filesystem::path& path) {
    auto text = path.generic_string();
    return "file://" + text;
}

struct Hsts {
    int reads = 0;
    int writes = 0;
    bool served = false;
    std::string written;
};

static CURLSTScode APS5_VABI HstsRead(CURL*, curl_hstsentry* entry, void* opaque) {
    auto* hsts = static_cast<Hsts*>(opaque);
    ++hsts->reads;
    if (hsts->served) return CURLSTS_DONE;
    hsts->served = true;
    std::strcpy(entry->name, "hsts.example");
    std::strcpy(entry->expire, "20991231 23:59:59");
    return CURLSTS_OK;
}

static CURLSTScode APS5_VABI HstsWrite(CURL*, curl_hstsentry* entry, curl_index*, void* opaque) {
    auto* hsts = static_cast<Hsts*>(opaque);
    ++hsts->writes;
    hsts->written = entry->name;
    return CURLSTS_OK;
}

static void TestHstsThroughDuplicates(const std::string& url) {
    Hsts hsts;
    std::string output;
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HSTS_CTRL, int64_t{CURLHSTS_ENABLE}) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HSTSREADFUNCTION, HstsRead) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HSTSREADDATA, &hsts) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HSTSWRITEFUNCTION, HstsWrite) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HSTSWRITEDATA, &hsts) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK && !output.empty());
    Require(hsts.reads == 2);
    for (int round = 0; round < 3; ++round) {
        hsts.served = false;
        const int reads = hsts.reads;
        const int writes = hsts.writes;
        auto* duplicate = curl_easy_duphandle_nid_postfix(easy);
        Require(duplicate);
        Require(hsts.reads == reads + 2);
        hsts.written.clear();
        curl_easy_cleanup_nid_postfix(duplicate);
        Require(hsts.writes == writes + 1 && hsts.written == "hsts.example");
    }
    output.clear();
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK && !output.empty());
    const int writes = hsts.writes;
    hsts.written.clear();
    curl_easy_cleanup_nid_postfix(easy);
    Require(hsts.writes == writes + 1 && hsts.written == "hsts.example");
}

class Server {
public:
    unsigned short port = 0;

    Server() {
        Require(listener != static_cast<Socket>(-1));
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

    ~Server() {
        Request();
        CloseSocket(listener);
    }

    const std::string& Request() {
        if (thread.joinable()) thread.join();
        return request;
    }

private:
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::string request;
    std::thread thread;

    void Serve() {
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
                const char response[] = "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n";
#ifdef MSG_NOSIGNAL
                send(client, response, sizeof(response) - 1, MSG_NOSIGNAL);
#else
                send(client, response, sizeof(response) - 1, 0);
#endif
                break;
            }
            const auto received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) break;
            request.append(buffer, static_cast<std::size_t>(received));
        }
        CloseSocket(client);
    }
};

struct Base64MimeCase {
    const char* name;
    const char* filename;
    std::string rawBytes;
};

static std::string LowercaseAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

static std::vector<std::string> SplitLinesCrlf(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t position = 0;
    while (true) {
        const std::size_t next = text.find("\r\n", position);
        const std::string line = next == std::string::npos ? text.substr(position) : text.substr(position, next - position);
        Require(line.find('\r') == std::string::npos);
        Require(line.find('\n') == std::string::npos);
        lines.push_back(line);
        if (next == std::string::npos) break;
        position = next + 2;
    }
    return lines;
}

static bool HasExactHeader(const std::vector<std::string>& lines, const std::string& expected) {
    const std::string needle = LowercaseAscii(expected);
    for (const auto& line : lines) {
        if (LowercaseAscii(line) == needle) return true;
    }
    return false;
}

static std::string ExtractMultipartBoundary(const std::string& httpHeaders) {
    for (const auto& line : SplitLinesCrlf(httpHeaders)) {
        const std::string lower = LowercaseAscii(line);
        if (!lower.starts_with("content-type:")) continue;
        Require(lower.find("multipart/form-data") != std::string::npos);
        const std::size_t marker = lower.find("boundary=");
        Require(marker != std::string::npos);
        std::size_t start = marker + 9;
        std::size_t end = line.find(';', start);
        if (end == std::string::npos) end = line.size();
        while (start < end && (line[start] == ' ' || line[start] == '\t')) ++start;
        while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
        if (end - start >= 2 && line[start] == '"' && line[end - 1] == '"') {
            ++start;
            --end;
        }
        const std::string boundary = line.substr(start, end - start);
        Require(!boundary.empty());
        return boundary;
    }
    Require(false);
    return {};
}

static std::vector<std::string> SplitMultipartParts(const std::string& httpBody, const std::string& boundary) {
    const std::string open = "--" + boundary + "\r\n";
    const std::string close = "\r\n--" + boundary + "--\r\n";
    const std::string separator = "\r\n--" + boundary + "\r\n";
    Require(httpBody.starts_with(open));
    Require(httpBody.ends_with(close));
    Require(httpBody.size() >= open.size() + close.size());
    const std::string inner = httpBody.substr(open.size(), httpBody.size() - open.size() - close.size());
    std::vector<std::string> parts;
    std::size_t position = 0;
    while (true) {
        const std::size_t next = inner.find(separator, position);
        if (next == std::string::npos) {
            parts.push_back(inner.substr(position));
            break;
        }
        parts.push_back(inner.substr(position, next - position));
        position = next + separator.size();
    }
    return parts;
}

static void WriteGuestFile(const std::string& path, const std::string& text) {
    auto* file = fopen_nid_postfix(path.c_str(), "wb");
    Require(file != nullptr);
    Require(fwrite_nid_postfix(text.data(), 1, text.size(), file) == text.size());
    Require(fclose_nid_postfix(file) == 0);
}

static std::string ReadGuestFile(const std::string& path) {
    auto* file = fopen_nid_postfix(path.c_str(), "rb");
    if (file == nullptr) return {};
    std::string text;
    char buffer[256];
    while (const auto count = fread_nid_postfix(buffer, 1, sizeof(buffer), file)) text.append(buffer, count);
    Require(fclose_nid_postfix(file) == 0);
    return text;
}

static CURLcode PerformTlsWithCaFile(const std::string& caFile) {
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    Server server;
    const std::string url = "https://127.0.0.1:" + std::to_string(server.port) + "/";
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CAINFO, caFile.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT_MS, static_cast<std::int64_t>(500)) == CURLE_OK);
    const auto result = static_cast<CURLcode>(curl_easy_perform_nid_postfix(easy));
    curl_easy_cleanup_nid_postfix(easy);
    return result;
}

static void TestGuestFileOptions() {
    const std::string directory = "/anyps5-curl-files-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(mkdir_nid_postfix(directory.c_str(), 0755) == 0);
    const std::string cookies = directory + "/cookies.txt";
    const std::string netrc = directory + "/netrc";
    const std::string jar = directory + "/jar.txt";
    const std::string ca = directory + "/ca.pem";
    WriteGuestFile(cookies, "127.0.0.1\tFALSE\t/\tFALSE\t0\tguest\tcookie\n");
    WriteGuestFile(netrc, "machine 127.0.0.1 login guest password secret\n");
    WriteGuestFile(ca,
        "-----BEGIN CERTIFICATE-----\n"
        "MIIBjDCCATOgAwIBAgIUJTITjGabd7w+NEvUhe6a72xPtSQwCgYIKoZIzj0EAwIw\n"
        "GzEZMBcGA1UEAwwQYW55cHM1LWN1cmwtdGVzdDAgFw0yNjExMjAyMDM5MDZaGA8y\n"
        "MTI2MTAyNzIwMzkwNlowGzEZMBcGA1UEAwwQYW55cHM1LWN1cmwtdGVzdDBZMBMG\n"
        "ByqGSM49AgEGCCqGSM49AwEHA0IABDr+cTAWtUCNcXKv290JZBGDYv2yux/VNEVu\n"
        "MMqJbYeVH8+TAU8XTI4CK2genbyM8CBDwd+DXKxhwgxHhtT9ulWjUzBRMB0GA1Ud\n"
        "DgQWBBRAKdeiE2w3h4+DwCZ4hSoxFqazIjAfBgNVHSMEGDAWgBRAKdeiE2w3h4+D\n"
        "wCZ4hSoxFqazIjAPBgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMCA0cAMEQCIBA5\n"
        "l4vQXZ+bne6vDMVWtFscwgmhm6PSGzs4R16mgUUaAiATobfFR+KRSzNwnyp82urt\n"
        "DJ+4B2KsL1vGfpZv9qJJ0Q==\n"
        "-----END CERTIFICATE-----\n");

    {
        auto* easy = curl_easy_init_nid_postfix();
        Require(easy != nullptr);
        Server server;
        const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";
        std::string responseBody;
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_COOKIEFILE, cookies.c_str()) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NETRC, static_cast<std::int64_t>(CURL_NETRC_REQUIRED)) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NETRC_FILE, netrc.c_str()) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &responseBody) == CURLE_OK);
        Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
        curl_easy_cleanup_nid_postfix(easy);
        const std::string& request = server.Request();
        const auto headers = SplitLinesCrlf(request.substr(0, request.find("\r\n\r\n")));
        Require(HasExactHeader(headers, "Cookie: guest=cookie"));
        Require(HasExactHeader(headers, "Authorization: Basic Z3Vlc3Q6c2VjcmV0"));
    }

    {
        auto* easy = curl_easy_init_nid_postfix();
        Require(easy != nullptr);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_COOKIEJAR, jar.c_str()) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_COOKIELIST, "127.0.0.1\tFALSE\t/\tFALSE\t0\tjar\tvalue") == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_COOKIELIST, "FLUSH") == CURLE_OK);
        Require(ReadGuestFile(jar).find("127.0.0.1\tFALSE\t/\tFALSE\t0\tjar\tvalue") != std::string::npos);
        curl_easy_cleanup_nid_postfix(easy);
    }

    Require(PerformTlsWithCaFile(directory + "/missing.pem") == CURLE_SSL_CACERT_BADFILE);
    const auto trusted = PerformTlsWithCaFile(ca);
    Require(trusted != CURLE_OK && trusted != CURLE_SSL_CACERT_BADFILE);

    for (const auto& file : {cookies, netrc, jar, ca}) Require(remove_nid_postfix(file.c_str()) == 0);
    Require(rmdir_nid_postfix(directory.c_str()) == 0);
}

static std::string MakeBinaryPayload(std::size_t length, unsigned char seed) {
    std::string bytes;
    bytes.reserve(length);
    constexpr unsigned char required[] = {0x00, 0xFF, 0x80, 0x01, 0x7F, 0xFE};
    for (std::size_t index = 0; index < length; ++index) {
        const unsigned char value = index < sizeof(required)
            ? required[index]
            : static_cast<unsigned char>((index * 37 + seed) & 0xFF);
        bytes.push_back(static_cast<char>(value));
    }
    return bytes;
}

static std::string ExpectedMimeBase64(const std::string& input) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    std::size_t lineLength = 0;
    for (std::size_t index = 0; index < input.size(); index += 3) {
        if (lineLength == 76) {
            encoded += "\r\n";
            lineLength = 0;
        }
        const unsigned char b0 = static_cast<unsigned char>(input[index]);
        const bool hasB1 = index + 1 < input.size();
        const bool hasB2 = index + 2 < input.size();
        const unsigned char b1 = hasB1 ? static_cast<unsigned char>(input[index + 1]) : 0;
        const unsigned char b2 = hasB2 ? static_cast<unsigned char>(input[index + 2]) : 0;
        const unsigned int triple = (static_cast<unsigned int>(b0) << 16) |
            (static_cast<unsigned int>(b1) << 8) | static_cast<unsigned int>(b2);
        encoded.push_back(alphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(alphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(hasB1 ? alphabet[(triple >> 6) & 0x3F] : '=');
        encoded.push_back(hasB2 ? alphabet[triple & 0x3F] : '=');
        lineLength += 4;
    }
    return encoded;
}

static void VerifyBase64Part(const std::string& rawPart, const Base64MimeCase& expected) {
    const std::size_t headerEnd = rawPart.find("\r\n\r\n");
    Require(headerEnd != std::string::npos);
    const auto headerLines = SplitLinesCrlf(rawPart.substr(0, headerEnd));
    Require(HasExactHeader(headerLines,
        std::string("Content-Disposition: form-data; name=\"") + expected.name + "\"; filename=\"" + expected.filename + "\""));
    Require(HasExactHeader(headerLines, "Content-Type: application/octet-stream"));
    Require(HasExactHeader(headerLines, "Content-Transfer-Encoding: base64"));

    const std::string body = rawPart.substr(headerEnd + 4);
    const auto bodyLines = SplitLinesCrlf(body);
    Require(!bodyLines.empty());
    Require(bodyLines.size() == (expected.rawBytes.size() + 56) / 57);
    for (std::size_t index = 0; index + 1 < bodyLines.size(); ++index) {
        Require(bodyLines[index].size() == 76);
    }
    Require(!bodyLines.back().empty() && bodyLines.back().size() <= 76);
    for (const auto& line : bodyLines) {
        Require(line.size() % 4 == 0);
    }
    const std::size_t remainder = expected.rawBytes.size() % 3;
    if (remainder == 0) {
        Require(!body.ends_with('='));
    } else if (remainder == 1) {
        Require(body.ends_with("=="));
    } else {
        Require(body.ends_with('=') && !body.ends_with("=="));
    }
    Require(body == ExpectedMimeBase64(expected.rawBytes));
}

static void TestMimeBase64Transfer() {
    const std::vector<Base64MimeCase> cases{
        {"wrapped", "wrapped.bin", MakeBinaryPayload(120, 17)},
        {"pad2", "pad2.bin", MakeBinaryPayload(4, 0)},
        {"pad1", "pad1.bin", MakeBinaryPayload(65, 91)},
    };

    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);

    for (const auto& item : cases) {
        auto* part = curl_mime_addpart_nid_postfix(mime);
        Require(part != nullptr);
        Require(curl_mime_name_nid_postfix(part, item.name) == CURLE_OK);
        Require(curl_mime_filename_nid_postfix(part, item.filename) == CURLE_OK);
        Require(curl_mime_type_nid_postfix(part, "application/octet-stream") == CURLE_OK);
        Require(curl_mime_data_nid_postfix(part, item.rawBytes.data(), item.rawBytes.size()) == CURLE_OK);
        Require(curl_mime_encoder_nid_postfix(part, "base64") == CURLE_OK);
    }

    Server server;
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";
    std::string responseBody;
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &responseBody) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);

    const std::string& request = server.Request();
    const std::size_t requestHeaderEnd = request.find("\r\n\r\n");
    Require(requestHeaderEnd != std::string::npos);
    const std::string httpHeaders = request.substr(0, requestHeaderEnd);
    const std::string httpBody = request.substr(requestHeaderEnd + 4);
    const std::string boundary = ExtractMultipartBoundary(httpHeaders);
    const auto parts = SplitMultipartParts(httpBody, boundary);
    Require(parts.size() == cases.size());
    for (std::size_t index = 0; index < cases.size(); ++index) {
        VerifyBase64Part(parts[index], cases[index]);
    }

    curl_mime_free_nid_postfix(mime);
    curl_easy_cleanup_nid_postfix(easy);
}

static void TestMimeFileGuestPath() {
    const std::string name = "anyps5-curl-mime-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    const std::string guestPath = "/" + name;
    const std::string contents("guest file\0bytes\xff", 17);
    auto* file = fopen_nid_postfix(guestPath.c_str(), "wb");
    Require(file != nullptr);
    Require(fwrite_nid_postfix(contents.data(), 1, contents.size(), file) == contents.size());
    Require(fclose_nid_postfix(file) == 0);

    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    auto* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    auto* part = curl_mime_addpart_nid_postfix(mime);
    Require(part != nullptr);
    Require(curl_mime_name_nid_postfix(part, "upload") == CURLE_OK);
    Require(curl_mime_filedata_nid_postfix(part, guestPath.c_str()) == CURLE_OK);
    auto* missing = curl_mime_addpart_nid_postfix(mime);
    Require(missing != nullptr);
    Require(curl_mime_filedata_nid_postfix(missing, ("/" + name + "-missing").c_str()) == CURLE_READ_ERROR);
    Require(curl_mime_filedata_nid_postfix(missing, ("/" + std::string(300, 'n')).c_str()) == CURLE_READ_ERROR);
    Require(curl_mime_data_nid_postfix(missing, "replaced", CURL_ZERO_TERMINATED) == CURLE_OK);
    Require(curl_mime_name_nid_postfix(missing, "replaced") == CURLE_OK);
    Require(curl_mime_filename_nid_postfix(missing, nullptr) == CURLE_OK);

    Server server;
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/";
    std::string responseBody;
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOPROXY, "*") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HTTP_VERSION, static_cast<std::int64_t>(CURL_HTTP_VERSION_1_0)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT, static_cast<std::int64_t>(10)) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &responseBody) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);

    const std::string& request = server.Request();
    const std::size_t requestHeaderEnd = request.find("\r\n\r\n");
    Require(requestHeaderEnd != std::string::npos);
    const auto parts = SplitMultipartParts(request.substr(requestHeaderEnd + 4), ExtractMultipartBoundary(request.substr(0, requestHeaderEnd)));
    Require(parts.size() == 2);
    const std::size_t headerEnd = parts[0].find("\r\n\r\n");
    Require(headerEnd != std::string::npos);
    Require(HasExactHeader(SplitLinesCrlf(parts[0].substr(0, headerEnd)), "Content-Disposition: form-data; name=\"upload\"; filename=\"" + name + "\""));
    Require(parts[0].substr(headerEnd + 4) == contents);

    curl_mime_free_nid_postfix(mime);
    curl_easy_cleanup_nid_postfix(easy);
    Require(remove_nid_postfix(guestPath.c_str()) == 0);
}

int main() {
#ifdef _WIN32
    WSADATA data;
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    auto path = std::filesystem::temp_directory_path() /
        ("anyps5-curl-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::string bytes("transfer bytes\0with binary\xff", 27);
    {
        std::ofstream file(path, std::ios::binary);
        file.write(bytes.data(), bytes.size());
        Require(file.good());
    }
    auto url = FileUrl(path);
    TestHstsThroughDuplicates(url);
    TestGuestFileOptions();
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy);
    std::string output;
    std::string headers;
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HEADERDATA, &headers) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_HEADERFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_PRIVATE, &path) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
    Require(output == bytes);
    Require(headers.starts_with("Content-Length: " + std::to_string(bytes.size()) + "\r\nAccept-ranges: bytes\r\nLast-Modified: "));
    Require(headers.ends_with(" GMT\r\n\r\n"));
    int64_t response = -1;
    curl_off_t downloaded = -1;
    char* effective = nullptr;
    char* content = reinterpret_cast<char*>(1);
    void* privateData = nullptr;
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_RESPONSE_CODE, &response) == CURLE_OK && response == 0);
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_SIZE_DOWNLOAD_T, &downloaded) == CURLE_OK && downloaded == bytes.size());
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK);
    Require(effective == url);
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_CONTENT_TYPE, &content) == CURLE_OK && !content);
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_PRIVATE, &privateData) == CURLE_OK && privateData == &path);
    auto* duplicate = curl_easy_duphandle_nid_postfix(easy);
    Require(duplicate);
    curl_easy_cleanup_nid_postfix(easy);
    output.clear();
    headers.clear();
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK && output == bytes);
    auto* multi = curl_multi_init_nid_postfix();
    Require(multi);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_PIPELINING_SITE_BL, nullptr) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_PIPELINING_SERVER_BL, nullptr) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE, curl_off_t{1} << 40) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE, curl_off_t{1} << 40) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_LASTENTRY) == CURLM_UNKNOWN_OPTION);
    int timerCalls = 0;
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_TIMERDATA, &timerCalls) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(multi, CURLMOPT_TIMERFUNCTION, Timer) == CURLM_OK);
    output.clear();
    Require(curl_multi_add_handle_nid_postfix(multi, duplicate) == CURLM_OK);
    int running;
    do {
        Require(curl_multi_perform_nid_postfix(multi, &running) == CURLM_OK);
        if (running) Require(curl_multi_wait_nid_postfix(multi, nullptr, 0, 10, nullptr) == CURLM_OK);
    } while (running);
    int remaining;
    auto* message = curl_multi_info_read_nid_postfix(multi, &remaining);
    Require(message && message->msg == CURLMSG_DONE && message->data.result == CURLE_OK);
    Require(output == bytes && timerCalls > 0);
    Require(curl_multi_remove_handle_nid_postfix(multi, duplicate) == CURLM_OK);
    Require(curl_multi_cleanup_nid_postfix(multi) == CURLM_OK);
    int abortCalls = 0;
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_XFERINFODATA, &abortCalls) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_XFERINFOFUNCTION, Abort) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_NOPROGRESS, int64_t{0}) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_ABORTED_BY_CALLBACK && abortCalls > 0);
    curl_easy_reset_nid_postfix(duplicate);
    auto* native = std::fopen((path.string() + "-native").c_str(), "w+b");
    Require(native);
    FileStream guest(native);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEDATA, &guest) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK);
    Require(StreamBytes(native) == bytes);
    auto* second = std::fopen((path.string() + "-second").c_str(), "w+b");
    Require(second);
    FileStream secondGuest(second);
    output.clear();
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEFUNCTION, nullptr) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEDATA, &secondGuest) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK && output.empty());
    Require(StreamBytes(second) == bytes);
    std::fclose(native);
    std::fclose(second);
    curl_easy_reset_nid_postfix(duplicate);
    auto uploadPath = path;
    uploadPath += "-upload";
    auto uploadUrl = FileUrl(uploadPath);
    Input input{bytes};
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_URL, uploadUrl.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_UPLOAD, int64_t{1}) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_READFUNCTION, Read) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_READDATA, &input) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(bytes.size())) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK && input.position == bytes.size());
    std::ifstream uploaded(uploadPath, std::ios::binary);
    Require(std::string(std::istreambuf_iterator<char>(uploaded), {}) == bytes);
    uploaded.close();
    auto* inputFile = std::fopen((path.string() + "-inputFile").c_str(), "w+b");
    Require(inputFile);
    Require(std::fwrite(bytes.data(), 1, bytes.size(), inputFile) == bytes.size());
    std::rewind(inputFile);
    FileStream guestInput(inputFile);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_READFUNCTION, nullptr) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_READDATA, &guestInput) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK);
    std::ifstream defaultUploaded(uploadPath, std::ios::binary);
    Require(std::string(std::istreambuf_iterator<char>(defaultUploaded), {}) == bytes);
    defaultUploaded.close();
    std::fclose(inputFile);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_READDATA, nullptr) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_ABORTED_BY_CALLBACK);
    curl_easy_reset_nid_postfix(duplicate);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEDATA, nullptr) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_WRITE_ERROR);
    curl_easy_reset_nid_postfix(duplicate);
    output.clear();
    headers.clear();
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_HEADERDATA, &headers) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK && output == bytes);
    Require(headers.starts_with("Content-Length: "));
    Require(curl_easy_setopt_nid_postfix(duplicate, CURLOPT_HEADERDATA, nullptr) == CURLE_OK);
    output.clear();
    Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK && output == bytes);
    auto* guestLog = std::fopen((path.string() + "-guestLog").c_str(), "w+b");
    auto* hostLog = std::fopen((path.string() + "-hostLog").c_str(), "w+b");
    auto* hostSink = std::fopen((path.string() + "-hostSink").c_str(), "w+b");
    Require(guestLog && hostLog && hostSink);
    FileStream guestLogStream(guestLog);
    auto* verbose = curl_easy_init_nid_postfix();
    Require(verbose);
    Require(curl_easy_setopt_nid_postfix(verbose, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(verbose, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(verbose, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(verbose, CURLOPT_STDERR, &guestLogStream) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(verbose, CURLOPT_VERBOSE, int64_t{1}) == CURLE_OK);
    auto* host = ::curl_easy_init();
    Require(host);
    Require(::curl_easy_setopt(host, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(::curl_easy_setopt(host, CURLOPT_WRITEDATA, hostSink) == CURLE_OK);
    Require(::curl_easy_setopt(host, CURLOPT_STDERR, hostLog) == CURLE_OK);
    Require(::curl_easy_setopt(host, CURLOPT_VERBOSE, 1L) == CURLE_OK);
    Require(::curl_easy_perform(host) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(verbose) == CURLE_OK);
    ::curl_easy_cleanup(host);
    curl_easy_cleanup_nid_postfix(verbose);
    Require(StreamBytes(guestLog) == StreamBytes(hostLog));
    std::fclose(guestLog);
    std::fclose(hostLog);
    std::fclose(hostSink);
    curl_easy_cleanup_nid_postfix(duplicate);
    for (const auto* suffix : {"native", "second", "inputFile", "guestLog", "hostLog", "hostSink"})
        std::filesystem::remove(path.string() + "-" + suffix);
    std::filesystem::remove(path);
    std::filesystem::remove(uploadPath);
    TestMimeBase64Transfer();
    TestMimeFileGuestPath();
    curl_global_cleanup_nid_postfix();
#ifdef _WIN32
    WSACleanup();
#endif
}
