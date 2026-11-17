#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <thread>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU tokenization check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
void Throws(TCall call) {
    try { call(); }
    catch (const std::exception&) { return; }
    Require(false);
}

void SetSearches() {
    const std::array<char16_t, 9> text{'a', 0xd83d, 0xde00, 'b', 0xd800, 'c', 0xdc00, 'd', 0};
    const auto before = text;
    const std::array<char16_t, 4> set{0xd83d, 0xde00, 0xd800, 0};
    const std::array<char16_t, 2> lead{0xd83d, 0};
    const std::array<char16_t, 2> trail{0xdc00, 0};
    Require(u_strspn_nid_postfix(text.data(), set.data()) == 0);
    Require(u_strspn_nid_postfix(text.data() + 1, set.data()) == 2);
    Require(u_strcspn_nid_postfix(text.data(), set.data()) == 1);
    Require(u_strpbrk_nid_postfix(text.data(), set.data()) == text.data() + 1);
    Require(u_strcspn_nid_postfix(text.data(), lead.data()) == 8);
    Require(u_strpbrk_nid_postfix(text.data(), lead.data()) == nullptr);
    Require(u_strcspn_nid_postfix(text.data(), trail.data()) == 6);
    Require(u_strpbrk_nid_postfix(text.data(), trail.data()) == text.data() + 6);
    Require(u_strspn_nid_postfix(text.data(), u"abcd") == 1);
    Require(u_strspn_nid_postfix(u"", set.data()) == 0);
    Require(u_strcspn_nid_postfix(u"", set.data()) == 0);
    Require(u_strspn_nid_postfix(text.data(), u"") == 0);
    Require(u_strcspn_nid_postfix(text.data(), u"") == 8);
    Require(u_strpbrk_nid_postfix(text.data(), u"") == nullptr);
    Require(text == before);
    Throws([&] { u_strspn_nid_postfix(nullptr, set.data()); });
    Throws([&] { u_strcspn_nid_postfix(text.data(), nullptr); });
    Throws([&] { u_strpbrk_nid_postfix(nullptr, nullptr); });
}

void ReverseSearch() {
    const std::array<char16_t, 9> text{'a', 0, 0xd800, 0xdc00, 'b', 0xd800, 0, 0xdc00, 'a'};
    const auto before = text;
    Require(u_memrchr32_nid_postfix(text.data(), 'a', 9) == text.data() + 8);
    Require(u_memrchr32_nid_postfix(text.data(), 0, 9) == text.data() + 6);
    Require(u_memrchr32_nid_postfix(text.data(), 0x10000, 9) == text.data() + 2);
    Require(u_memrchr32_nid_postfix(text.data(), 0xd800, 9) == text.data() + 5);
    Require(u_memrchr32_nid_postfix(text.data(), 0xdc00, 9) == text.data() + 7);
    Require(u_memrchr32_nid_postfix(text.data(), 0xd800, 3) == text.data() + 2);
    Require(u_memrchr32_nid_postfix(text.data(), 0x10000, 3) == nullptr);
    Require(u_memrchr32_nid_postfix(text.data() + 3, 0xdc00, 1) == text.data() + 3);
    Require(u_memrchr32_nid_postfix(nullptr, 'a', 0) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, 'a', -1) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, 0x10000, 1) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, 0xd800, 7) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, 0xdfff, 7) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, -1, 7) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, 0x110000, 7) == nullptr);
    Require(u_memrchr32_nid_postfix(nullptr, std::numeric_limits<std::int32_t>::max(), 7) == nullptr);
    Throws([&] { u_memrchr32_nid_postfix(nullptr, 'a', 1); });
    Require(text == before);
}

void Tokens() {
    std::array<char16_t, 10> text{0xa55a, ',', 'a', ':', 'b', ',', 'c', 0, 0xa55a, 0xa55a};
    char16_t* state = nullptr;
    Require(u_strtok_r_nid_postfix(text.data() + 1, u",:", &state) == text.data() + 2);
    Require(state == text.data() + 4 && text[3] == 0 && text[1] == ',');
    Require(u_strtok_r_nid_postfix(nullptr, u",", &state) == text.data() + 4);
    Require(state == text.data() + 6 && text[5] == 0);
    Require(u_strtok_r_nid_postfix(nullptr, u"", &state) == text.data() + 6);
    Require(state == nullptr);
    Require(u_strtok_r_nid_postfix(nullptr, nullptr, &state) == nullptr);
    Require(text.front() == 0xa55a && text[8] == 0xa55a && text[9] == 0xa55a);
    std::array<char16_t, 4> delimitersOnly{',', ',', ',', 0};
    Require(u_strtok_r_nid_postfix(delimitersOnly.data(), u",", &state) == nullptr && state == nullptr);
    const std::array<char16_t, 4> unchanged{',', ',', ',', 0};
    Require(delimitersOnly == unchanged);
    Throws([&] { u_strtok_r_nid_postfix(text.data(), u",", nullptr); });
    const auto before = text;
    Throws([&] { u_strtok_r_nid_postfix(text.data(), nullptr, &state); });
    Require(text == before && state == nullptr);
}

