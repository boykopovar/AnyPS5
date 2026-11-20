#include "prx/libkernel_sys/libkernel_sys.hpp"
#include "prx/libSceAudioIn/include/AudioIn.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int InvalidSize = static_cast<int>(0x80260102);
constexpr int InvalidFrequency = static_cast<int>(0x80260103);
constexpr int InvalidParameter = static_cast<int>(0x80260106);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AUDIOIN PS4 OPEN: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int OpenAndClose(std::uint32_t samples, std::uint32_t frequency, std::uint32_t param) {
    const int handle = sceAudioInOpen(0, 0, 0, samples, frequency, param);
    if (handle >= 0) Require(sceAudioInClose(handle) == 0, "a successful open must close");
    return handle;
}

void CheckPs5OpenContract() {
    Require(sceKernelIsPs4Process() == 0, "the parent process must use the PS5 open path");
    Require(OpenAndClose(0x100, 48000, 2) >= 0, "PS5 stereo 48 kHz / 256-sample open must remain accepted");
    Require(OpenAndClose(0x80, 48000, 2) >= 0, "PS5 stereo 48 kHz / 128-sample open must remain accepted");
    Require(OpenAndClose(0x100, 16000, 2) >= 0, "PS5 stereo 16 kHz / 256-sample open must remain accepted");
}

void CheckPs4OpenContract() {
    Require(sceKernelIsPs4Process() == 1, "a CUSA title must select the PS4 open path");
    Require(OpenAndClose(0x100, 16000, 0) >= 0, "PS4 mono parameter 0 at 16 kHz / 256 samples must be accepted");
    Require(OpenAndClose(0x100, 16000, 1) >= 0, "PS4 mono parameter 1 at 16 kHz / 256 samples must be accepted");
    Require(OpenAndClose(0x100, 48000, 1) == InvalidFrequency, "PS4 must reject frequencies other than 16 kHz");
    Require(OpenAndClose(0x80, 16000, 1) == InvalidSize, "PS4 must reject granularities other than 256 samples");
    Require(OpenAndClose(0x100, 16000, 2) == InvalidParameter, "PS4 must reject parameters greater than 1");
}

void WritePs4ParamSfo(const std::filesystem::path& directory) {
    struct Entry {
        std::uint16_t key;
        std::uint16_t format;
        std::uint32_t length;
        std::uint32_t maximum;
        std::uint32_t offset;
    };
    const std::uint32_t header[5] = {0x46535000, 0x0101, 52, 72, 2};
    const Entry entries[2] = {{0, 0x0204, 8, 8, 0}, {6, 0x0204, 10, 12, 8}};
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    std::ofstream file(directory / "app0" / "sce_sys" / "param.sfo", std::ios::binary);
    Require(file.good(), "the PS4 param.sfo must be created");
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(entries), sizeof(entries));
    file << std::string("TITLE\0TITLE_ID\0\0\0\0\0\0", 20)
         << std::string("Example\0", 8)
         << "CUSA00000"
         << std::string("\0\0\0", 3);
    Require(file.good(), "the PS4 param.sfo must be written completely");
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "ps4-child") {
        CheckPs4OpenContract();
        return 0;
    }

    CheckPs5OpenContract();

    const auto directory = std::filesystem::temp_directory_path() /
        ("audio-in-ps4-open-" + std::to_string(std::random_device{}()));
    WritePs4ParamSfo(directory);

#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length != 0 && length < executable.size(), "the test executable path must be readable");
    executable.resize(length);
    const std::wstring command = L"\"cd /d \"" + directory.wstring() + L"\" && \"" + executable + L"\" ps4-child\"";
    const int status = _wsystem(command.c_str());
#else
    const std::string executable = std::filesystem::absolute(argv[0]).string();
    const std::string command = "cd \"" + directory.string() + "\" && \"" + executable + "\" ps4-child";
    const int status = std::system(command.c_str());
#endif

    std::filesystem::remove_all(directory);
    Require(status == 0, "the PS4 child process must pass its AudioIn checks");
    return 0;
}
