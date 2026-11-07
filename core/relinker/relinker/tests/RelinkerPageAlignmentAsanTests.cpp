#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;

template<typename TValue>
void Write(Bytes& bytes, const std::size_t offset, const TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset)
        throw std::runtime_error("Invalid fixture offset");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

Bytes Fixture() {
    Bytes bytes(0x300);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(bytes.data(), magic, sizeof(magic));
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint64_t>(bytes, 40, 128);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 1);
    Write<std::uint16_t>(bytes, 58, 64);
    Write<std::uint16_t>(bytes, 60, 3);
    Write<std::uint16_t>(bytes, 62, 2);
    Write<std::uint32_t>(bytes, 64, 1);
    Write<std::uint32_t>(bytes, 68, 5);
    Write<std::uint64_t>(bytes, 72, 0x240);
    Write<std::uint64_t>(bytes, 80, 0x1000);
    Write<std::uint64_t>(bytes, 96, 32);
    Write<std::uint64_t>(bytes, 104, 64);
    Write<std::uint64_t>(bytes, 112, 0);
    Write<std::uint32_t>(bytes, 192, 1);
    Write<std::uint32_t>(bytes, 196, 1);
    Write<std::uint64_t>(bytes, 216, 0x240);
    Write<std::uint64_t>(bytes, 224, 32);
    Write<std::uint64_t>(bytes, 240, 8);
    Write<std::uint64_t>(bytes, 248, 24);
    Write<std::uint32_t>(bytes, 256, 7);
    Write<std::uint32_t>(bytes, 260, 3);
    Write<std::uint64_t>(bytes, 280, 0x200);
    const char strings[] = "\0.text\0.shstrtab";
    Write<std::uint64_t>(bytes, 288, sizeof(strings));
    std::memcpy(bytes.data() + 0x200, strings, sizeof(strings));
    return bytes;
}

void Require(const bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error("Assertion failed: " + message);
}

void RejectProgramHeaders(Bytes bytes, const std::string& expectedMessage) {
    try {
        Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
    } catch (const Relinker::RelinkerException& error) {
        if (!expectedMessage.empty()) {
            Require(std::string(error.what()).find(expectedMessage) != std::string::npos, "Unexpected error: " + std::string(error.what()));
        }
        return;
    }
    throw std::runtime_error("Invalid program header was accepted");
}

