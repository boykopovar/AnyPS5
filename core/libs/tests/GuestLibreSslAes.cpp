#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

static std::vector<unsigned char> Bytes(const char* hex) {
    std::vector<unsigned char> bytes;
    for (; hex[0] && hex[1]; hex += 2) bytes.push_back(static_cast<unsigned char>(std::stoi(std::string(hex, 2), nullptr, 16)));
    return bytes;
}

static std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

static std::uint32_t Word(const std::vector<unsigned char>& bytes, std::size_t index) {
    return std::uint32_t(bytes[4 * index + 3]) << 24 | std::uint32_t(bytes[4 * index + 2]) << 16 | std::uint32_t(bytes[4 * index + 1]) << 8 | bytes[4 * index];
}

static AesKey Sentinel() {
    AesKey key;
    for (std::uint32_t i = 0; i < 60; ++i) key.roundKeys[i] = 0xa5a50000u | i;
    key.rounds = 0x5a5a;
    return key;
}

template<typename TSetup> static void RequireSchedule(TSetup setup, const char* keyHex, const char* scheduleHex, const char* what) {
    const auto userKey = Bytes(keyHex);
    const auto schedule = Bytes(scheduleHex);
    const std::size_t used = schedule.size() / 4;
    const AesKey before = Sentinel();
    AesKey key = before;
    Require(setup(userKey.data(), static_cast<int>(userKey.size() * 8), &key) == 0 && key.rounds == static_cast<int>(used / 4 - 1), what);
    for (std::size_t i = 0; i < 60; ++i) Require(key.roundKeys[i] == (i < used ? Word(schedule, i) : before.roundKeys[i]), what);
}

template<typename TSetup> static void RequireRefusalsKeepKey(TSetup setup, const char* what) {
    const unsigned char userKey[32] {};
    const AesKey before = Sentinel();
    for (const int bits : {0, 64, 127, 129, 160, 224, 255, 384, 512, -128}) {
        AesKey key = before;
        Require(setup(userKey, bits, &key) == -2 && !std::memcmp(&key, &before, sizeof(key)), what);
    }
    for (const int bits : {128, 64}) {
        AesKey key = before;
        Require(setup(nullptr, bits, &key) == -1 && !std::memcmp(&key, &before, sizeof(key)), what);
    }
    Require(setup(userKey, 128, nullptr) == -1, what);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info {};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr, "guarded page allocation");
        DWORD previous {};
        Require(VirtualProtect(base + size, size, PAGE_NOACCESS, &previous) != 0, "guard page protection");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "guarded page allocation");
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + size, size, PROT_NONE) == 0, "guard page protection");
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size * 2);
#endif
    }

    unsigned char* End() const { return base + size; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous {};
        Require(VirtualProtect(base, size, PAGE_READONLY, &previous) != 0, "read-only protection");
#else
        Require(mprotect(base, size, PROT_READ) == 0, "read-only protection");
#endif
    }

private:
    unsigned char* base = nullptr;
    std::size_t size = 0;
};

template<typename TSetup> static void RequireGuestKeyPointers(TSetup setup, const char* what) {
    const auto userKey = Bytes("2b7e151628aed2a6abf7158809cf4f3c");
    const auto* unmapped = reinterpret_cast<const unsigned char*>(std::uintptr_t {0x10});
    const AesKey before = Sentinel();
    AesKey key = before;
    errno = 71;
    RequireThrows([&] { setup(unmapped, 128, &key); }, what);
    Require(errno == 71 && !std::memcmp(&key, &before, sizeof(key)), what);
    RequireThrows([&] { setup(userKey.data(), 128, reinterpret_cast<AesKey*>(std::uintptr_t {0x10})); }, what);
    Require(errno == 71, what);
    Require(setup(unmapped, 64, &key) == -2 && errno == 71, what);

    GuardedPage keyPage;
    auto* edge = keyPage.End() - userKey.size();
    std::memcpy(edge, userKey.data(), userKey.size());
    AesKey expected = before;
    Require(setup(userKey.data(), 128, &expected) == 0, what);
    key = before;
    Require(setup(edge, 128, &key) == 0 && !std::memcmp(&key, &expected, sizeof(key)) && errno == 71, what);
    RequireThrows([&] { setup(keyPage.End() - 8, 128, &key); }, what);
    Require(errno == 71, what);

    GuardedPage schedulePage;
    auto* readOnly = reinterpret_cast<AesKey*>(schedulePage.End() - sizeof(AesKey));
    std::memcpy(static_cast<void*>(readOnly), &before, sizeof(before));
    schedulePage.ReadOnly();
    RequireThrows([&] { setup(userKey.data(), 128, readOnly); }, what);
    Require(errno == 71 && !std::memcmp(readOnly, &before, sizeof(before)), what);
}

