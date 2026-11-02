#include "prx/libSceSysUtil/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "SysUtil notification: %s\n", message);
    std::abort();
}

class StandardErrorCapture {
public:
    StandardErrorCapture() : file_(TemporaryFile::Open()) {
        Require(file_ != nullptr, "the capture file could not be opened");
        std::fflush(stderr);
#ifdef _WIN32
        saved_ = ::_dup(::_fileno(stderr));
        Require(saved_ >= 0 && ::_dup2(::_fileno(file_), ::_fileno(stderr)) == 0, "stderr could not be redirected");
#else
        saved_ = ::dup(::fileno(stderr));
        Require(saved_ >= 0 && ::dup2(::fileno(file_), ::fileno(stderr)) == ::fileno(stderr), "stderr could not be redirected");
#endif
    }

    ~StandardErrorCapture() {
        Restore();
        if (file_ != nullptr) std::fclose(file_);
    }

    std::string Text() {
        Restore();
        std::rewind(file_);
        std::string text;
        char buffer[256];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), file_)) text.append(buffer, count);
        return text;
    }

private:
    void Restore() {
        if (saved_ < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        ::_dup2(saved_, ::_fileno(stderr));
        ::_close(saved_);
#else
        ::dup2(saved_, ::fileno(stderr));
        ::close(saved_);
#endif
        saved_ = -1;
    }

    std::FILE* file_;
    int saved_ = -1;
};

}

int main() {
    std::string longText;
    for (int index = 0; index < 300; ++index) longText.push_back(static_cast<char>('a' + index % 26));

    int hello = -1;
    int empty = -1;
    int truncated = -1;
    std::string output;
    try {
        StandardErrorCapture capture;
        hello = sceSysUtilSendSystemNotificationWithText(7, "hello");
        empty = sceSysUtilSendSystemNotificationWithText(-1, nullptr);
        truncated = sceSysUtilSendSystemNotificationWithText(0, longText.c_str());
        output = capture.Text();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SysUtil notification: %s\n", error.what());
        std::abort();
    }

    Require(hello == 0, "a text notification was not sent");
    Require(empty == 0, "a null message was not sent as an empty text");
    Require(truncated == 0, "a long message was not sent");
    const std::string expected = "[notification] hello\n[notification] \n[notification] " + longText.substr(0, 179) + "\n";
    if (output != expected) {
        std::fprintf(stderr, "notification output was:\n%s", output.c_str());
        Require(false, "the notification output differs from the sent texts");
    }

    std::puts("SysUtil notification tests passed");
    return 0;
}
