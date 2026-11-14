#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::size_t checks = 0;
std::size_t cases = 0;
std::size_t attempts = 0;
std::size_t releases = 0;
void* live = nullptr;
std::size_t liveSize = 0;
bool failAllocation = false;
const std::uint32_t* observedOutput = nullptr;
std::uint32_t expectedAtRelease = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    ++checks;
    if (condition) return;
    std::fprintf(stderr, "TLS protocol check failed at line %u, case %zu\n", location.line(), cases);
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    ++attempts;
    Require(live == nullptr);
    if (failAllocation) return nullptr;
    auto* raw = static_cast<unsigned char*>(std::malloc(size + 32));
    Require(raw != nullptr);
    std::memset(raw, 0xa5, size + 32);
    live = raw + 16;
    liveSize = size;
    return live;
}

void APS5_VABI Release_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(pointer == live);
    if (observedOutput) Require(*observedOutput == expectedAtRelease);
    auto* raw = static_cast<unsigned char*>(pointer) - 16;
    for (std::size_t i = 0; i < 16; ++i) Require(raw[i] == 0xa5 && raw[16 + liveSize + i] == 0xa5);
    std::free(raw);
    live = nullptr;
    ++releases;
}

void* APS5_VABI UnexpectedCalloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI UnexpectedRealloc_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected align"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realign"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix align"); }

void Check(const char* input, int status, std::uint32_t expected) {
    ++cases;
    struct Output { std::uint32_t before; std::uint32_t value; std::uint32_t after; } output{0x12345678, 0xcafebeef, 0x87654321};
    const std::string saved = input ? input : "";
    const std::size_t oldAttempts = attempts;
    const std::size_t oldReleases = releases;
    observedOutput = &output.value;
    expectedAtRelease = status == 0 ? expected : 0xcafebeef;
    Require(tls_config_parse_protocols_nid_postfix(&output.value, input) == status);
    observedOutput = nullptr;
    Require(output.value == (status == 0 ? expected : 0xcafebeef));
    Require(output.before == 0x12345678 && output.after == 0x87654321);
    Require(!input || saved == input);
    Require(attempts == oldAttempts + (input ? 1 : 0));
    Require(releases == oldReleases + (input && !failAllocation ? 1 : 0));
    Require(live == nullptr);
    if (input && !failAllocation) Require(liveSize == saved.size() + 1);
}

void KnownCases() {
    Check(nullptr, 0, 0x18);
    const std::array<std::pair<const char*, std::uint32_t>, 24> good{{
        {"all", 0x18}, {"legacy", 0x18}, {"default", 0x18}, {"secure", 0x18}, {"tlsv1", 0x18},
        {"tlsv1.0", 2}, {"tlsv1.1", 4}, {"tlsv1.2", 8}, {"tlsv1.3", 16},
        {" \tTlSv1.2", 8}, {"!tlsv1.3", 8}, {"!tlsv1.2", 16}, {"!all", 0},
        {"tlsv1.0,tlsv1.1:tlsv1.2,tlsv1.3", 30}, {"all:!tlsv1.2", 16},
        {"all:!tlsv1.3", 8}, {"all,!all", 0}, {"!all,!tlsv1.2", 16},
        {"tlsv1.0,!tlsv1.0,!tlsv1.3", 8}, {"!all,!all", 0},
        {"tlsv1.0:!secure", 2}, {"!tlsv1.0", 24}, {"tlsv1.2,tlsv1.2", 8},
        {"\t!default:TLSV1.1", 4}
    }};
    for (const auto& [input, expected] : good) Check(input, 0, expected);
    const std::array bad{"", " ", "\t", "!", "!!all", "! all", "!\tall", "all ", "all\t", "all\n", "\nall", "\rall", "\vall", "\fall", "tlsv1.4", "tlsv1.20", "tlsv1.2x", "TLSv2", "sslv3", "none", "0x18", "*", ",all", "all,", "all::tlsv1.2", "all,,tlsv1.3", "all: \t", "all:bad", "tlsv1.2;tlsv1.3", "tlsv1.2 tlsv1.3", "all:!tlsv1.2:bad", "\xc2\xa0" "all"};
    for (const auto* input : bad) Check(input, -1, 0);
    failAllocation = true;
    Check(nullptr, 0, 0x18);
    for (const auto& [input, expected] : good) Check(input, -1, 0);
    for (const auto* input : bad) Check(input, -1, 0);
    failAllocation = false;
}