static void RequireGuestModePointers() {
    const auto key = Bytes("2b7e151628aed2a6abf7158809cf4f3c");
    AesKey encrypt {}, decrypt {};
    AES_set_encrypt_key_nid_postfix(key.data(), 128, &encrypt);
    AES_set_decrypt_key_nid_postfix(key.data(), 128, &decrypt);
    auto* unmapped = reinterpret_cast<unsigned char*>(std::uintptr_t {0x10});
    const auto* unmappedKey = reinterpret_cast<const AesKey*>(std::uintptr_t {0x10});
    std::vector<unsigned char> data(64, 0x42), out(64), iv(16), keystream(16);
    int cfbPosition = 0;
    unsigned ctrPosition = 0;

    errno = 71;
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(data.data(), out.data(), 16, unmappedKey, iv.data(), 1); }, "CBC unmapped key");
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(unmapped, out.data(), 16, &encrypt, iv.data(), 1); }, "CBC unmapped input");
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(data.data(), unmapped, 16, &encrypt, iv.data(), 1); }, "CBC unmapped output");
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(data.data(), out.data(), 16, &encrypt, unmapped, 0); }, "CBC unmapped iv");
    RequireThrows([&] { AES_cfb128_encrypt_nid_postfix(unmapped, out.data(), 16, &encrypt, iv.data(), &cfbPosition, 1); }, "CFB128 unmapped input");
    RequireThrows([&] { AES_cfb128_encrypt_nid_postfix(data.data(), out.data(), 16, &encrypt, iv.data(), reinterpret_cast<int*>(unmapped), 1); }, "CFB128 unmapped position");
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(data.data(), unmapped, 16, &encrypt, iv.data(), keystream.data(), &ctrPosition); }, "CTR unmapped output");
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(data.data(), out.data(), 16, &encrypt, iv.data(), unmapped, &ctrPosition); }, "CTR unmapped keystream");
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(data.data(), out.data(), 16, &encrypt, iv.data(), keystream.data(), reinterpret_cast<unsigned*>(unmapped)); }, "CTR unmapped position");
    Require(errno == 71, "mode errors keep errno");

    std::vector<unsigned char> large(5000);
    for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<unsigned char>(i * 7 + 3);
    std::vector<unsigned char> whole(large.size() + 16), pieces(large.size() + 16);
    std::vector<unsigned char> wholeIv(16, 0x24), piecesIv(16, 0x24);
    AES_cbc_encrypt_nid_postfix(large.data(), whole.data(), large.size(), &encrypt, wholeIv.data(), 1);
    for (std::size_t done = 0; done < large.size(); done += 16)
        AES_cbc_encrypt_nid_postfix(large.data() + done, pieces.data() + done, std::min<std::size_t>(16, large.size() - done), &encrypt, piecesIv.data(), 1);
    Require(whole == pieces && wholeIv == piecesIv, "CBC encrypt across copy chunks matches block-by-block calls");
    std::vector<unsigned char> backIv(16, 0x24);
    AES_cbc_encrypt_nid_postfix(whole.data(), whole.data(), large.size(), &decrypt, backIv.data(), 0);
    Require(std::equal(large.begin(), large.end(), whole.begin()) && backIv == wholeIv, "CBC decrypt in place across copy chunks");
    std::vector<unsigned char> ctrWhole(large.size()), ctrPieces(large.size());
    std::vector<unsigned char> wholeCounter(16, 0xfe), piecesCounter(16, 0xfe), wholeStream(16), piecesStream(16);
    unsigned wholePosition = 0, piecesPosition = 0;
    AES_ctr128_encrypt_nid_postfix(large.data(), ctrWhole.data(), large.size(), &encrypt, wholeCounter.data(), wholeStream.data(), &wholePosition);
    for (std::size_t done = 0; done < large.size(); done += 7)
        AES_ctr128_encrypt_nid_postfix(large.data() + done, ctrPieces.data() + done, std::min<std::size_t>(7, large.size() - done), &encrypt, piecesCounter.data(), piecesStream.data(), &piecesPosition);
    Require(ctrWhole == ctrPieces && wholeCounter == piecesCounter && wholeStream == piecesStream && wholePosition == piecesPosition, "CTR across copy chunks matches small calls");

    GuardedPage inputPage;
    auto* edge = inputPage.End() - 20;
    std::memcpy(edge, large.data(), 20);
    std::vector<unsigned char> edgeOut(32), edgeIv(16), expected(32), expectedIv(16);
    AES_cbc_encrypt_nid_postfix(large.data(), expected.data(), 20, &encrypt, expectedIv.data(), 1);
    errno = 71;
    AES_cbc_encrypt_nid_postfix(edge, edgeOut.data(), 20, &encrypt, edgeIv.data(), 1);
    Require(edgeOut == expected && edgeIv == expectedIv && errno == 71, "CBC encrypt reads only the input length before a guard page");
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(edge, edgeOut.data(), 20, &decrypt, edgeIv.data(), 0); }, "CBC decrypt reads the whole last block");
    auto* streamEdge = inputPage.End() - 3;
    std::memcpy(streamEdge, large.data(), 3);
    std::vector<unsigned char> cfbIv(16), cfbOut(3);
    cfbPosition = 0;
    AES_cfb128_encrypt_nid_postfix(streamEdge, cfbOut.data(), 3, &encrypt, cfbIv.data(), &cfbPosition, 1);
    Require(cfbPosition == 3 && errno == 71, "CFB128 reads only the input length before a guard page");

    GuardedPage statePage;
    auto* readOnlyIv = statePage.End() - 16;
    auto* readOnlyStream = readOnlyIv - 16;
    auto* readOnlyPosition = reinterpret_cast<unsigned*>(readOnlyStream - 16);
    auto* readOnlyCfbPosition = reinterpret_cast<int*>(readOnlyStream - 32);
    std::memset(readOnlyIv, 0x11, 16);
    std::memcpy(readOnlyStream, wholeStream.data(), 16);
    *readOnlyPosition = 3;
    *readOnlyCfbPosition = 5;
    statePage.ReadOnly();
    std::vector<unsigned char> writableIv(16, 0x11), writableStream(wholeStream);
    unsigned writablePosition = 3;
    std::vector<unsigned char> expectedStream(4), streamOut(4);
    AES_ctr128_encrypt_nid_postfix(large.data(), expectedStream.data(), 4, &encrypt, writableIv.data(), writableStream.data(), &writablePosition);
    unsigned position = 3;
    AES_ctr128_encrypt_nid_postfix(large.data(), streamOut.data(), 4, &encrypt, readOnlyIv, readOnlyStream, &position);
    Require(streamOut == expectedStream && position == 7 && errno == 71, "CTR inside a block leaves a read-only counter and keystream unwritten");
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(large.data(), streamOut.data(), 4, &encrypt, writableIv.data(), writableStream.data(), readOnlyPosition); }, "CTR read-only position");
    position = 0;
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(large.data(), streamOut.data(), 4, &encrypt, readOnlyIv, writableStream.data(), &position); }, "CTR read-only counter");
    AES_cfb128_encrypt_nid_postfix(large.data(), streamOut.data(), 0, &encrypt, readOnlyIv, readOnlyCfbPosition, 1);
    AES_ctr128_encrypt_nid_postfix(large.data(), streamOut.data(), 0, &encrypt, readOnlyIv, readOnlyStream, readOnlyPosition);
    AES_cbc_encrypt_nid_postfix(large.data(), out.data(), 0, &encrypt, readOnlyIv, 1);
    Require(*readOnlyCfbPosition == 5 && *readOnlyPosition == 3 && errno == 71, "zero-length modes write nothing");
    cfbPosition = 0;
    RequireThrows([&] { AES_cfb128_encrypt_nid_postfix(large.data(), streamOut.data(), 4, &encrypt, readOnlyIv, &cfbPosition, 1); }, "CFB128 read-only iv");
    RequireThrows([&] { AES_cbc_encrypt_nid_postfix(large.data(), out.data(), 16, &encrypt, readOnlyIv, 1); }, "CBC read-only iv");
    Require(errno == 71, "read-only mode errors keep errno");
}

