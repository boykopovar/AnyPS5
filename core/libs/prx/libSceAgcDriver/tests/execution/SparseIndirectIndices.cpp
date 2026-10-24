#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};
constexpr std::uint32_t DeclaredIndices = 0x10000000u;
constexpr std::size_t ReservationBytes = std::size_t{DeclaredIndices} * 4;
alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 9> WriterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020203u, 0x7e040204u, 0xe07c0000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> Reference{};
alignas(256) std::array<std::byte, 1024> Actual{};
alignas(256) std::array<std::uint32_t, 24> Records{};
alignas(256) std::array<std::uint32_t, 4> RecordDescriptor{};
alignas(256) std::array<std::uint32_t, 4> Count{};
alignas(256) std::array<std::uint32_t, 4> CountDescriptor{};

class SparseIndices {
public:
    SparseIndices() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, ReservationBytes, MEM_RESERVE, PAGE_NOACCESS));
        Require(memory != nullptr, "index reservation failed");
        Require(VirtualAlloc(memory, page, MEM_COMMIT, PAGE_READWRITE) == memory, "first index page commit failed");
        Require(VirtualAlloc(memory + 2 * page, page, MEM_COMMIT, PAGE_READWRITE) == memory + 2 * page, "last index page commit failed");
#else
        const auto hostPage = sysconf(_SC_PAGESIZE);
        Require(hostPage > 0, "host page size is unavailable");
        page = static_cast<std::size_t>(hostPage);
        auto* allocation = mmap(nullptr, ReservationBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "index reservation failed");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory, page, PROT_READ | PROT_WRITE) == 0, "first index page commit failed");
        Require(mprotect(memory + 2 * page, page, PROT_READ | PROT_WRITE) == 0, "last index page commit failed");
#endif
    }

    ~SparseIndices() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, ReservationBytes);
#endif
    }

    SparseIndices(const SparseIndices&) = delete;
    SparseIndices& operator=(const SparseIndices&) = delete;

    std::byte* Base() const { return memory + page - 16; }
    std::uint32_t AfterGap(std::uint32_t size) const { return static_cast<std::uint32_t>((page + 16) / size); }

    void Fill(std::uint32_t size) {
        const std::array<std::uint16_t, 3> small{0, 1, 2};
        const std::array<std::uint32_t, 3> large{0, 1, 2};
        const void* source = size == 2 ? static_cast<const void*>(small.data()) : static_cast<const void*>(large.data());
        std::memcpy(Base(), source, 3 * size);
        std::memcpy(memory + 2 * page, source, 3 * size);
    }

private:
    std::byte* memory = nullptr;
    std::size_t page = 0;
};

std::uint32_t Low(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t High(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); }

void Append(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    words.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    words.insert(words.end(), payload);
}

void WriteArguments(std::vector<std::uint32_t>& words, const std::uint32_t* descriptor, std::array<std::uint32_t, 3> values) {
    const auto address = reinterpret_cast<std::uintptr_t>(WriterCode.data());
    Append(words, 0x76, {0x20c, static_cast<std::uint32_t>(address >> 8u)});
    Append(words, 0x76, {0x20d, static_cast<std::uint32_t>(address >> 40u)});
    for (const auto offset : {0x207u, 0x208u, 0x209u}) Append(words, 0x76, {offset, 1});
    Append(words, 0x76, {0x212, 0});
    Append(words, 0x76, {0x213, 10});
    Append(words, 0x76, {0x240, Low(descriptor), High(descriptor), values[0], values[1], values[2]});
    Append(words, 0x15, {1, 1, 1, 0x8041});
}

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};
alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

