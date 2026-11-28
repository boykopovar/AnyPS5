#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

    std::string plainTemplate = (base / "plain").string();
    Require(mkdtemp_nid_postfix(plainTemplate.data()) == plainTemplate.data());
    Require(std::filesystem::is_directory(base / "plain"));
    *__error_nid_postfix() = 0;
    std::string plainAgain = (base / "plain").string();
    Require(mkdtemp_nid_postfix(plainAgain.data()) == nullptr);
    Require(*__error_nid_postfix() == 17);

    std::string shortTemplate = (base / "shortXXXXX").string();
    Require(mkdtemp_nid_postfix(shortTemplate.data()) == shortTemplate.data());
    Require(shortTemplate.size() == (base / "shortXXXXX").string().size());
    Require(shortTemplate.compare(shortTemplate.size() - 5, 5, "XXXXX") != 0);
    Require(std::filesystem::is_directory(shortTemplate));

    std::string wideTemplate = (base / "wideXXXXXXXX").string();
    Require(mkdtemp_nid_postfix(wideTemplate.data()) == wideTemplate.data());
    Require(wideTemplate.compare(wideTemplate.size() - 8, 8, "XXXXXXXX") != 0);
    Require(std::filesystem::is_directory(wideTemplate));

    const std::string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (std::size_t index = 1; index < alphabet.size(); ++index)
        std::filesystem::create_directory(base / ("retry" + alphabet.substr(index, 1)));
    std::string retryTemplate = (base / "retryX").string();
    Require(mkdtemp_nid_postfix(retryTemplate.data()) == retryTemplate.data());
    Require(retryTemplate == (base / "retry0").string() && std::filesystem::is_directory(base / "retry0"));

    std::string exhaustedTemplate = (base / "retryX").string();
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(exhaustedTemplate.data()) == nullptr);
    Require(*__error_nid_postfix() == 17);

    std::ofstream(base / "regular") << "x";
    std::string notDirectory = (base / "regular" / "tmpXXXXXX").string();
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(notDirectory.data()) == nullptr);
    Require(*__error_nid_postfix() == 20);

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
