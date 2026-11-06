#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern int opterr_nid_postfix;
extern int optind_nid_postfix;
extern int optreset_nid_postfix;
extern char* optarg_nid_postfix;
}

namespace {

void Expect(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

}

int main() {
    const char* previous = getenv_nid_postfix("POSIXLY_CORRECT");
    const bool hadPrevious = previous != nullptr;
    const std::string saved = previous ? previous : "";
    Expect(unsetenv_nid_postfix("POSIXLY_CORRECT") == 0, "clear POSIXLY_CORRECT");

    std::array<std::string, 7> storage {"prog", "first", "second", "--required", "one", "--required", "two"};
    std::array<char*, 8> arguments {};
    for (std::size_t i = 0; i < storage.size(); ++i) arguments[i] = storage[i].data();
    const auto original = arguments;
    const GuestOption options[] = {
        {"required", 1, nullptr, 'r'},
        {nullptr, 0, nullptr, 0},
    };
    optind_nid_postfix = optreset_nid_postfix = 1;
    opterr_nid_postfix = 0;
    int index = -1;
    Expect(getopt_long_nid_postfix(7, arguments.data(), "", options, &index) == 'r', "first required option");
    Expect(index == 0 && optarg_nid_postfix == original[4] && optind_nid_postfix == 5, "first required value");
    index = -1;
    Expect(getopt_long_nid_postfix(7, arguments.data(), "", options, &index) == 'r', "second required option");
    Expect(index == 0 && optarg_nid_postfix == original[6] && optind_nid_postfix == 7, "second required value");
    index = -1;
    Expect(getopt_long_nid_postfix(7, arguments.data(), "", options, &index) == -1, "permutation completes");
    Expect(optind_nid_postfix == 5 && optarg_nid_postfix == nullptr && index == -1, "final parser state");
    const std::array<std::size_t, 8> order {0, 3, 4, 5, 6, 1, 2, 7};
    for (std::size_t i = 0; i < order.size(); ++i)
        Expect(arguments[i] == original[order[i]], "stable pointer order");
    const std::array<const char*, 7> texts {"prog", "first", "second", "--required", "one", "--required", "two"};
    for (std::size_t i = 0; i < storage.size(); ++i)
        Expect(std::strcmp(storage[i].c_str(), texts[i]) == 0, "argument text stays unchanged");

    Expect(hadPrevious ? setenv_nid_postfix("POSIXLY_CORRECT", saved.c_str(), 1) == 0
                       : unsetenv_nid_postfix("POSIXLY_CORRECT") == 0,
           "restore POSIXLY_CORRECT");
}