void RejectWindowsLoadImage(const Bytes& source, const std::vector<Domain::ProgramHeader>& headers, const std::string& expectedMessage) {
    try {
        Elfpatcher::Windows::WindowsLoadImage(source, headers, false, Elfpatcher::Windows::ImageBase);
    } catch (const Domain::RelinkerException& error) {
        if (!expectedMessage.empty()) {
            Require(std::string(error.what()).find(expectedMessage) != std::string::npos, "Unexpected error: " + std::string(error.what()));
        }
        return;
    }
    throw std::runtime_error("Invalid WindowsLoadImage headers were accepted");
}

}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> cases = {
        {"constants verification", [] {
            Require(Elfpatcher::kHostPageSize == 0x1000, "kHostPageSize must be 4096");
            Require(Elfpatcher::kPs5PageSize == 0x4000, "kPs5PageSize must be 16384");
            Require(Elfpatcher::kDefaultLoadAlignment == Elfpatcher::kHostPageSize, "kDefaultLoadAlignment mismatch");
        }},
        {"PT_LOAD PS5 16k page alignment parsed cleanly", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 112, Elfpatcher::kPs5PageSize);
            const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
            Require(headers.size() == 1, "Expected 1 program header");
            Require(headers[0].Alignment == 0x4000, "Expected 0x4000 alignment");
        }},
        {"PT_LOAD host 4k page alignment parsed cleanly", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 112, Elfpatcher::kHostPageSize);
            const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
            Require(headers.size() == 1, "Expected 1 program header");
            Require(headers[0].Alignment == 0x1000, "Expected 0x1000 alignment");
        }},
        {"PT_LOAD valid power-of-two alignments accepted", [] {
            for (const std::uint64_t align : {0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL, 128ULL, 256ULL, 512ULL, 1024ULL, 2048ULL, 4096ULL, 8192ULL, 16384ULL, 32768ULL, 65536ULL, 0x200000ULL, 0x40000000ULL}) {
                auto bytes = Fixture();
                Write<std::uint64_t>(bytes, 112, align);
                const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
                Require(headers.size() == 1 && headers[0].Alignment == align, "Failed for power-of-two alignment");
            }
        }},
        {"WindowsLoadImage address overflow rejected", [] {
            const auto source = Fixture();
            std::vector<Domain::ProgramHeader> headers{
                {1, 5, 0x240, 0xffffffffffff0000ULL, 0xffffffffffff0000ULL, 32, 0x20000ULL, Elfpatcher::kPs5PageSize}
            };
            RejectWindowsLoadImage(source, headers, "PT_LOAD address overflow");
        }},
        {"WindowsLoadImage unsupported permission flags rejected", [] {
            const auto source = Fixture();
            for (const std::uint32_t flags : {8u, 16u, 32u, 64u, 0x80u, 0x100u, 0x80000000u}) {
                std::vector<Domain::ProgramHeader> headers{
                    {1, flags, 0x240, 0x1000, 0x1000, 32, 64, Elfpatcher::kPs5PageSize}
                };
                RejectWindowsLoadImage(source, headers, "Unsupported PT_LOAD permission flags");
            }
        }},
        {"WindowsLoadImage PS5 and host page alignment accepted", [] {
            const auto source = Fixture();
            std::vector<Domain::ProgramHeader> headers{
                {1, 5, 0x240, 0x1000, 0x1000, 32, 64, Elfpatcher::kPs5PageSize}
            };
            Elfpatcher::Windows::WindowsLoadImage image(source, headers, false, Elfpatcher::Windows::ImageBase);
            Require(image.GetRva(0x1000, 8) == Elfpatcher::Windows::LoadRva, "Unexpected RVA for PS5 aligned load");
            Require(image.GetEndRva() > Elfpatcher::Windows::LoadRva, "Unexpected end RVA");
        }},
        {"WindowsLoadImage non power of two alignment rejected", [] {
            const auto source = Fixture();
            for (const std::uint64_t align : {3ULL, 5ULL, 7ULL, 0x3000ULL, 0x4001ULL}) {
                std::vector<Domain::ProgramHeader> headers{
                    {1, 5, 0x240, 0x1000, 0x1000, 32, 64, align}
                };
                RejectWindowsLoadImage(source, headers, "Invalid PT_LOAD alignment");
            }
        }},
        {"write-watch boundary alignment safety", [] {
            const auto source = Fixture();
            std::vector<Domain::ProgramHeader> headers{
                {1, 6, 0x240, 0x10000, 0x10000, 32, 0x8000, Elfpatcher::kPs5PageSize}
            };
            Elfpatcher::Windows::WindowsLoadImage image(source, headers, false, Elfpatcher::Windows::ImageBase);
            image.RequireWritable(0x10000, 8);
            image.RequireWritable(0x10000 + Elfpatcher::kHostPageSize - 8, 8);
            image.RequireWritable(0x10000 + Elfpatcher::kPs5PageSize - 8, 8);
            image.WritePointer(0x10000, 0x1234567890abcdefULL);
            image.WritePointer(0x10000 + Elfpatcher::kHostPageSize, 0xfedcba0987654321ULL);
            image.WritePointer(0x10000 + Elfpatcher::kPs5PageSize, 0xdeadbeefcafebabeULL);
            const auto sections = image.BuildSections();
            Require(!sections.empty(), "Sections should not be empty");
            Require(sections[0].Rva == Elfpatcher::Windows::LoadRva, "Unexpected first section RVA");
            std::uint64_t totalSize = 0;
            for (const auto& sec : sections) totalSize += sec.VirtualSize;
            Require(totalSize >= 0x8000, "Total virtual size should cover the allocated memory");
        }},
        {"ASAN guard page allocation isolation safety", [] {
            const auto source = Fixture();
            const std::size_t pageSize = 4096;
            const std::size_t allocSize = 2 * pageSize;
#ifdef _WIN32
            auto* const memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(memory != nullptr, "VirtualAlloc failed");
            DWORD oldProtect = 0;
            const BOOL protectOk = VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &oldProtect);
            Require(protectOk != 0, "VirtualProtect failed");
#else
            auto* const memory = static_cast<std::uint8_t*>(mmap(nullptr, allocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
            Require(memory != MAP_FAILED, "mmap failed");
            const int protectOk = mprotect(memory + pageSize, pageSize, PROT_NONE);
            Require(protectOk == 0, "mprotect failed");
#endif
            const std::size_t fixtureSize = source.size();
            Require(fixtureSize <= pageSize, "Fixture exceeds single page");
            std::uint8_t* const fitPtr = memory + pageSize - fixtureSize;
            std::memcpy(fitPtr, source.data(), fixtureSize);
            Bytes testBuffer(fitPtr, fitPtr + fixtureSize);
            const auto headers = Relinker::ElfReader(std::move(testBuffer)).ReadProgramHeaders();
            Require(headers.size() == 1, "Expected 1 header from guarded memory");

#ifdef _WIN32
            VirtualFree(memory, 0, MEM_RELEASE);
#else
            munmap(memory, allocSize);
#endif
        }}
    };

    int passed = 0;
    for (const auto& [name, test] : cases) {
        try {
            test();
            ++passed;
        } catch (const std::exception& error) {
            std::cerr << "FAILED: " << name << ": " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << passed << " relinker page alignment and ASAN isolation tests passed\n";
    return 0;
}
