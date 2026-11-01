#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <source_location>
#include <string>

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

int main() {
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
    curl_global_cleanup_nid_postfix();
}
