#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestLibcStrtofflags check failed\n", stderr);
        std::abort();
    }
}

constexpr std::uint64_t UfNodump = 0x00000001;
constexpr std::uint64_t UfImmutable = 0x00000002;
constexpr std::uint64_t UfAppend = 0x00000004;
constexpr std::uint64_t UfHidden = 0x00008000;
constexpr std::uint64_t SfImmutable = 0x00020000;
constexpr std::uint64_t SfNounlink = 0x00100000;

int main() {
    char first[] = "uchg,hidden";
    char* text = first;
    std::uint64_t set = ~std::uint64_t{0};
    std::uint64_t clear = ~std::uint64_t{0};
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == (UfImmutable | UfHidden) && clear == 0);
    Require(std::strcmp(text, "hidden") == 0);

    char nodump[] = "nodump";
    text = nodump;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == UfNodump && clear == 0);

    char dump[] = "dump";
    text = dump;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == 0 && clear == UfNodump);

    char noschg[] = "noschg";
    text = noschg;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == 0 && clear == SfImmutable);

    char schg[] = "schg";
    text = schg;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == SfImmutable && clear == 0);

    char mixed[] = "\tuappnd , ,nosunlink\t";
    text = mixed;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 0);
    Require(set == UfAppend && clear == SfNounlink);

    char unknown[] = "nouchg,bogus,hidden";
    text = unknown;
    Require(strtofflags_nid_postfix(&text, &set, &clear) == 1);
    Require(set == 0 && clear == UfImmutable);
    Require(std::strcmp(text, "bogus") == 0);

    char shared[] = "uchg,nohidden";
    text = shared;
    Require(strtofflags_nid_postfix(&text, &set, &set) == 0);
    Require(set == (UfImmutable | UfHidden));

    char sharedUnknown[] = "hidden,bogus";
    text = sharedUnknown;
    Require(strtofflags_nid_postfix(&text, &set, &set) == 1);
    Require(set == UfHidden && std::strcmp(text, "bogus") == 0);

    char empty[] = "";
    text = empty;
    Require(strtofflags_nid_postfix(&text, nullptr, nullptr) == 0);
    Require(std::strcmp(text, "") == 0);

    char nullOutputs[] = "uchg";
    text = nullOutputs;
    Require(strtofflags_nid_postfix(&text, nullptr, nullptr) == 0);

    bool threw = false;
    try {
        strtofflags_nid_postfix(nullptr, &set, &clear);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