struct Term { std::string_view text; std::uint32_t bits; bool remove; };
constexpr std::array<Term, 18> Terms{{
    {"all",24,false},{"legacy",24,false},{"default",24,false},{"secure",24,false},{"tlsv1",24,false},
    {"tlsv1.0",2,false},{"tlsv1.1",4,false},{"tlsv1.2",8,false},{"tlsv1.3",16,false},
    {"!all",24,true},{"!legacy",24,true},{"!default",24,true},{"!secure",24,true},{"!tlsv1",24,true},
    {"!tlsv1.0",2,true},{"!tlsv1.1",4,true},{"!tlsv1.2",8,true},{"!tlsv1.3",16,true}
}};

std::uint32_t Expected(const std::array<Term, 3>& terms) {
    std::array<bool, 4> enabled{};
    for (const auto& term : terms) {
        if (term.remove && enabled == std::array<bool, 4>{}) enabled = {false, false, true, true};
        for (std::size_t bit = 0; bit < enabled.size(); ++bit) {
            if (term.bits & (2u << bit)) enabled[bit] = !term.remove;
        }
    }
    std::uint32_t mask = 0;
    for (std::size_t bit = 0; bit < enabled.size(); ++bit) if (enabled[bit]) mask += 2u << bit;
    return mask;
}

void Combinations() {
    for (const auto& first : Terms) for (const auto& second : Terms) for (const auto& third : Terms) {
        std::string input = std::string(first.text) + ", \t" + std::string(second.text) + ":\t" + std::string(third.text);
        for (std::size_t i = 0; i < input.size(); ++i) if ((i & 1) && input[i] >= 'a' && input[i] <= 'z') input[i] -= 'a' - 'A';
        Check(input.c_str(), 0, Expected({first, second, third}));
    }
}

void CasePermutations() {
    for (std::size_t term = 0; term < 9; ++term) {
        std::string word(Terms[term].text);
        for (unsigned mask = 0; mask < (1u << word.size()); ++mask) {
            std::string input(word);
            for (std::size_t index = 0; index < word.size(); ++index) if ((mask & (1u << index)) && word[index] >= 'a' && word[index] <= 'z') input[index] -= 'a' - 'A';
            Check(input.c_str(), 0, Terms[term].bits);
        }
    }
    for (unsigned byte = 1; byte < 256; ++byte) {
        std::string input(1, static_cast<char>(byte));
        input += "all";
        if (byte == ' ' || byte == '\t') Check(input.c_str(), 0, 24);
        else if (byte == '!') Check(input.c_str(), 0, 0);
        else Check(input.c_str(), -1, 0);
    }
}

void Boundaries() {
    std::string longList;
    for (int index = 0; index < 1024; ++index) longList += "tlsv1.0,!tlsv1.0,";
    longList += "!tlsv1.2";
    Check(longList.c_str(), 0, 16);
    longList += ",bad";
    Check(longList.c_str(), -1, 0);
    const std::array<char, 10> embedded{'a','l','l','\0',',','b','a','d','\0','x'};
    Check(embedded.data(), 0, 24);
    std::array<std::uint32_t, 8> alias{};
    auto* text = reinterpret_cast<char*>(alias.data());
    std::memcpy(text, "tlsv1.0,!tlsv1.0,!tlsv1.2", sizeof("tlsv1.0,!tlsv1.0,!tlsv1.2"));
    const auto before = alias;
    Require(tls_config_parse_protocols_nid_postfix(&alias[1], text) == 0);
    Require(alias[1] == 16);
    for (std::size_t i = 0; i < alias.size(); ++i) if (i != 1) Require(alias[i] == before[i]);
    for (const auto* input : std::array<const char*, 3>{nullptr, "all", "bad"}) {
        const auto oldAttempts = attempts;
        bool caught = false;
        try { tls_config_parse_protocols_nid_postfix(nullptr, input); }
        catch (const std::invalid_argument&) { caught = true; }
        Require(caught && attempts == oldAttempts && live == nullptr);
    }
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Release_nid_no_patch), reinterpret_cast<void*>(UnexpectedCalloc_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedRealloc_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    KnownCases();
    Combinations();
    CasePermutations();
    Boundaries();
    Require(live == nullptr);
    std::printf("TLS protocol parser: %zu cases, %zu checks, %zu releases, zero live allocations\n", cases, checks, releases);
}
