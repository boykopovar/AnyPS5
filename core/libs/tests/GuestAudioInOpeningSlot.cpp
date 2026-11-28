#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int invalidState = static_cast<int>(0x80260109);
constexpr int portFull = static_cast<int>(0x80260107);

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct CaptureFile {
    std::string directory = (std::filesystem::temp_directory_path() / "anyps5-audioin-XXXXXX").string();
    std::filesystem::path path;

    CaptureFile() {
        Require(mkdtemp(directory.data()) != nullptr, "cannot create the capture directory");
        path = std::filesystem::path(directory) / "capture";
    }

    ~CaptureFile() {
        unlink(path.c_str());
        rmdir(directory.c_str());
    }
};

int OpenPort() {
    return sceAudioInHqOpen(0x10000000, 0, 0, 128, 48000, 2);
}

}

int main() {
    CaptureFile capture;
    Require(setenv("SDL_DISKAUDIOFILEIN", capture.path.c_str(), 1) == 0, "cannot select the capture file");
    Require(sceAudioInInit() == 0, "AudioIn initialization failed");
    std::array<int, 8> ports{};
    for (int& handle : ports) {
        handle = OpenPort();
        Require(handle > 0 && sceAudioInGetSilentState(handle) == 1, "missing capture file should yield a silent port");
    }
    const int stale = ports.back();
    Require(sceAudioInClose(stale) == 0, "cannot release the last port");
    Require(mkfifo(capture.path.c_str(), 0600) == 0, "cannot create the capture FIFO");

    std::promise<void> full;
    auto observedFull = full.get_future();
    auto openLast = [&] {
        const int handle = OpenPort();
        if (handle == portFull) full.set_value();
        return handle;
    };
    auto first = std::async(std::launch::async, openLast);
    auto second = std::async(std::launch::async, openLast);
    observedFull.wait();
    float gain = 0.0f;
    const bool hidden = sceAudioInGetSilentState(stale) == invalidState &&
                        sceAudioInGetGain(stale, &gain) == invalidState &&
                        sceAudioInIsPadEmbeddedMic(stale) == invalidState &&
                        sceAudioInIsSharedDevice(stale) == invalidState &&
                        sceAudioInInput(stale, nullptr) == invalidState &&
                        sceAudioInClose(stale) == invalidState;
    const int other = sceAudioInGetSilentState(ports.front());

    const int writer = open(capture.path.c_str(), O_RDWR | O_NONBLOCK);
    Require(writer >= 0, "cannot release the blocked SDL capture open");
    const int left = first.get();
    const int right = second.get();
    close(writer);
    Require((left == portFull && right == stale) || (right == portFull && left == stale),
            "one open must reserve the last slot while the other reports a full table");
    Require(sceAudioInGetSilentState(stale) == 0, "the completed open must use the disk capture device");
    for (const int handle : ports) Require(sceAudioInClose(handle) == 0, "cannot close a completed port");
    Require(other == 1, "an existing port must stay available during the open");
    Require(hidden, "the opening slot was visible through its previously closed handle");
}
