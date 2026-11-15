#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>

extern "C" std::FILE* HostStandardErrorFile_nid_no_patch();
#else
#include <unistd.h>
#endif

namespace {

std::FILE* Stream() {
#ifdef _WIN32
    return HostStandardErrorFile_nid_no_patch();
#else
    return stderr;
#endif
}

void Require(bool value, const std::string& what) {
    if (value) return;
    std::printf("host print single write test failed: %s\n", what.c_str());
    std::fflush(stdout);
    std::_Exit(1);
}

class CapturedStandardError {
public:
    CapturedStandardError() {
        std::fflush(Stream());
#ifdef _WIN32
        const std::string name = "\\\\.\\pipe\\anyps5-host-print-" + std::to_string(_getpid()) + "-" + std::to_string(++serial);
        server = CreateNamedPipeA(name.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 1u << 20u, 1u << 20u, 0, nullptr);
        Require(server != INVALID_HANDLE_VALUE, "cannot create the message pipe");
        const HANDLE client = CreateFileA(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        Require(client != INVALID_HANDLE_VALUE, "cannot open the message pipe");
        Require(ConnectNamedPipe(server, nullptr) != 0 || GetLastError() == ERROR_PIPE_CONNECTED, "cannot connect the message pipe");
        const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(client), _O_WRONLY | _O_BINARY);
        Require(descriptor >= 0, "cannot wrap the message pipe");
        saved = _dup(2);
        Require(saved >= 0 && _dup2(descriptor, 2) == 0, "cannot divert stderr");
        _close(descriptor);
#else
        int ends[2];
        Require(pipe(ends) == 0, "cannot create the pipe");
        source = ends[0];
        saved = dup(2);
        Require(saved >= 0 && dup2(ends[1], 2) == 2, "cannot divert stderr");
        close(ends[1]);
#endif
        reader = std::thread([this] { collect(); });
    }

    std::vector<std::string> Finish() {
        std::fflush(Stream());
#ifdef _WIN32
        Require(_dup2(saved, 2) == 0, "cannot restore stderr");
        _close(saved);
#else
        Require(dup2(saved, 2) == 2, "cannot restore stderr");
        close(saved);
#endif
        reader.join();
#ifdef _WIN32
        CloseHandle(server);
        return std::move(writes);
#else
        close(source);
        std::string text;
        for (const auto& write : writes) text += write;
        std::vector<std::string> lines;
        std::size_t start = 0;
        for (std::size_t end = text.find('\n'); end != std::string::npos; end = text.find('\n', start)) {
            lines.push_back(text.substr(start, end + 1 - start));
            start = end + 1;
        }
        if (start < text.size()) lines.push_back(text.substr(start));
        return lines;
#endif
    }

