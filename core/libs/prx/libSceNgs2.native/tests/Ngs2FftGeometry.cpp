#include "prx/libSceNgs2/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::vector<std::byte> work(4096);
    void* handle = nullptr;

    Require(sceNgs2FftInit(nullptr, work.size(), 8, 0, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), work.size(), 8, 0, nullptr) == -1);
    Require(sceNgs2FftInit(work.data(), work.size(), 7, 0, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), work.size(), 13, 0, &handle) == -1);
    Require(handle == nullptr);

    errno = ERANGE;
    Require(sceNgs2FftInit(work.data(), 2111, 8, 0, &handle) == -1);
    Require(errno == ERANGE);
    Require(sceNgs2FftInit(work.data(), 2112, 8, 0, &handle) == 0);
    Require(handle == work.data());

    const auto* header = static_cast<const std::uint8_t*>(handle);
    std::int32_t length = 0;
    std::uint32_t flags = 0xffffffffu;
    std::memcpy(&length, header + 0x10, sizeof(length));
    std::memcpy(&flags, header + 0x14, sizeof(flags));
    Require(length == 8 && flags == 0);

    Require(sceNgs2FftInit(work.data(), 1087, 8, 2, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), 1088, 8, 2, &handle) == 0);

    std::vector<float> scratch(1024);
    std::vector<float> input(512, 3.0f);
    std::vector<float> output(512, 7.0f);
    Require(sceNgs2FftProcess(nullptr, input.data(), output.data(), scratch.data(), 0, 0) == -1);
    Require(sceNgs2FftProcess(handle, nullptr, output.data(), scratch.data(), 0, 0) == -1);
    Require(sceNgs2FftProcess(handle, input.data(), nullptr, scratch.data(), 0, 0) == -1);
    Require(sceNgs2FftProcess(reinterpret_cast<void*>(0x10), input.data(), output.data(), scratch.data(), 0, 0) == -1);

    Require(sceNgs2FftInit(work.data(), work.size(), 8, 0, &handle) == 0);
    output.assign(output.size(), 7.0f);
    Require(sceNgs2FftProcess(handle, input.data(), output.data(), nullptr, 0, 0) == 0);
    Require(output.front() == 0.0f && output.back() == 0.0f);
    return 0;
}
