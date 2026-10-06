#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceVideoRecordingQueryMemSize(const void* param);
extern "C" int APS5_VABI sceVideoRecordingOpen(const char* path, const void* param, void* heap, int heapSize);
extern "C" int APS5_VABI sceVideoRecordingStart(void);
extern "C" int APS5_VABI sceVideoRecordingStop(void);
extern "C" int APS5_VABI sceVideoRecordingClose(int discard);
extern "C" int APS5_VABI sceVideoRecordingGetStatus(void);

namespace {

constexpr int ERROR_INVALID_STATE = static_cast<int>(0x80A80006);
constexpr int ERROR_UNSUPPORTED = static_cast<int>(0x80A80008);

struct RecordingParam {
    std::size_t size;
    int ringSec;
    int format;
};

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

alignas(16) unsigned char heap[4096];

}

int main() {
    const RecordingParam param{sizeof(RecordingParam), 15 * 60, 0};
    Require(Throws([] { sceVideoRecordingQueryMemSize(nullptr); }));
    const int memSize = sceVideoRecordingQueryMemSize(&param);
    Require(memSize == 4096);

    Require(sceVideoRecordingGetStatus() == 0);
    Require(Throws([&] { sceVideoRecordingOpen(nullptr, &param, heap, memSize); }));
    Require(Throws([&] { sceVideoRecordingOpen("/data/clip.mp4", nullptr, heap, memSize); }));
    Require(Throws([&] { sceVideoRecordingOpen("/data/clip.mp4", &param, nullptr, memSize); }));
    const std::string longPath(1024, 'a');
    Require(Throws([&] { sceVideoRecordingOpen(longPath.c_str(), &param, heap, memSize); }));
    const std::string maxPath(1023, 'a');
    Require(sceVideoRecordingOpen(maxPath.c_str(), &param, heap, memSize) == ERROR_UNSUPPORTED);
    Require(sceVideoRecordingOpen("/data/clip.mp4", &param, heap, memSize) == ERROR_UNSUPPORTED);
    Require(sceVideoRecordingGetStatus() == 0);

    Require(sceVideoRecordingStart() == ERROR_INVALID_STATE);
    Require(sceVideoRecordingStop() == ERROR_INVALID_STATE);
    Require(sceVideoRecordingClose(0) == ERROR_INVALID_STATE);
    Require(sceVideoRecordingClose(1) == ERROR_INVALID_STATE);
    Require(sceVideoRecordingGetStatus() == 0);
}
