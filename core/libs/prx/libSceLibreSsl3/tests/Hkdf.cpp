#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 HKDF check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; } catch (const std::exception&) {}
    Require(false, location);
}

int liveBlocks = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t bytes) {
    void* const block = std::malloc(bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

void APS5_VABI Release_nid_no_patch(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed_nid_no_patch(std::size_t count, std::size_t bytes) {
    void* const block = std::calloc(count ? count : 1, bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

void* APS5_VABI UnexpectedReallocate_nid_no_patch(void*, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { std::abort(); }

void RegisterHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Release_nid_no_patch), reinterpret_cast<void*>(&AllocateZeroed_nid_no_patch),
        reinterpret_cast<void*>(&UnexpectedReallocate_nid_no_patch), reinterpret_cast<void*>(&UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(&UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(&UnexpectedPosixAlign_nid_no_patch),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

constexpr std::uint64_t CryptoError(std::uint64_t reason) {
    return 15ull << 24 | 0xfffull << 12 | reason;
}

constexpr std::uint64_t CryptoLibraryError = CryptoError(15);
constexpr std::uint64_t TooLargeError = CryptoError(164);
constexpr std::uint8_t Fill = 0xa5;

void RequireQueue(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const auto code : expected) Require(ERR_get_error_nid_postfix() == code, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
    Require(liveBlocks == 0, location);
}

std::vector<std::uint8_t> Range(int first, int last) {
    std::vector<std::uint8_t> bytes;
    for (int value = first; value < last; ++value) bytes.push_back(static_cast<std::uint8_t>(value));
    return bytes;
}

std::string Hex(const std::uint8_t* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

struct Input {
    const std::uint8_t* secret;
    std::size_t secretLength;
    const std::uint8_t* salt;
    std::size_t saltLength;
    const std::uint8_t* info;
    std::size_t infoLength;
};

int Run(std::vector<std::uint8_t>& output, std::size_t length, const EvpMd* digest, const Input& input) {
    return HKDF_nid_postfix(output.data(), length, digest, input.secret, input.secretLength, input.salt, input.saltLength, input.info, input.infoLength);
}

std::string Derive(const EvpMd* digest, const Input& input, std::size_t length, std::source_location location = std::source_location::current()) {
    std::vector<std::uint8_t> output(length + 1, Fill);
    Require(Run(output, length, digest, input) == 1, location);
    Require(output[length] == Fill, location);
    RequireQueue({}, location);
    return Hex(output.data(), length);
}

const std::vector<std::uint8_t> shortSecret(22, 0x0b);
const std::vector<std::uint8_t> shortSalt = Range(0x00, 0x0d);
const std::vector<std::uint8_t> shortInfo = Range(0xf0, 0xfa);
const std::vector<std::uint8_t> longSecret = Range(0x00, 0x50);
const std::vector<std::uint8_t> longSalt = Range(0x60, 0xb0);
const std::vector<std::uint8_t> longInfo = Range(0xb0, 0x100);
const Input shortInput {shortSecret.data(), shortSecret.size(), shortSalt.data(), shortSalt.size(), shortInfo.data(), shortInfo.size()};
const Input longInput {longSecret.data(), longSecret.size(), longSalt.data(), longSalt.size(), longInfo.data(), longInfo.size()};

const std::string case1 = "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865";
const std::string case3 = "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8";

void TestRfc5869() {
    const auto* sha256 = EVP_sha256_nid_postfix();
    Require(Derive(sha256, shortInput, 42) == case1);
    Require(Derive(sha256, longInput, 82) ==
        "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87");
    const std::uint8_t empty[1] {};
    Require(Derive(sha256, {shortSecret.data(), shortSecret.size(), empty, 0, empty, 0}, 42) == case3);
}

void TestSalt() {
    const auto* sha256 = EVP_sha256_nid_postfix();
    const std::vector<std::uint8_t> zeros(32, 0);
    Require(Derive(sha256, {shortSecret.data(), shortSecret.size(), nullptr, 0, nullptr, 0}, 42) == case3);
    Require(Derive(sha256, {shortSecret.data(), shortSecret.size(), nullptr, 7, nullptr, 0}, 42) == case3);
    Require(Derive(sha256, {shortSecret.data(), shortSecret.size(), zeros.data(), zeros.size(), nullptr, 0}, 42) == case3);
    Require(Derive(sha256, {shortSecret.data(), shortSecret.size(), shortSalt.data(), (std::size_t{1} << 32) + shortSalt.size(), shortInfo.data(), shortInfo.size()}, 42) == case1);
    const auto longerSalt = Range(0, 200);
    Require(Derive(EVP_sha512_nid_postfix(), {shortSecret.data(), shortSecret.size(), longerSalt.data(), longerSalt.size(), shortInfo.data(), shortInfo.size()}, 64) ==
        "0d2fc35216a15b5bea02bfa0d87c198613ae687ef793070039c019fc2bc01f5a36b604cce4c574e27de58d17b0a1bb7b2630612c07f98d3fdc13442a813f0b9a");
    Require(Derive(sha256, {nullptr, 0, shortSalt.data(), shortSalt.size(), shortInfo.data(), shortInfo.size()}, 42) ==
        "4dd449ba1911c57d79603e7e902452f79601b5e4d7b235ce0e11a7789a17766099725013cd1c97a52d03");
}

void TestDigests() {
    Require(Derive(EVP_sha384_nid_postfix(), longInput, 100) ==
        "484ca052b8cc724fd1c4ec64d57b4e818c7e25a8e0f4569ed72a6a05fe0649eebf69f8d5c832856bf4e4fbc17967d54975324a94987f7f41835817d8994fdbd6f4c09c5500dca24a56222fea53d8967a8b2e2a125bbbd822a84eb77be82dfd76a1c0ab1e");
    Require(Derive(EVP_sha512_nid_postfix(), longInput, 130) ==
        "ce6c97192805b346e6161e821ed165673b84f400a2b514b2fe23d84cd189ddf1b695b48cbd1c8388441137b3ce28f16aa64ba33ba466b24df6cfcb021ecff235f6a2056ce3af1de44d572097a8505d9e7a9354e5796284151c2dd39c39b3cd3d8e50fcc383ebdec37476e03b721ef5efef873c281f018b8ca42e1245b2271f871ba6");
    Require(Derive(EVP_md5_nid_postfix(), longInput, 40) == "6046b84c0ee83f7e686e072a52208c708bae321b8b2cd7d952b41d1033ff98df77016d42db55afc0");
}

void TestLength() {
    const auto* sha256 = EVP_sha256_nid_postfix();
    Require(HKDF_nid_postfix(nullptr, 0, sha256, shortSecret.data(), shortSecret.size(), nullptr, 0, nullptr, 0) == 1);
    RequireQueue({});
    const std::size_t maximum = 255 * 32;
    std::vector<std::uint8_t> full(maximum + 1, Fill);
    Require(Run(full, maximum, sha256, shortInput) == 1 && full[maximum] == Fill);
    RequireQueue({});
    Require(Hex(full.data(), 32) == case1.substr(0, 64));
    Require(Hex(full.data() + maximum - 32, 32) == "76a3f78bcffe95fecf91923c22ad6ee64d48a6d1b981d7e523d5c0f22154ee88");
    std::vector<std::uint8_t> over(maximum + 2, Fill);
    Require(Run(over, maximum + 1, sha256, shortInput) == 0);
    Require(std::memcmp(over.data(), full.data(), maximum) == 0 && over[maximum] == Fill && over[maximum + 1] == Fill);
    RequireQueue({TooLargeError});
    const std::size_t md5Maximum = 255 * 16;
    std::vector<std::uint8_t> md5(md5Maximum + 1, Fill);
    Require(Run(md5, md5Maximum, EVP_md5_nid_postfix(), shortInput) == 1);
    Require(Hex(md5.data() + md5Maximum - 16, 16) == "4bd7530276fe27e565ad97dfebf8a23f");
    RequireQueue({});
    Require(Run(md5, md5Maximum + 1, EVP_md5_nid_postfix(), shortInput) == 0 && md5[md5Maximum] == Fill);
    RequireQueue({TooLargeError});
}

void TestFailures() {
    std::vector<std::uint8_t> output(43, Fill);
    Require(Run(output, 42, nullptr, shortInput) == 0);
    Require(output == std::vector<std::uint8_t>(43, Fill));
    RequireQueue({CryptoLibraryError});
    Require(HKDF_nid_postfix(nullptr, 42, nullptr, shortSecret.data(), shortSecret.size(), nullptr, 0, nullptr, 0) == 0);
    RequireQueue({CryptoLibraryError});
    Require(HKDF_nid_postfix(output.data(), 42, EVP_sha256_nid_postfix(), shortSecret.data(), shortSecret.size(), shortSalt.data(), std::size_t{1} << 31, nullptr, 0) == 0);
    Require(output == std::vector<std::uint8_t>(43, Fill));
    RequireQueue({CryptoLibraryError});
    const auto* sha256 = EVP_sha256_nid_postfix();
    RequireInvalidArgument([&] { HKDF_nid_postfix(nullptr, 42, sha256, shortSecret.data(), shortSecret.size(), nullptr, 0, nullptr, 0); });
    RequireInvalidArgument([&] { HKDF_nid_postfix(output.data(), 42, sha256, nullptr, 1, nullptr, 0, nullptr, 0); });
    RequireInvalidArgument([&] { HKDF_nid_postfix(output.data(), 42, sha256, shortSecret.data(), shortSecret.size(), nullptr, 0, nullptr, 1); });
    RequireQueue({});
}

}

int main() {
    RegisterHeap();
    ERR_clear_error_nid_postfix();
    TestRfc5869();
    TestSalt();
    TestDigests();
    TestLength();
    TestFailures();
    std::puts("LibreSSL3 HKDF tests passed");
}
