#include <nid/NidCompute.hpp>
#include <nid/Sha1.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

void Require(bool condition) {
    if (!condition) throw std::runtime_error("NID compute tests failed");
}

std::vector<std::uint8_t> Bytes(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::string Hex(const std::array<std::uint8_t, 20>& digest) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2u);
    for (const std::uint8_t byte : digest) {
        result.push_back(kDigits[byte >> 4u]);
        result.push_back(kDigits[byte & 0x0fu]);
    }
    return result;
}

void RequireSha1(const std::string& text, const std::string& expected) {
    Require(Hex(Nid::Sha1(Bytes(text))) == expected);
}

void RequireNid(const std::string& name, const std::string& expected) {
    const std::string nid = Nid::ComputeNid(name, "libkernel");
    Require(nid == expected);
    Require(nid.size() == 11u);
    for (const char character : nid)
        Require(kAlphabet.find(character) != std::string_view::npos);
}

template<typename TAction>
void RequireRejected(TAction action) {
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid symbol name was accepted");
}

}

int main() {
    try {
        RequireSha1("", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        RequireSha1("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
        RequireSha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
        RequireSha1("The quick brown fox jumps over the lazy dog",
            "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

        struct NidCase {
            std::string name;
            std::string expected;
        };
        const NidCase cases[] = {
            {"sceKernelGetProcessTimeWide", "IXMJh8CeS7Q"},
            {"sceKernelGetSystemTimeWide", "rD34HcfvxVY"},
            {"sceNetRecv", "9wO9XrMsNhc"},
            {"sceKernelAllocateDirectMemory", "rTXw65xmLIA"},
            {"sceSysmoduleLoadModule", "g8cM39EUZ6o"},
            {"sceUserServiceGetLoginUserIdList", "fPhymKNvK-A"},
            {"scePadReadState", "YndgXqQVV7c"},
            {"!", "Ge-sIz2M8MQ"},
            {"sceNet_Recv@1?*+/-", "i+c7BYx+xtI"},
            {std::string(200, 'a'), "f0ag4GA-q9A"},
        };
        for (const NidCase& testCase : cases)
            RequireNid(testCase.name, testCase.expected);

        RequireRejected([] { Nid::ComputeNid("", "libkernel"); });
        Require(Nid::ComputeNid("sceNetRecv", "") == Nid::ComputeNid("sceNetRecv", "libSceNet"));
        std::cout << "NID compute tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
