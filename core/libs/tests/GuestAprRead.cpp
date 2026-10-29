#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::size_t FileBytes = (16u << 20u) + 257u;
constexpr std::size_t MappingBytes = 17u << 20u;
constexpr const char* FilePath = "apr_read_generated.bin";

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t FileByte(std::uint64_t offset) {
    return static_cast<std::uint8_t>((offset * 37u) ^ (offset >> 8u) ^ (offset >> 16u));
}

class Fixture {
public:
    Fixture() {
        std::ofstream stream(FilePath, std::ios::binary | std::ios::trunc);
        std::array<char, 65536> chunk;
        for (std::size_t offset = 0; offset < FileBytes; offset += chunk.size()) {
            const auto bytes = std::min(chunk.size(), FileBytes - offset);
            for (std::size_t index = 0; index < bytes; ++index) chunk[index] = static_cast<char>(FileByte(offset + index));
            stream.write(chunk.data(), static_cast<std::streamsize>(bytes));
        }
        stream.close();
        Require(!stream.fail(), "cannot create the synthetic APR file");
        const char* path = FilePath;
        std::uint32_t failed = 0;
        Require(sceKernelAprResolveFilepathsToIds(&path, 1, &id, &failed) == 0 && failed == 0, "cannot resolve the synthetic APR file");
    }

    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove(FilePath, ignored);
    }

    void Read(void* destination, std::uint64_t bytes, std::uint64_t offset) const {
        const Apr::ReadFileCommand command{{Apr::Opcode::ReadFile, sizeof(Apr::ReadFileCommand)}, id, 0,
            reinterpret_cast<std::uint64_t>(destination), bytes, offset};
        alignas(8) std::array<std::uint8_t, sizeof(command)> memory{};
        std::memcpy(memory.data(), &command, sizeof(command));
        Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, sizeof(memory), 1, sizeof(memory), memory.data()};
        Require(sceKernelAprSubmitCommandBuffer(&buffer, 0) == 0, "APR read submission failed");
    }

private:
    std::uint32_t id = 0;
};

class DirectBuffer {
public:
    DirectBuffer() {
        Require(sceKernelAllocateDirectMemory(0, 13824ll << 20, MappingBytes, 0x4000, 0, &physical) == 0, "cannot allocate direct memory");
        if (sceKernelMapDirectMemory(&pointer, MappingBytes, 0x33, 0, physical, 0x4000) != 0) {
            sceKernelReleaseDirectMemory(physical, MappingBytes);

            throw std::runtime_error("cannot map direct memory");
        }
    }

    ~DirectBuffer() {
        sceKernelMunmap(reinterpret_cast<std::uint64_t>(pointer), MappingBytes);
        sceKernelReleaseDirectMemory(physical, MappingBytes);

    }

    std::uint8_t* Data() const { return static_cast<std::uint8_t*>(pointer); }

private:
    void* pointer = nullptr;
    std::int64_t physical = 0;
};

void CheckRead(const Fixture& fixture, std::uint8_t* data, std::size_t bytes, std::uint64_t offset) {
    data[-1] = 0xA5;
    data[bytes] = 0x5A;
    fixture.Read(data, bytes, offset);
    Require(data[-1] == 0xA5 && data[bytes] == 0x5A, "APR wrote outside the requested range");
    for (std::size_t index = 0; index < bytes; ++index) {
        const auto expected = offset + index < FileBytes ? FileByte(offset + index) : std::uint8_t{0};
        Require(data[index] == expected, "APR returned incorrect bytes or did not zero-fill EOF");
    }
}

void TestRead(const Fixture& fixture, std::uint8_t* data) {
    CheckRead(fixture, data + 17, (16u << 20u) + 13u, 7);
    for (const auto bytes : {std::size_t{0}, std::size_t{1}, std::size_t{65535}, std::size_t{65536}, std::size_t{65537}}) {
        CheckRead(fixture, data + 17, bytes, 31);
    }
}

void TestEof(const Fixture& fixture) {
    std::vector<std::uint8_t> storage(131200, 0xCC);
    auto* data = storage.data() + 17;
    CheckRead(fixture, data, 131073, FileBytes - 65537);
    CheckRead(fixture, data, 65537, FileBytes);
    CheckRead(fixture, data, 65537, FileBytes + 19);
}

