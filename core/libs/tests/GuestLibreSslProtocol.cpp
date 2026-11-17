#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

using SelectProtocol = int (APS5_VABI*)(unsigned char**, unsigned char*, const unsigned char*, std::uint32_t, const unsigned char*, std::uint32_t);
using Protocol = std::vector<unsigned char>;
using Protocols = std::vector<Protocol>;

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; } catch (const std::exception&) {}
    Require(false, location);
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial = nullptr, std::size_t size = 0) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page_ = info.dwPageSize;
        base_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base_ != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base_ + page_, page_, PAGE_NOACCESS, &previous) != 0);
#else
        page_ = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* const mapped = mmap(nullptr, 2 * page_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base_ = static_cast<unsigned char*>(mapped);
        Require(mprotect(base_ + page_, page_, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base_, 0, MEM_RELEASE);
#else
        munmap(base_, 2 * page_);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    unsigned char* End() const { return base_ + page_; }

    unsigned char* Place(std::initializer_list<unsigned char> bytes) {
        unsigned char* const start = End() - bytes.size();
        std::copy(bytes.begin(), bytes.end(), start);
        return start;
    }

private:
    unsigned char* base_ = nullptr;
    std::size_t page_ = 0;
};

Protocol Encode(const Protocols& names) {
    Protocol result;
    for (const auto& name : names) {
        Require(!name.empty() && name.size() <= 255);
        result.push_back(static_cast<unsigned char>(name.size()));
        result.insert(result.end(), name.begin(), name.end());
    }
    return result;
}

void CheckSelection(SelectProtocol select, const Protocols& peerNames, const Protocols& supportedNames) {
    const auto peer = Encode(peerNames);
    const auto supported = Encode(supportedNames);
    const auto peerCopy = peer;
    const auto supportedCopy = supported;
    const unsigned char* expected = supported.data() + 1;
    std::size_t expectedLength = supportedNames.front().size();
    int expectedStatus = 2;
    std::size_t offset = 1;
    for (const auto& name : peerNames) {
        if (std::find(supportedNames.begin(), supportedNames.end(), name) != supportedNames.end()) {
            expected = peer.data() + offset;
            expectedLength = name.size();
            expectedStatus = 1;
            break;
        }
        offset += name.size() + 1;
    }
    unsigned char* output = nullptr;
    std::array<unsigned char, 3> length{0xC1, 0xA5, 0xC2};
    Require(select(&output, &length[1], peer.data(), static_cast<std::uint32_t>(peer.size()),
        supported.data(), static_cast<std::uint32_t>(supported.size())) == expectedStatus);
    Require(output == expected && length[1] == expectedLength);
    Require(length[0] == 0xC1 && length[2] == 0xC2);
    Require(peer == peerCopy && supported == supportedCopy);
}

void CheckRaw(SelectProtocol select, const unsigned char* peer, std::uint32_t peerLength, const unsigned char* supported,
    std::uint32_t supportedLength, int expectedStatus, const unsigned char* expectedOutput, unsigned char expectedLength,
    std::source_location location = std::source_location::current()) {
    unsigned char* output = nullptr;
    unsigned char length = 0xA5;
    Require(select(&output, &length, peer, peerLength, supported, supportedLength) == expectedStatus, location);
    Require(output == expectedOutput && length == expectedLength, location);
}

void RunValid(SelectProtocol select) {
    const Protocol http{'h', 't', 't', 'p', '/', '1', '.', '1'};
    const Protocol h2{'h', '2'};
    const Protocol h3{'h', '3'};
    CheckSelection(select, {h2, http}, {http, h2});
    CheckSelection(select, {http, h2}, {h2, http});
    CheckSelection(select, {h3}, {http, h2});
    CheckSelection(select, {h2, h2, http}, {http, h2, h2});
    CheckSelection(select, {{0, 0xFF, 0x80}, {'a', 0, 'b'}}, {{'a', 0, 'b'}, {0, 0xFF, 0x80}});
    CheckSelection(select, {{'h', '2', 0}, {'H', '2'}}, {h2, http});
    Protocol longest(255);
    for (std::size_t index = 0; index < longest.size(); ++index) longest[index] = static_cast<unsigned char>(index);
    CheckSelection(select, {longest}, {h2, longest});
    CheckSelection(select, {h2}, {longest});

    const Protocols alphabet{{'a'}, {'a', 'b'}, {'b'}, {0, 0xFF}};
    std::vector<Protocols> lists;
    for (std::size_t count = 1, combinations = alphabet.size(); count <= 3; ++count, combinations *= alphabet.size()) {
        for (std::size_t code = 0; code < combinations; ++code) {
            Protocols names;
            for (std::size_t index = 0, remaining = code; index < count; ++index, remaining /= alphabet.size())
                names.push_back(alphabet[remaining % alphabet.size()]);
            lists.push_back(std::move(names));
        }
    }
    for (const auto& peer : lists)
        for (const auto& supported : lists) CheckSelection(select, peer, supported);

    Protocols large(33000, Protocol{'x'});
    large.push_back(h2);
    CheckSelection(select, large, {h2});
    CheckSelection(select, {h2}, large);
}

void RunReported(SelectProtocol select) {
    GuardedPages peerPages;
    GuardedPages supportedPages;
    {
        const auto* peer = peerPages.Place({0, 2, 'h', '2'});
        const auto* supported = supportedPages.Place({0});
        CheckRaw(select, peer, 4, supported, 1, 1, peer + 1, 0);
    }
    {
        const auto* peer = peerPages.Place({0, 2, 'h', '2'});
        const auto* supported = supportedPages.Place({2, 'h', '2'});
        CheckRaw(select, peer, 4, supported, 3, 1, peer + 2, 2);
    }
    {
        const auto* peer = peerPages.Place({5, 'a', 'b'});
        const auto* supported = supportedPages.Place({2, 'a', 'b'});
        CheckRaw(select, peer, 3, supported, 3, 2, supported + 1, 2);
    }
    {
        const auto* peer = peerPages.Place({2, 'h', '2'});
        const auto* supported = supportedPages.Place({5, 'a', 'b'});
        CheckRaw(select, peer, 3, supported, 3, 2, supported + 1, 5);
    }
    {
        const auto* supported = supportedPages.Place({2, 'h', '2'});
        CheckRaw(select, peerPages.End(), 0, supported, 3, 2, supported + 1, 2);
        CheckRaw(select, nullptr, 0, supported, 3, 2, supported + 1, 2);
    }
    {
        const auto* peer = peerPages.Place({2, 'h', '2'});
        const auto* supported = supportedPages.Place({3, 'x', 'y', 'z'});
        CheckRaw(select, peer, 3, supported, 0, 2, supported + 1, 3);
    }
    unsigned char* output = nullptr;
    unsigned char length = 0;
    const unsigned char list[]{2, 'h', '2'};
    RequireInvalidArgument([&] { select(nullptr, &length, list, 3, list, 3); });
    RequireInvalidArgument([&] { select(&output, nullptr, list, 3, list, 3); });
    RequireInvalidArgument([&] { select(&output, &length, nullptr, 3, list, 3); });
    RequireInvalidArgument([&] { select(&output, &length, list, 3, nullptr, 3); });
    RequireInvalidArgument([&] { select(&output, &length, list, 3, nullptr, 0); });
    Require(output == nullptr && length == 0);
}

void RunGuestFaults(SelectProtocol select) {
    GuardedPages peerPages;
    GuardedPages supportedPages;
    const unsigned char validList[]{2, 'h', '2'};
    unsigned char sentinelByte = 0x5A;
    unsigned char* const sentinelOutput = &sentinelByte;
    unsigned char* output = sentinelOutput;
    unsigned char length = 0xA5;
    ReadOnlyPage roOut(&sentinelOutput, sizeof(sentinelOutput));
    ReadOnlyPage roLen(&length, sizeof(length));

    errno = 73;
    ExpectErr<std::runtime_error>([&] { select(&output, &length, peerPages.End(), 1, validList, sizeof(validList)); });
    Require(errno == 73 && output == sentinelOutput && length == 0xA5);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { select(&output, &length, validList, sizeof(validList), supportedPages.End(), 1); });
    Require(errno == 73 && output == sentinelOutput && length == 0xA5);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { select(roOut.As<unsigned char*>(), &length, validList, sizeof(validList), validList, sizeof(validList)); });
    Require(errno == 73 && length == 0xA5);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { select(&output, roLen.As<unsigned char>(), validList, sizeof(validList), validList, sizeof(validList)); });
    Require(errno == 73 && output == sentinelOutput);
}

void Run(SelectProtocol select) {
    RunValid(select);
    RunReported(select);
    RunGuestFaults(select);
    std::printf("LibreSSL protocol selection: 7056 generated pairs, valid-list boundaries and the reported unvalidated lists passed\n");
}

int main(int argc, char** argv) {
    Require(argc == 3);
#ifdef _WIN32
    const auto library = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library) std::fprintf(stderr, "LoadLibraryExW failed: %lu\n", GetLastError());
    Require(library != nullptr);
    const auto select = std::bit_cast<SelectProtocol>(GetProcAddress(library, argv[2]));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr);
    const auto select = reinterpret_cast<SelectProtocol>(dlsym(library, argv[2]));
#endif
    Require(select != nullptr);
    Run(select);
}