    static bool CountsWrites() {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

private:
    void collect() {
        std::vector<char> buffer(1u << 16u);
        for (;;) {
#ifdef _WIN32
            DWORD count = 0;
            if (ReadFile(server, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) == 0) return;
            writes.emplace_back(buffer.data(), count);
#else
            const auto count = ::read(source, buffer.data(), buffer.size());
            if (count <= 0) return;
            writes.emplace_back(buffer.data(), static_cast<std::size_t>(count));
#endif
        }
    }

    static inline int serial = 0;
#ifdef _WIN32
    HANDLE server = INVALID_HANDLE_VALUE;
#else
    int source = -1;
#endif
    int saved = -1;
    std::thread reader;
    std::vector<std::string> writes;
};

int PrintForwarded(const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    const int result = std::vfprintf(Stream(), format, arguments);
    va_end(arguments);
    return result;
}

void CheckOneCallIsOneWrite() {
    CapturedStandardError capture;
    const int direct = std::fprintf(Stream(), "[trophy] %s: %d trophies, %d groups, texts in %s\n", "/app0/sce_sys/trophy2/trophy00.ucp", 131, 1, "en-US");
    const int forwarded = PrintForwarded("[staging] adaptive: 0x%llx+0x%llx GPU %.1f ms/frame staged, %.1f in place\n", 0x200000000ull, 0xca80000ull, 91.0, 73.0);
    const int empty = std::fprintf(Stream(), "%s", "");
    const auto writes = capture.Finish();
    const std::string first = "[trophy] /app0/sce_sys/trophy2/trophy00.ucp: 131 trophies, 1 groups, texts in en-US\n";
    const std::string second = "[staging] adaptive: 0x200000000+0xca80000 GPU 91.0 ms/frame staged, 73.0 in place\n";
    Require(direct == static_cast<int>(first.size()) && forwarded == static_cast<int>(second.size()) && empty == 0, "fprintf did not return the number of characters");
    Require(writes.size() == 2, "two formatted lines reached the stream in " + std::to_string(writes.size()) + " pieces");
    Require(writes[0] == first && writes[1] == second, "a formatted line changed on the way to the stream");
}

void CheckEveryLengthIsOneWrite() {
    constexpr std::size_t Longest = 67;
    CapturedStandardError capture;
    for (std::size_t length = 0; length <= Longest; ++length) std::fprintf(Stream(), "%s\n", std::string(length, 'z').c_str());
    for (std::size_t length = 1; length <= Longest; ++length) std::fputs((std::string(length, 'w') + "\n").c_str(), Stream());
    const auto writes = capture.Finish();
    Require(writes.size() == 2 * Longest + 1, "lines of every length up to " + std::to_string(Longest) + " took " + std::to_string(writes.size()) + " writes for " + std::to_string(2 * Longest + 1) + " calls");
    for (std::size_t length = 0; length <= Longest; ++length) Require(writes[length] == std::string(length, 'z') + "\n", "a formatted line of " + std::to_string(length) + " characters was cut");
    for (std::size_t length = 1; length <= Longest; ++length) Require(writes[Longest + length] == std::string(length, 'w') + "\n", "a plain line of " + std::to_string(length) + " characters was cut");
}

void CheckLongLineIsOneWrite() {
    const std::string payload(5000, 'x');
    CapturedStandardError capture;
    const int written = std::fprintf(Stream(), "[dispatch-io] %s end %d\n", payload.c_str(), 42);
    const auto writes = capture.Finish();
    const std::string expected = "[dispatch-io] " + payload + " end 42\n";
    Require(written == static_cast<int>(expected.size()), "a long line did not return its length");
    std::string joined;
    for (const auto& write : writes) joined += write;
    Require(joined == expected, "a long line changed on the way to the stream");
    if (CapturedStandardError::CountsWrites()) Require(writes.size() == 1, "a 5 KiB line reached the stream in " + std::to_string(writes.size()) + " pieces");
}

void CheckBurstKeepsWholeLines() {
    constexpr int Threads = 8;
    constexpr int Lines = 250;
    CapturedStandardError capture;
    {
        std::vector<std::thread> threads;
        for (int thread = 0; thread < Threads; ++thread) {
            threads.emplace_back([thread] {
                for (int line = 0; line < Lines; ++line) {
                    std::fprintf(Stream(), "[burst] thread %02d line %04d %s %.3f\n", thread, line, "a few words that another thread could cut", line / 8.0);
                    std::fprintf(Stream(), "[burst] thread %02d note %04d\n[burst] thread %02d second %04d\n", thread, line, thread, line);
                }
            });
        }
        threads.emplace_back([] {
            for (int line = 0; line < Lines; ++line) std::fputs("[plain] a line written without formatting\n", Stream());
        });
        for (auto& thread : threads) thread.join();
    }
    const auto writes = capture.Finish();
    std::map<int, int> next;
    std::size_t plain = 0;
    std::size_t formatted = 0;
    const auto take = [&](const std::string& line) {
        if (line == "[plain] a line written without formatting\n") {
            ++plain;
            return;
        }
        int thread = -1;
        int index = -1;
        char kind[16] = "";
        Require(std::sscanf(line.c_str(), "[burst] thread %d %15s %d", &thread, kind, &index) == 3, "a line was cut: " + line);
        char expected[160];
        const int step = next[thread];
        if (step % 3 == 0) std::snprintf(expected, sizeof(expected), "[burst] thread %02d line %04d %s %.3f\n", thread, step / 3, "a few words that another thread could cut", (step / 3) / 8.0);
        else std::snprintf(expected, sizeof(expected), "[burst] thread %02d %s %04d\n", thread, step % 3 == 1 ? "note" : "second", step / 3);
        Require(line == expected, "a line was cut or out of order: " + line);
        ++next[thread];
        ++formatted;
    };
    for (const auto& write : writes) {
        Require(!write.empty() && write.back() == '\n', "a write ended inside a line: " + write);
        std::size_t start = 0;
        for (std::size_t end = write.find('\n'); end != std::string::npos; end = write.find('\n', start)) {
            take(write.substr(start, end + 1 - start));
            start = end + 1;
        }
    }
    Require(plain == Lines && formatted == static_cast<std::size_t>(Threads) * Lines * 3, "lines were lost in the burst");
    if (CapturedStandardError::CountsWrites()) Require(writes.size() == static_cast<std::size_t>(Threads) * Lines * 2 + Lines, "the burst took " + std::to_string(writes.size()) + " writes for " + std::to_string(Threads * Lines * 2 + Lines) + " calls");
}

void CheckBufferedFileKeepsItsText() {
    std::FILE* const file = std::tmpfile();
    Require(file != nullptr, "cannot create a temporary file");
    const std::string payload(3000, 'y');
    const int shortLine = std::fprintf(file, "value %d and %s\n", 7, "text");
    const int longLine = std::fprintf(file, "%s\n", payload.c_str());
    Require(shortLine == 17 && longLine == 3001, "fprintf into a file did not return the number of characters");
    std::rewind(file);
    std::string text(4000, '\0');
    text.resize(std::fread(text.data(), 1, text.size(), file));
    std::fclose(file);
    Require(text == "value 7 and text\n" + payload + "\n", "fprintf into a buffered file changed the text");
}

}

int main() {
    CheckOneCallIsOneWrite();
    CheckEveryLengthIsOneWrite();
    CheckLongLineIsOneWrite();
    CheckBurstKeepsWholeLines();
    CheckBufferedFileKeepsItsText();
    std::puts("host print single write tests passed");
    return 0;
}