void TestErrors(const Fixture& fixture) {
    std::array<std::uint8_t, 8> data;
    data.fill(0xA5);
    bool rejected = false;
    try {
        fixture.Read(data.data(), data.size(), std::uint64_t{1} << 63u);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "APR accepted an unseekable offset");
    for (const auto byte : data) Require(byte == 0xA5, "failed APR read modified the destination");
#ifdef _WIN32
    {
        const DirectBuffer storage;
        storage.Data()[17] = 0xA5;
        Require(sceKernelMprotect(storage.Data(), MappingBytes, 0x11) == 0, "cannot protect the direct read buffer");
        rejected = false;
        try {
            fixture.Read(storage.Data() + 17, 65537, 0);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("not writable guest memory") != std::string::npos;
        }
        Require(rejected && storage.Data()[17] == 0xA5, "APR did not reject a read-only guest destination");
    }
#endif
    Require(std::filesystem::remove(FilePath), "cannot remove the synthetic APR file");
    rejected = false;
    try {
        fixture.Read(data.data(), data.size(), 0);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "APR accepted a missing file");
    for (const auto byte : data) Require(byte == 0xA5, "missing-file APR read modified the destination");
}

void TestStreams(const Fixture& fixture) {
    constexpr int StreamCount = 600;
    std::vector<FileStream*> streams;
    for (int index = 0; index < StreamCount; ++index) {
        FileStream* stream = fopen_nid_postfix(("apr_stream_" + std::to_string(index) + ".tmp").c_str(), "w");
        if (stream == nullptr) break;
        streams.push_back(stream);
    }
    const bool held = streams.size() == StreamCount;
    std::vector<std::uint8_t> storage(131200, 0xCC);
    std::string error;
    try {
        CheckRead(fixture, storage.data() + 17, 65537, 0);
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }
    for (FileStream* stream : streams) fclose_nid_postfix(stream);
    for (int index = 0; index < StreamCount; ++index) std::filesystem::remove("apr_stream_" + std::to_string(index) + ".tmp");
    Require(error.empty(), ("APR read failed while " + std::to_string(streams.size()) + " guest streams were open: " + error).c_str());
    Require(held, "cannot hold 600 guest fopen streams");
}

void TestQueries() {
    constexpr int GuestEinval = 22;
    constexpr int GuestEnoent = 2;
    const char* names[] = {"generated.bin", "missing.bin"};
    std::array<std::uint32_t, 2> ids{0, 0};
    std::array<std::uint64_t, 2> sizes{1, 1};
    std::uint32_t failed = 7;
    Require(sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes("apr_read_", names, 1, ids.data(), sizes.data(), &failed) == 0, "APR did not resolve a prefixed path");
    Require(failed == 7 && sizes[0] == FileBytes, "APR did not report the prefixed file's size");
    const auto id = ids[0];
    ids = {0, 0};
    sizes = {1, 1};
    Require(sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes("apr_read_", names, 2, ids.data(), sizes.data(), &failed) == -1 && *__error_nid_postfix() == GuestEnoent, "APR resolved a missing prefixed path");
    Require(failed == 1 && ids[0] == id && sizes[0] == FileBytes && ids[1] == 0xFFFFFFFFu && sizes[1] == 0, "APR did not mark only the missing prefixed path");
    std::uint64_t size = 0;
    Require(sceKernelAprGetFileSize(id, nullptr) == -1 && *__error_nid_postfix() == GuestEinval, "APR accepted a null file size");
    Require(sceKernelAprGetFileSize(id, &size) == 0 && size == FileBytes, "APR reported a wrong file size");
    FileStat stat{};
    Require(sceKernelAprGetFileStat(id, nullptr) == -1 && *__error_nid_postfix() == GuestEinval, "APR accepted a null file stat");
    Require(sceKernelAprGetFileStat(id, &stat) == 0 && stat.st_size == static_cast<std::int64_t>(FileBytes), "APR reported a wrong file stat size");
    std::vector<std::uint8_t> data(4096, 0xCC);
    const Apr::ReadFileCommand command{{Apr::Opcode::ReadFile, sizeof(Apr::ReadFileCommand)}, id, 0, reinterpret_cast<std::uint64_t>(data.data()), data.size(), 100};
    alignas(8) std::array<std::uint8_t, sizeof(command)> memory{};
    std::memcpy(memory.data(), &command, sizeof(command));
    Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, sizeof(memory), 1, sizeof(memory), memory.data()};
    std::uint32_t result = 0xFFFFFFFFu;
    std::uint32_t first = 0;
    Require(sceKernelAprSubmitCommandBufferAndGetResult(&buffer, 0, nullptr, &first) == -1 && *__error_nid_postfix() == GuestEinval && first == 0, "APR accepted a null result");
    Require(sceKernelAprSubmitCommandBufferAndGetResult(nullptr, 0, &result, &first) == -1 && *__error_nid_postfix() == GuestEinval, "APR accepted a null command buffer");
    Require(sceKernelAprSubmitCommandBufferAndGetResult(&buffer, 0, &result, nullptr) == -1 && *__error_nid_postfix() == GuestEinval, "APR accepted a null submission id");
    Require(result == 0xFFFFFFFFu && first == 0, "a rejected APR submission wrote its result or id");
    Require(sceKernelAprSubmitCommandBufferAndGetResult(&buffer, 0, &result, &first) == 0 && result == 0 && first != 0, "APR did not return a result and an id");
    Require(sceKernelAprWaitCommandBuffer(first) == 0, "APR did not wait for the submission");
    for (std::size_t index = 0; index < data.size(); ++index) Require(data[index] == FileByte(100 + index), "APR read through the submission with a result returned incorrect bytes");
    std::uint32_t second = 0;
    Require(sceKernelAprSubmitCommandBufferAndGetResult(&buffer, 1, &result, &second) == 0 && second == first + 1, "APR submission ids did not count up");
    Require(sceKernelAprWaitCommandBuffer(second) == 0, "APR did not wait for the second submission");
}