std::vector<std::uint32_t> State() {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0}, {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240}, {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028}, {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0}, {0xc, 0}, {0xd, 0x40040}, {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040}, {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    std::vector<std::uint32_t> words;
    Append(words, 0x79, {0x242, 4});
    Append(words, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(words, 0x69, {offset, value});
    Append(words, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(words, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(words, 0x76, {0x8b, 2});
    Append(words, 0x76, {0x8c, 0});
    Append(words, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u)});
    Append(words, 0x76, {0x009, static_cast<std::uint32_t>(pixel >> 40u)});
    Append(words, 0x76, {0x00b, 2});
    Append(words, 0x76, {0x00c, std::bit_cast<std::uint32_t>(1.0f)});
    return words;
}

void Target(std::vector<std::uint32_t>& words, void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Append(words, 0x69, {0x318, static_cast<std::uint32_t>(address >> 8u)});
    Append(words, 0x69, {0x390, static_cast<std::uint32_t>(address >> 40u)});
}

void Run(SparseIndices& indices, std::uint32_t size, bool multi, bool afterGap, bool unreadable) {
    indices.Fill(size);
    Reference.fill(Kept);
    Actual.fill(Kept);
    Records.fill(0);
    Count.fill(0);
    const auto first = unreadable ? 16u / size : afterGap ? indices.AfterGap(size) : 0u;
    Records[8] = 3;
    Records[9] = 1;
    Records[10] = DeclaredIndices;
    Records[16] = 3;
    Records[17] = 1;
    Records[18] = 16u / size;
    RecordDescriptor = {Low(Records.data()), High(Records.data()) & 0xffffu, sizeof(Records), 0x31016facu};
    CountDescriptor = {Low(Count.data()), High(Count.data()) & 0xffffu, sizeof(Count), 0x31016facu};
    auto words = State();
    Target(words, Reference.data());
    Append(words, 0x2a, {1});
    Append(words, 0x2d, {3, 2});
    WriteArguments(words, RecordDescriptor.data(), {3, 1, first});
    if (multi) WriteArguments(words, CountDescriptor.data(), {2, 0, 0});
    Target(words, Actual.data());
    Append(words, 0x2a, {size == 2 ? 0u : 1u});
    Append(words, 0x26, {Low(indices.Base()), High(indices.Base())});
    Append(words, 0x13, {DeclaredIndices});
    Append(words, 0x11, {1, Low(Records.data()), High(Records.data())});
    if (multi) Append(words, 0x38, {0, 0, 0, 0x40000280u, 3, Low(Count.data()), High(Count.data()), 32, 0});
    else Append(words, 0x25, {0, 0, 0, 0});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "sparse indexed draw submit failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(Records[0] == 3 && Records[1] == 1 && Records[2] == first, "the GPU did not write the current indirect record");
    Require(!multi || Count[0] == 2, "the GPU did not write the indirect draw count");
    const auto covered = std::count_if(Reference.begin(), Reference.end(), [](std::byte value) { return value != Kept; });
    Require(covered != 0, "the reference triangle did not draw");
    Require(Actual == Reference, "the sparse indexed indirect draw differs from the reference triangle");
    std::cout << "sparse indices " << size << " bytes, " << (multi ? "counted multi" : "single") << ", " << (afterGap ? "after gap" : "before gap") << " passed\n";
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        SparseIndices indices;
        if (argc == 2 && std::string(argv[1]) == "--unreadable") {
            bool refused = false;
            try {
                Run(indices, 2, false, false, true);
            } catch (const std::runtime_error& error) {
                const std::string message = error.what();
                refused = message.find("guest memory is not readable") != std::string::npos && message.find("+0x6)") != std::string::npos;
                Require(refused, "the unreadable record did not fail on its actual six-byte index range: " + message);
            }
            Require(refused, "an indirect record reading the unmapped index page was accepted");
            try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
            std::cout << "the unreadable indirect index range was refused\n";
            return 0;
        }
        for (const auto size : {2u, 4u}) {
            for (const auto multi : {false, true}) {
                Run(indices, size, multi, false, false);
                Run(indices, size, multi, true, false);
            }
        }
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