int main() {
    const auto plain = Bytes("00112233445566778899aabbccddeeff");
    const char* keys[3] {
        "000102030405060708090a0b0c0d0e0f",
        "000102030405060708090a0b0c0d0e0f1011121314151617",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
    };
    const char* ciphers[3] {"69c4e0d86a7b0430d8cdb78070b4c55a", "dda97ca4864cdfe06eaf70a0ec0d7191", "8ea2b7ca516745bfeafc49904b496089"};
    for (int i = 0; i < 3; ++i) {
        const auto key = Bytes(keys[i]);
        const int bits = static_cast<int>(key.size() * 8);
        AesKey encrypt {}, decrypt {};
        unsigned char out[16] {}, back[16] {};
        Require(AES_set_encrypt_key_nid_postfix(key.data(), bits, &encrypt) == 0 && encrypt.rounds == 10 + 2 * i, "encrypt key");
        Require(AES_set_decrypt_key_nid_postfix(key.data(), bits, &decrypt) == 0 && decrypt.rounds == encrypt.rounds, "decrypt key");
        const int last = 4 * encrypt.rounds;
        Require(!std::memcmp(decrypt.roundKeys, encrypt.roundKeys + last, 16) && !std::memcmp(decrypt.roundKeys + last, encrypt.roundKeys, 16), "decrypt key reverses the round keys");
        AES_encrypt_nid_postfix(plain.data(), out, &encrypt);

        Require(Hex(out, 16) == ciphers[i], "FIPS-197 appendix C cipher");
        AES_decrypt_nid_postfix(out, back, &decrypt);

        Require(!std::memcmp(back, plain.data(), 16), "FIPS-197 appendix C inverse cipher");
        AES_ecb_encrypt_nid_postfix(plain.data(), out, &encrypt, 1);

        Require(Hex(out, 16) == ciphers[i], "ECB encrypt");
        AES_ecb_encrypt_nid_postfix(out, out, &decrypt, 0);

        Require(!std::memcmp(out, plain.data(), 16), "ECB decrypt in place");
    }

    AesKey fipsKey {};
    Require(AES_set_encrypt_key_nid_postfix(Bytes("2b7e151628aed2a6abf7158809cf4f3c").data(), 128, &fipsKey) == 0, "FIPS-197 appendix A key");
    Require(fipsKey.roundKeys[4] == 0x17fefaa0 && fipsKey.roundKeys[43] == 0xa60c63b6, "FIPS-197 appendix A.1 expansion");
    AesKey bad {};
    bad.rounds = 7;
    bad.roundKeys[0] = 1;
    const unsigned char zero[32] {};
    Require(AES_set_encrypt_key_nid_postfix(zero, 64, &bad) == -2 && bad.rounds == 7 && bad.roundKeys[0] == 1, "unsupported key length keeps the key");
    Require(AES_set_decrypt_key_nid_postfix(zero, 129, &bad) == -2 && bad.rounds == 7 && bad.roundKeys[0] == 1, "unsupported decrypt key length keeps the key");
    Require(AES_set_encrypt_key_nid_postfix(nullptr, 128, &bad) == -1 && AES_set_encrypt_key_nid_postfix(zero, 128, nullptr) == -1, "null key arguments");
    unsigned char block[16] {};
    RequireThrows([&] { AES_encrypt_nid_postfix(zero, block, &bad); }, "key schedule with an invalid round count");
    RequireRefusalsKeepKey(&AES_set_encrypt_key_nid_postfix, "refused encrypt key setup leaves the key unchanged");
    RequireRefusalsKeepKey(&AES_set_decrypt_key_nid_postfix, "refused decrypt key setup leaves the key unchanged");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "2b7e151628aed2a6abf7158809cf4f3c",
        "2b7e151628aed2a6abf7158809cf4f3ca0fafe1788542cb123a339392a6c7605f2c295f27a96b9435935807a7359f67f3d80477d4716fe3e1e237e446d7a883b"
        "ef44a541a8525b7fb671253bdb0bad00d4d1c6f87c839d87caf2b8bc11f915bc6d88a37a110b3efddbf98641ca0093fd4e54f70e5f5fc9f384a64fb24ea6dc4f"
        "ead27321b58dbad2312bf5607f8d292fac7766f319fadc2128d12941575c006ed014f9a8c9ee2589e13f0cc8b6630ca6",
        "FIPS-197 appendix A.1 AES-128 encrypt schedule");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7bfe0c91f72402f5a5ec12068e6c827f6b0e7a95b95c56fec24db7b4bd69b5411885a74796e92538fd"
        "e75fad44bb095386485af05721efb14fa448f6d94d6dce24aa326360113b30e6a25e7ed583b1cf9a27f939436a94f767c0a69407d19da4e1ec1786eb6fa64971"
        "485f703222cb8755e26d135233f0b7b340beeb282f18a2596747d26b458c553ea7e1466c9411f1df821f750aad07d753ca4005388fcc5006282d166abc3ce7b5"
        "e98ba06f448c773c8ecc720401002202",
        "FIPS-197 appendix A.2 AES-192 encrypt schedule");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff49ba354118e6925afa51a8b5f2067fcdea8b09c1a93d194cdbe49846eb75d5b9a"
        "d59aecb85bf3c917fee94248de8ebe96b5a9328a2678a647983122292f6c79b3812c81addadf48ba24360af2fab8b46498c5bfc9bebd198e268c3ba709e04214"
        "68007bacb2df331696e939e46c518d80c814e20476a9fb8a5025c02d59c58239de1369676ccc5a71fa2563959674ee155886ca5d2e2f31d77e0af1fa27cf73c3"
        "749c47ab18501ddae2757e4f7401905acafaaae3e4d59b349adf6acebd10190dfe4890d1e6188d0b046df344706c631e",
        "FIPS-197 appendix A.3 AES-256 encrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "2b7e151628aed2a6abf7158809cf4f3c",
        "d014f9a8c9ee2589e13f0cc8b6630ca60c7b5a631319eafeb0398890664cfbb4df7d925a1f62b09da320626ed675732412c07647c01f22c7bc42d2f37555114a"
        "6efcd876d2df54807c5df034c917c3b96ea30afcbc238cf6ae82a4b4b54a338d90884413d280860a12a128421bc897397c1f13f74208c219c021ae480969bf7b"
        "cc7505eb3e17d1ee82296c51c94811332b3708a7f262d405bc3ebdbf4b617d622b7e151628aed2a6abf7158809cf4f3c",
        "AES-128 decrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "e98ba06f448c773c8ecc720401002202ac491644e55710b746c08a75c89b2cada3979ac28e5ba6d8e12cc9e654b272baf3b42258b59ebb5cf8fb64fe491e06f3"
        "4d65dfa2b1e5620dea899c312dcc3c1a5b6cfe3cc745a02bf8b9a572462a9904c5ddb7f8be933c760b4f46a6fc80bdafb5dc7ad0f7cffb09a7ec43939c295e17"
        "5023b89a3bc51d84d04b19377b4e8b8e41b34544ab0592b9ce92f15e421381d9659763e78c817087123039436be6a51e9eb149c479d69c5dfeb4a27ceab6d7fd"
        "8e73b0f7da0e6452c810f32b809079e5",
        "AES-192 decrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        "fe4890d1e6188d0b046df344706c631eada23f4963e23b2455427c8a5c70910457c96cf6074f07c0706abb07137f9241b668b621ce40046d36a047ae0932ed8e"
        "34ad1e4450866b367725bcc76315294632526c367828b24cf8e043c33f92aa20c440b289642b757227a3d7f114309581d669a7334a7ade7a80c8f18fc772e9e3"
        "25ba3c22a06bc7fb4388a2833393427054fb808b9c137949cab22ff547ba186c6c3d632985d1fbd9e3e36578701be0f34a7459f9c8e8f9c256a156bc8d083799"
        "42107758e9ec98f066329ea193f8858b8ec6bff6829ca03b9e49af7edba96125603deb1015ca71be2b73aef0857d7781",
        "AES-256 decrypt schedule");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f",
        "000102030405060708090a0b0c0d0e0fd6aa74fdd2af72fadaa678f1d6ab76feb692cf0b643dbdf1be9bc5006830b3feb6ff744ed2c2c9bf6c590cbf0469bf41"
        "47f7f7bc95353e03f96c32bcfd058dfd3caaa3e8a99f9deb50f3af57adf622aa5e390f7df7a69296a7553dc10aa31f6b14f9701ae35fe28c440adf4d4ea9c026"
        "47438735a41c65b9e016baf4aebf7ad2549932d1f08557681093ed9cbe2c974e13111d7fe3944a17f307a78b4d2b30c5",
        "PS5 firmware 11.60 AES-128 encrypt schedule of the key 00 01 02 ...");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f1011121314151617",
        "000102030405060708090a0b0c0d0e0f10111213141516175846f2f95c43f4fe544afef55847f0fa4856e2e95c43f4fe40f949b31cbabd4d48f043b810b7b342"
        "58e151ab04a2a5557effb5416245080c2ab54bb43a02f8f662e3a95d66410c08f501857297448d7ebdf1c6ca87f33e3ce510976183519b6934157c9ea351f1e0"
        "1ea0372a995309167c439e77ff12051edd7e0e887e2fff68608fc842f9dcc154859f5f237a8d5a3dc0c02952beefd63ade601e7827bcdf2ca223800fd8aeda32"
        "a4970a331a78dc09c418c271e3a41d5d",
        "PS5 firmware 11.60 AES-192 encrypt schedule of the key 00 01 02 ...");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1fa573c29fa176c498a97fce93a572c09c1651a8cd0244beda1a5da4c10640bade"
        "ae87dff00ff11b68a68ed5fb03fc15676de1f1486fa54f9275f8eb5373b8518dc656827fc9a799176f294cec6cd5598b3de23a75524775e727bf9eb45407cf39"
        "0bdc905fc27b0948ad5245a4c1871c2f45f5a66017b2d387300d4d33640a820a7ccff71cbeb4fe5413e6bbf0d261a7dff01afafee7a82979d7a5644ab3afe640"
        "2541fe719bf500258813bbd55a721c0a4e5a6699a9f24fe07e572baacdf8cdea24fc79ccbf0979e9371ac23c6d68de36",
        "PS5 firmware 11.60 AES-256 encrypt schedule of the key 00 01 02 ...");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f",
        "13111d7fe3944a17f307a78b4d2b30c513aa29be9c8faff6f770f58000f7bf031362a4638f2586486bff5a76f7874a838d82fc749c47222be4dadc3e9c7810f5"
        "72e3098d11c5de5f789dfe1578a2cccb2ec410276326d7d26958204a003f32dea8a2f5044de2c7f50a7ef79869671294c7c6e391e54032f1479c306d6319e50c"
        "a0db02992286d160a2dc029c2485d5618c56dff0825dd3f9805ad3fc8659d7fd000102030405060708090a0b0c0d0e0f",
        "PS5 firmware 11.60 AES-128 decrypt schedule of the key 00 01 02 ...");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f1011121314151617",
        "a4970a331a78dc09c418c271e3a41d5dd6bebd0dc209ea494db073803e021bb98fb999c973b26839c7f9d89d85c68c72f77d6ec1423f54ef5378317f14b75744"
        "1147659047cf663b9b0ece8dfc0bf1f0dcc1a8b667053f7dcc5c194ab5423a2ec6deb0ab791e2364a4055fbe568803abdd1b7cdaf28d5c158a49ab1dbbc497cb"
        "78c4f708318d3cd69655b701bfc093cf60dcef10299524ce62dbef152f9620cf4b4ecbdb4d4dcfda5752d7c74949cbde1a1f181d1e1b1c194742c7d74949cbde"
        "000102030405060708090a0b0c0d0e0f",
        "PS5 firmware 11.60 AES-192 decrypt schedule of the key 00 01 02 ...");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "24fc79ccbf0979e9371ac23c6d68de3634f1d1ffbfceaa2ffce9e25f2558016e5e1648eb384c350a7571b746dc80e684c8a305808b3f7bd043274870d9b1e331"
        "b5708e13665a7de14d3d824ca9f151c274da7ba3439c7e50c81833a09a96ab413ca69715d32af3f22b67ffade4ccd38ef85fc4f3374605f38b844df0528e98e1"
        "de69409aef8c64e7f84d0c5fcfab2c23aed55816cf19c100bcc24803d90ad51115c668bd31e5247d17c168b837e6207c7fd7850f61cc991673db890365c89d12"
        "2a2840c924234cc026244cc5202748c41a1f181d1e1b1c191217101516131411000102030405060708090a0b0c0d0e0f",
        "PS5 firmware 11.60 AES-256 decrypt schedule of the key 00 01 02 ...");

    const auto key = Bytes("2b7e151628aed2a6abf7158809cf4f3c");
    const auto message = Bytes("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    AesKey encrypt {}, decrypt {};
    AES_set_encrypt_key_nid_postfix(key.data(), 128, &encrypt);
    AES_set_decrypt_key_nid_postfix(key.data(), 128, &decrypt);

    std::vector<unsigned char> out(message.size() + 16), back(message.size() + 16);

    auto iv = Bytes("000102030405060708090a0b0c0d0e0f");
    AES_cbc_encrypt_nid_postfix(message.data(), out.data(), 64, &encrypt, iv.data(), 1);

    Require(Hex(out.data(), 64) == "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7", "SP 800-38A F.2.1 CBC encrypt");
    Require(Hex(iv.data(), 16) == "3ff1caa1681fac09120eca307586e1a7", "CBC encrypt returns the last cipher block as iv");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::memcpy(back.data(), out.data(), 64);
    AES_cbc_encrypt_nid_postfix(back.data(), back.data(), 32, &decrypt, iv.data(), 0);
    AES_cbc_encrypt_nid_postfix(back.data() + 32, back.data() + 32, 32, &decrypt, iv.data(), 0);

    Require(!std::memcmp(back.data(), message.data(), 64) && Hex(iv.data(), 16) == "3ff1caa1681fac09120eca307586e1a7", "SP 800-38A F.2.2 CBC decrypt in place across calls");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::vector<unsigned char> partial(32, 0xee);
    AES_cbc_encrypt_nid_postfix(message.data(), partial.data(), 20, &encrypt, iv.data(), 1);

    Require(Hex(partial.data(), 16) == "7649abac8119b246cee98e9b12e9197d" && partial[31] != 0xee && Hex(iv.data(), 16) == Hex(partial.data() + 16, 16), "CBC encrypt pads a partial block with the iv and writes the whole block");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::vector<unsigned char> plainPartial(32, 0xee);
    AES_cbc_encrypt_nid_postfix(partial.data(), plainPartial.data(), 20, &decrypt, iv.data(), 0);

    Require(!std::memcmp(plainPartial.data(), message.data(), 20) && plainPartial[20] == 0xee && Hex(iv.data(), 16) == Hex(partial.data() + 16, 16), "CBC decrypt writes only the requested length");

    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    int num = 0;
    AES_cfb128_encrypt_nid_postfix(message.data(), out.data(), 5, &encrypt, iv.data(), &num, 1);
    AES_cfb128_encrypt_nid_postfix(message.data() + 5, out.data() + 5, 32, &encrypt, iv.data(), &num, 1);

    Require(num == 5, "CFB128 keeps the block position");
    AES_cfb128_encrypt_nid_postfix(message.data() + 37, out.data() + 37, 27, &encrypt, iv.data(), &num, 1);

    Require(Hex(out.data(), 64) == "3b3fd92eb72dad20333449f8e83cfb4ac8a64537a0b3a93fcde3cdad9f1ce58b26751f67a3cbb140b1808cf187a4f4dfc04b05357c5d1c0eeac4c66f9ff7f2e6" && num == 0, "SP 800-38A F.3.13 CFB128 encrypt in pieces");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    num = 0;
    AES_cfb128_encrypt_nid_postfix(out.data(), back.data(), 64, &encrypt, iv.data(), &num, 0);

    Require(!std::memcmp(back.data(), message.data(), 64), "SP 800-38A F.3.14 CFB128 decrypt");
    num = 16;
    RequireThrows([&] { AES_cfb128_encrypt_nid_postfix(message.data(), out.data(), 1, &encrypt, iv.data(), &num, 1); }, "CFB128 position out of range");

    auto counter = Bytes("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    unsigned char keystream[16] {};
    unsigned position = 0;
    AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 7, &encrypt, counter.data(), keystream, &position);
    AES_ctr128_encrypt_nid_postfix(message.data() + 7, out.data() + 7, 57, &encrypt, counter.data(), keystream, &position);

    Require(Hex(out.data(), 64) == "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee", "SP 800-38A F.5.1 CTR encrypt in pieces");
    Require(position == 0 && Hex(counter.data(), 16) == "f0f1f2f3f4f5f6f7f8f9fafbfcfdff03", "CTR advances the counter per block");
    counter = Bytes("ffffffffffffffffffffffffffffffff");
    position = 0;
    AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 37, &encrypt, counter.data(), keystream, &position);

    Require(Hex(out.data(), 37) == "e13338e36cb71962e00d020b4cedbd86d3dae15b04bb352fa0f59febfcb4da3e67da610697", "CTR counter wraps across all 128 bits");
    Require(position == 5 && Hex(counter.data(), 16) == "00000000000000000000000000000002", "CTR position and counter after a partial block");
    position = 16;
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 1, &encrypt, counter.data(), keystream, &position); }, "CTR position out of range");

    RequireGuestKeyPointers(&AES_set_encrypt_key_nid_postfix, "encrypt key setup");
    RequireGuestKeyPointers(&AES_set_decrypt_key_nid_postfix, "decrypt key setup");
    RequireGuestModePointers();

    std::puts("libSceLibreSsl AES tests passed");
}
