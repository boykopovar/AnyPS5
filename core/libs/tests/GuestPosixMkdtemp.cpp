#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

static void Require(bool condition) { if (!condition) std::abort(); }

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::filesystem::path base = argv[1];
    std::filesystem::remove_all(base);
    Require(std::filesystem::create_directory(base));

    std::string first = (base / "tmpXXXXXX").string();
    char* created = mkdtemp_nid_postfix(first.data());
    Require(created == first.data());
    Require(first.size() == (base / "tmpXXXXXX").string().size());
    Require(first.compare(first.size() - 6, 6, "XXXXXX") != 0);
    Require(std::filesystem::is_directory(first));

    std::string second = (base / "tmpXXXXXX").string();
    Require(mkdtemp_nid_postfix(second.data()) == second.data());
    Require(second != first && std::filesystem::is_directory(second));

    std::string fixedTemplate = (base / "sameXXXXXX").string();
    Require(mkdtemp_nid_postfix(fixedTemplate.data()) == fixedTemplate.data());

    std::string unchangedTemplate = (base / "plain").string();
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(unchangedTemplate.data()) == nullptr);
    Require(*__error_nid_postfix() == 22);

    std::string shortTemplate = "XXXXX";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(shortTemplate.data()) == nullptr);
    Require(*__error_nid_postfix() == 22 && shortTemplate == "XXXXX");

    std::string missingParent = (base / "absent" / "tmpXXXXXX").string();
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(missingParent.data()) == nullptr);
    Require(*__error_nid_postfix() == 2);

    bool threw = false;
    try {
        mkdtemp_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::filesystem::remove_all(base);
    return 0;
}