void SupplementaryTokens() {
    std::array<char16_t, 7> text{0xa55a, 'a', 0xd83d, 0xde00, 'b', 0, 0xa55a};
    const char16_t delimiter[]{0xd83d, 0xde00, 0};
    char16_t* state = nullptr;
    Require(u_strtok_r_nid_postfix(text.data() + 1, delimiter, &state) == text.data() + 1);
    Require(text[2] == 0 && text[3] == 0xde00 && state == text.data() + 3);
    Require(u_strtok_r_nid_postfix(nullptr, delimiter, &state) == text.data() + 3);
    Require(state == nullptr && text.front() == 0xa55a && text.back() == 0xa55a);
    std::array<char16_t, 4> leading{0xd83d, 0xde00, 'a', 0};
    Require(u_strtok_r_nid_postfix(leading.data(), delimiter, &state) == leading.data() + 2 && state == nullptr);
    Require(leading[0] == 0xd83d && leading[1] == 0xde00);
    std::array<char16_t, 4> pair{0xd83d, 0xde00, 'b', 0};
    const char16_t unpaired[]{0xd83d, 0};
    Require(u_strtok_r_nid_postfix(pair.data(), unpaired, &state) == pair.data() && state == nullptr);
    Require(pair[0] == 0xd83d && pair[1] == 0xde00);
}

void AliasedDelimiters() {
    std::array<char16_t, 7> text{'a', ',', 'b', ',', 'c', 0, 0xa55a};
    auto* delimiters = text.data() + 3;
    char16_t* state = nullptr;
    Require(u_strtok_r_nid_postfix(text.data(), delimiters, &state) == text.data());
    Require(state == text.data() + 2 && text[1] == 0 && delimiters[0] == ',');
    Require(u_strtok_r_nid_postfix(nullptr, delimiters, &state) == text.data() + 2);
    Require(state == text.data() + 4 && delimiters[0] == 0);
    Require(u_strtok_r_nid_postfix(nullptr, delimiters, &state) == text.data() + 4);
    Require(state == nullptr && text[4] == 'c' && text.back() == 0xa55a);
}

class GuestStorage {
public:
    static constexpr std::size_t Page = 0x4000;

    GuestStorage() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page * 3, 0, 0, &physical) == 0);
        Require(sceKernelMapDirectMemory(&mapping, Page * 3, 3, 0, physical, 0) == 0);
        Require(GuestArena::GuestArenaContains_nid_postfix(mapping, Page * 3));
        std::fill_n(static_cast<unsigned char*>(mapping), Page * 3, 0xa5);
    }

    ~GuestStorage() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), Page * 3) == 0);
        Require(sceKernelReleaseDirectMemory(physical, Page * 3) == 0);
    }

    char16_t* Text() const { return reinterpret_cast<char16_t*>(static_cast<unsigned char*>(mapping) + 16); }
    char16_t** State() const { return reinterpret_cast<char16_t**>(static_cast<unsigned char*>(mapping) + Page + 16); }
    char16_t* Delimiters() const { return reinterpret_cast<char16_t*>(static_cast<unsigned char*>(mapping) + Page * 2 + 16); }

    void Protect(std::size_t page, int protection) {
        Require(sceKernelMprotect(static_cast<unsigned char*>(mapping) + Page * page, Page, protection) == 0);
    }

    void CheckCanaries() const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        for (const auto offset : {std::size_t{15}, Page + 15, Page + 16 + sizeof(char16_t*), Page * 2 + 15, Page * 3 - 1})
            Require(bytes[offset] == 0xa5);
    }

private:
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