#ifdef _WIN32
std::uintptr_t faultBegin = 0;
std::size_t faultBytes = 0;
std::size_t faultPage = 0;
std::size_t committedPages = 0;

LONG CALLBACK CommitOnWrite(EXCEPTION_POINTERS* exception) {
    const auto& record = *exception->ExceptionRecord;
    if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record.NumberParameters < 2 || record.ExceptionInformation[0] != 1) return EXCEPTION_CONTINUE_SEARCH;
    const auto address = static_cast<std::uintptr_t>(record.ExceptionInformation[1]);
    if (address < faultBegin || address - faultBegin >= faultBytes) return EXCEPTION_CONTINUE_SEARCH;
    const auto page = address & ~(static_cast<std::uintptr_t>(faultPage) - 1);
    if (VirtualAlloc(reinterpret_cast<void*>(page), faultPage, MEM_COMMIT, PAGE_READWRITE) == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    ++committedPages;
    return EXCEPTION_CONTINUE_EXECUTION;
}

class FaultBuffer {
public:
    FaultBuffer() {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        pointer = VirtualAlloc(nullptr, MappingBytes, MEM_RESERVE, PAGE_NOACCESS);
        Require(pointer != nullptr, "cannot reserve the fault-backed read buffer");
        handler = AddVectoredExceptionHandler(1, CommitOnWrite);
        if (handler == nullptr) {
            VirtualFree(pointer, 0, MEM_RELEASE);
            throw std::runtime_error("cannot install the synthetic page fault handler");
        }
        faultPage = system.dwPageSize;
        faultBytes = MappingBytes;
        faultBegin = reinterpret_cast<std::uintptr_t>(pointer);
        committedPages = 0;
    }

    ~FaultBuffer() {
        faultBytes = 0;
        RemoveVectoredExceptionHandler(handler);
        VirtualFree(pointer, 0, MEM_RELEASE);
    }

    std::uint8_t* Data() const { return static_cast<std::uint8_t*>(pointer); }

private:
    void* pointer = nullptr;
    void* handler = nullptr;
};

struct SmallStackRead {
    const Fixture* fixture = nullptr;
    std::vector<std::uint8_t>* storage = nullptr;
    std::string error;
};

DWORD WINAPI ReadOnSmallStack(void* parameter) {
    auto& read = *static_cast<SmallStackRead*>(parameter);
    try {
        CheckRead(*read.fixture, read.storage->data() + 17, 65537, 0);
    } catch (const std::exception& failure) {
        read.error = failure.what();
    }
    return 0;
}

void TestSmallStack(const Fixture& fixture) {
    std::vector<std::uint8_t> storage(131200, 0xCC);
    SmallStackRead read{&fixture, &storage, {}};
    const HANDLE thread = CreateThread(nullptr, 64u * 1024u, ReadOnSmallStack, &read, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    Require(thread != nullptr, "cannot start the small-stack APR reader");
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    Require(read.error.empty(), read.error.c_str());
}
#endif

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected host, direct, eof, errors, streams, fault or stack");
        const Fixture fixture;
        const std::string mode = argv[1];
        if (mode == "host") {
            std::vector<std::uint8_t> storage(MappingBytes, 0xCC);
            TestRead(fixture, storage.data());
        } else if (mode == "direct") {
            const DirectBuffer storage;
            TestRead(fixture, storage.Data());
        } else if (mode == "eof") {
            TestEof(fixture);
        } else if (mode == "queries") {
            TestQueries();
        } else if (mode == "errors") {
            TestErrors(fixture);
        } else if (mode == "streams") {
            TestStreams(fixture);
#ifdef _WIN32
        } else if (mode == "fault") {
            const FaultBuffer storage;
            TestRead(fixture, storage.Data());
            Require(committedPages > 2, "APR did not exercise demand-committed destination pages");
        } else if (mode == "stack") {
            TestSmallStack(fixture);
#endif
        } else {
            throw std::runtime_error("unknown APR read case");
        }
        std::puts("APR synthetic read checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
