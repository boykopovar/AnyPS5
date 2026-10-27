#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <source_location>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using SelectProtocol = int (APS5_VABI*)(unsigned char**, unsigned char*, const unsigned char*, std::uint32_t, const unsigned char*, std::uint32_t);
using Protocol = std::vector<unsigned char>;
using Protocols = std::vector<Protocol>;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "LibreSSL protocol check failed at line %u\n", location.line());
        std::abort();
    }
}

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

void Run(SelectProtocol select) {
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

    std::printf("LibreSSL protocol selection: 7056 generated pairs, and valid-list boundaries passed\n");
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