void GuestMappedTokens() {
    GuestStorage storage;
    auto* text = storage.Text();
    auto** state = storage.State();
    auto* delimiters = storage.Delimiters();
    const std::array<char16_t, 9> initial{0xa55a, ',', 'a', ':', 'b', ',', 'c', 0, 0xa55a};
    std::copy(initial.begin(), initial.end(), text);
    const std::array<char16_t, 3> separators{',', ':', 0};
    std::copy(separators.begin(), separators.end(), delimiters);
    *state = nullptr;
    storage.Protect(2, 1);
    Require(u_strtok_r_nid_postfix(text + 1, delimiters, state) == text + 2);
    Require(*state == text + 4 && text[3] == 0 && text[1] == ',');
    Require(u_strtok_r_nid_postfix(nullptr, delimiters, state) == text + 4);
    Require(*state == text + 6 && text[5] == 0);
    Require(u_strtok_r_nid_postfix(nullptr, delimiters, state) == text + 6 && *state == nullptr);
    Require(u_strtok_r_nid_postfix(nullptr, nullptr, state) == nullptr);
    Require(text[0] == 0xa55a && text[8] == 0xa55a);
    Require(std::equal(separators.begin(), separators.end(), delimiters));
    storage.CheckCanaries();
    storage.Protect(2, 3);
    const std::array<char16_t, 7> supplementary{0xa55a, 'a', 0xd83d, 0xde00, 'b', 0, 0xa55a};
    std::copy(supplementary.begin(), supplementary.end(), text);
    const std::array<char16_t, 3> pair{0xd83d, 0xde00, 0};
    std::copy(pair.begin(), pair.end(), delimiters);
    storage.Protect(2, 1);
    Require(u_strtok_r_nid_postfix(text + 1, delimiters, state) == text + 1);
    Require(text[2] == 0 && text[3] == 0xde00 && *state == text + 3);
    Require(u_strtok_r_nid_postfix(nullptr, delimiters, state) == text + 3 && *state == nullptr);
    Require(text[0] == 0xa55a && text[6] == 0xa55a);
    Require(std::equal(pair.begin(), pair.end(), delimiters));
    storage.CheckCanaries();
}

void ReadOnlyGuestTokens() {
#ifdef _WIN32
    GuestStorage storage;
    auto* text = storage.Text();
    auto** state = storage.State();
    const std::array<char16_t, 5> initial{0xa55a, 'a', ',', 'b', 0};
    std::copy(initial.begin(), initial.end(), text);
    *state = nullptr;
    storage.Protect(1, 1);
    {
        GuestArena::HostWrite rejected(state, sizeof(*state));
        Require(!rejected.Open());
    }
    Require(u_strtok_r_nid_postfix(nullptr, nullptr, state) == nullptr);
    Throws([&] { u_strtok_r_nid_postfix(text + 1, u",", state); });
    Require(*state == nullptr && std::equal(initial.begin(), initial.end(), text));
    storage.Protect(1, 3);
    *state = text + 1;
    storage.Protect(1, 1);
    Throws([&] { u_strtok_r_nid_postfix(nullptr, u",", state); });
    Require(*state == text + 1 && std::equal(initial.begin(), initial.end(), text));
    storage.Protect(1, 3);
    storage.Protect(0, 1);
    {
        GuestArena::HostWrite rejected(text + 2, sizeof(*text));
        Require(!rejected.Open());
    }
    Throws([&] { u_strtok_r_nid_postfix(nullptr, u",", state); });
    Require(*state == text + 1 && std::equal(initial.begin(), initial.end(), text));
    *state = nullptr;
    Throws([&] { u_strtok_r_nid_postfix(text + 1, u",", state); });
    Require(*state == text + 1 && std::equal(initial.begin(), initial.end(), text));
    Require(u_strtok_r_nid_postfix(nullptr, u"", state) == text + 1 && *state == nullptr);
    Require(std::equal(initial.begin(), initial.end(), text));
    Require(u_strtok_r_nid_postfix(text + 2, u",b", state) == nullptr && *state == nullptr);
    Require(u_strtok_r_nid_postfix(text + 4, u",", state) == nullptr && *state == nullptr);
    Require(std::equal(initial.begin(), initial.end(), text));
    storage.CheckCanaries();
#endif
}

void IndependentState() {
    const auto tokenize = [](char16_t separator) {
        for (unsigned iteration = 0; iteration < 1000; ++iteration) {
            std::array<char16_t, 4> text{'a', separator, 'b', 0};
            const char16_t delimiters[]{separator, 0};
            char16_t* state = nullptr;
            Require(u_strtok_r_nid_postfix(text.data(), delimiters, &state) == text.data());
            Require(u_strtok_r_nid_postfix(nullptr, delimiters, &state) == text.data() + 2);
            Require(state == nullptr && text[1] == 0 && text[3] == 0);
        }
    };
    std::thread first(tokenize, ',');
    std::thread second(tokenize, ':');
    first.join();
    second.join();
}

} // namespace

int main() {
    SetSearches();
    ReverseSearch();
    Tokens();
    SupplementaryTokens();
    AliasedDelimiters();
    GuestMappedTokens();
    ReadOnlyGuestTokens();
    IndependentState();
}
