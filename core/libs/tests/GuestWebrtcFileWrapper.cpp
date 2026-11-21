#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr auto Close = _ZN6webrtc11FileWrapper5CloseEv_nid_postfix;
constexpr auto Flush = _ZN6webrtc11FileWrapper5FlushEv_nid_postfix;
constexpr auto Write = _ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix;
constexpr auto Move = _ZN6webrtc11FileWrapperC1EOS0__nid_postfix;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "WebRTC file wrapper: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireThrows(TCall call) {
    try { call(); }
    catch (const std::invalid_argument&) { return; }
    Require(false, "invalid object or operation must throw");
}

void RoundTrip() {
    FileStream stream(TemporaryFile::Open());
    WebRtcFileWrapper wrapper{&stream};
    const std::array<unsigned char, 8> bytes{0, 255, 13, 10, 26, 128, 1, 42};
    Require(Write(&wrapper, nullptr, 0), "zero-length write succeeds");
    Require(Write(&wrapper, bytes.data(), 3), "write first chunk");
    Require(Write(&wrapper, bytes.data() + 3, bytes.size() - 3), "write second chunk");
    Require(Flush(&wrapper), "flush data");
    Require(wrapper.file == &stream, "flush retains ownership");
    Require(ftell_nid_postfix(&stream) == 8, "write exactly the requested bytes");
    Require(fseek_nid_postfix(&stream, 0, SEEK_SET) == 0, "rewind");
    std::array<unsigned char, 10> result{};
    result.fill(0x5a);
    Require(fread_nid_postfix(result.data() + 1, 1, bytes.size(), &stream) == bytes.size(), "read back bytes");
    Require(std::memcmp(result.data() + 1, bytes.data(), bytes.size()) == 0, "binary data is preserved");
    Require(result.front() == 0x5a && result.back() == 0x5a, "read preserves guards");
    RequireThrows([&] { Write(&wrapper, nullptr, 1); });
    Require(Close(&wrapper), "close succeeds");
    Require(wrapper.file == nullptr && !stream.IsOpen(), "close clears wrapper and closes stream");
    Require(Close(&wrapper), "closing twice succeeds");
}

void Ownership() {
    FileStream stream(TemporaryFile::Open());
    WebRtcFileWrapper source{&stream};
    WebRtcFileWrapper destination;
    std::memset(&destination, 0xa5, sizeof(destination));
    Move(&destination, &source);
    Require(source.file == nullptr && destination.file == &stream, "move transfers ownership");
    Require(Close(&source) && stream.IsOpen(), "moved-from close does not close destination");
    Require(Write(&destination, "moved", 5), "moved-to wrapper can write");
    Require(Close(&destination) && !stream.IsOpen(), "moved-to wrapper closes stream");
    WebRtcFileWrapper empty{};
    Move(&destination, &empty);
    Require(destination.file == nullptr && empty.file == nullptr, "move of empty wrapper");
    Require(Close(&destination), "close moved empty wrapper");
}

void WriteFailure() {
    FileStream stream(TemporaryFile::Open());
#ifdef _WIN32
    const int descriptor = _dup(_fileno(stream.GetHandle()));
    Require(descriptor >= 0, "duplicate stream descriptor");
    auto* native = _fdopen(descriptor, "rb");
#else
    const int descriptor = dup(fileno(stream.GetHandle()));
    Require(descriptor >= 0, "duplicate stream descriptor");
    auto* native = fdopen(descriptor, "rb");
#endif
    Require(native != nullptr, "create read-only stream");
    FileStream reader(native);
    WebRtcFileWrapper wrapper{&reader};
    Require(!Write(&wrapper, "x", 1), "read-only write returns false");
    Require(wrapper.file == &reader, "failed write retains ownership");
    Require(Close(&wrapper), "close read-only stream");
    Require(stream.Close() == 0, "close original stream");
}

#ifdef __linux__
void BufferedFailures() {
    for (bool flushFirst : {false, true}) {
        auto* native = std::fopen("/dev/full", "wb");
        Require(native != nullptr, "open full device");
        Require(std::setvbuf(native, nullptr, _IOFBF, 4096) == 0, "enable buffering");
        FileStream stream(native);
        WebRtcFileWrapper wrapper{&stream};
        Require(Write(&wrapper, "data", 4), "buffer write before device failure");
        if (flushFirst) {
            Require(!Flush(&wrapper), "flush reports device failure");
            Require(wrapper.file == &stream && stream.IsOpen(), "failed flush retains ownership");
            Close(&wrapper);
        } else {
            Require(!Close(&wrapper), "close reports implicit flush failure");
        }
        Require(wrapper.file == nullptr && !stream.IsOpen(), "failed close still releases ownership");
        Require(Close(&wrapper), "close after error succeeds");
    }
}
#endif

}

int main() {
    RoundTrip();
    Ownership();
    WriteFailure();
#ifdef __linux__
    BufferedFailures();
#endif
    WebRtcFileWrapper empty{};
    Require(Close(&empty), "close unopened wrapper");
    RequireThrows([] { Close(nullptr); });
    RequireThrows([] { Flush(nullptr); });
    RequireThrows([] { Write(nullptr, "x", 1); });
    RequireThrows([&] { Flush(&empty); });
    RequireThrows([&] { Write(&empty, "x", 1); });
    RequireThrows([&] { Move(nullptr, &empty); });
    RequireThrows([&] { Move(&empty, nullptr); });
    RequireThrows([&] { Move(&empty, &empty); });
}
