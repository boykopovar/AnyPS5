#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>

namespace {
struct ErrorString {
    std::uint32_t code;
    const char* text;
};

#include "ErrorStrings.inc"
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fputs(message, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

static void ExpectString(std::uint64_t code, const char* expected) {
    std::array<char, 256> buffer{};
    Require(ERR_error_string_nid_postfix(code, buffer.data()) == buffer.data(), "caller buffer returned");
    Require(std::strcmp(buffer.data(), expected) == 0, buffer.data());
}

static void ExpectNumeric(std::uint64_t code) {
    std::array<char, 32> hexadecimal{};
    std::snprintf(hexadecimal.data(), hexadecimal.size(), "%08llX", static_cast<unsigned long long>(code));
    const auto expected = "error:" + std::string(hexadecimal.data()) +
        ":lib(" + std::to_string((code >> 24) & 255) +
        "):func(" + std::to_string((code >> 12) & 4095) +
        "):reason(" + std::to_string(code & 4095) + ")";
    ExpectString(code, expected.c_str());
}

static void TestNumericCodes() {
    for (const auto& entry : CryptoStrings) ExpectNumeric(entry.code);
    for (std::uint64_t library = 0; library < 256; ++library) ExpectNumeric(library << 24);
    for (std::uint64_t reason = 1; reason <= 127; ++reason) ExpectNumeric(0x02000000 | reason);
    for (const auto code : {0x100000000ULL, 0x12345678e1abc789ULL, 0xffffffffffffffffULL, 0x8000000000000000ULL}) ExpectNumeric(code);
    std::uint64_t code = 0x51e073ab94d62fc8;
    for (int index = 0; index < 2000; ++index) {
        code ^= code << 13;
        code ^= code >> 7;
        code ^= code << 17;
        ExpectNumeric(code);
    }
}

static void TestStrings() {
    TestNumericCodes();
    ExpectString(0, "error:00000000:lib(0):func(0):reason(0)");
    ExpectString(0x20fff043, "error:20FFF043:lib(32):func(4095):reason(67)");
    ExpectString(0x20fff07e, "error:20FFF07E:lib(32):func(4095):reason(126)");
    ExpectString(0x03fff06c, "error:03FFF06C:lib(3):func(4095):reason(108)");
    ExpectString(0x0dfff0a8, "error:0DFFF0A8:lib(13):func(4095):reason(168)");
    ExpectString(0x08fff065, "error:08FFF065:lib(8):func(4095):reason(101)");
    ExpectString(0xe1abc789, "error:E1ABC789:lib(225):func(2748):reason(1929)");
    ExpectString(0x12345678e1abc789, "error:12345678E1ABC789:lib(225):func(2748):reason(1929)");
    ExpectString(0x02002023, "error:02002023:lib(2):func(2):reason(35)");
    auto* shared = ERR_error_string_nid_postfix(0xe1abc789, nullptr);
    Require(shared != nullptr, "static buffer exists");
    Require(ERR_error_string_nid_postfix(0, nullptr) == shared, "static buffer reused");
    Require(std::strcmp(shared, "error:00000000:lib(0):func(0):reason(0)") == 0, "static buffer overwritten");
    constexpr auto expected = "error:E1ABC789:lib(225):func(2748):reason(1929)";
    for (std::size_t size = 0; size <= std::strlen(expected) + 2; ++size) {
        std::array<char, 260> buffer;
        buffer.fill('#');
        ERR_error_string_n_nid_postfix(0xe1abc789, buffer.data() + 1, size);
        Require(buffer.front() == '#' && buffer[size + 1] == '#', "bounded formatting canaries");
        if (size == 0) continue;
        const auto count = std::strlen(buffer.data() + 1);
        Require(count < size, "bounded formatting terminates");
        if (size > 4 && size <= std::strlen(expected)) {
            std::size_t colons = 0;
            for (std::size_t index = 1; index <= count; ++index) colons += buffer[index] == ':';
            Require(colons == 4, "truncation keeps four colons");
        }
        if (size > std::strlen(expected)) Require(std::strcmp(buffer.data() + 1, expected) == 0, "full bounded string");
    }
    char tiny[6]{};
    ERR_error_string_n_nid_postfix(0xe1abc789, tiny, sizeof(tiny));
    Require(std::strcmp(tiny, "e::::") == 0, "historical truncation placement");
    ERR_error_string_n_nid_postfix(0, nullptr, 0);
}

static void TestQueue() {
    ERR_clear_error_nid_postfix();
    Require(ERR_get_error_nid_postfix() == 0 && ERR_peek_error_nid_postfix() == 0 &&
            ERR_peek_last_error_nid_postfix() == 0, "initially empty");
    auto* guestError = __error_nid_postfix();
    errno = EDOM;
    const int savedGuestError = *guestError;
    LibreSslPutError_nid_no_patch(32, 0xfff, 67, "producer", 19);
    Require(errno == EDOM, "publication preserves host errno");
    Require(*guestError == savedGuestError, "publication preserves guest errno");
    LibreSslPutError_nid_no_patch(3, 0xfff, 108, nullptr, 0);
    Require(ERR_peek_error_nid_postfix() == 0x20fff043 && ERR_peek_error_nid_postfix() == 0x20fff043, "peek oldest without removal");
    Require(ERR_peek_last_error_nid_postfix() == 0x03fff06c && ERR_peek_last_error_nid_postfix() == 0x03fff06c, "cross-library peek newest");
    Require(ERR_get_error_nid_postfix() == 0x20fff043 && ERR_get_error_nid_postfix() == 0x03fff06c, "FIFO removal");
    Require(ERR_get_error_nid_postfix() == 0 && ERR_peek_last_error_nid_postfix() == 0, "drained queue");
    LibreSslPutError_nid_no_patch(-1, -1, -1, nullptr, 0);
    Require(ERR_get_error_nid_postfix() == 0xffffffff, "pack masks negative fields");
    LibreSslPutError_nid_no_patch(0x132, 0x2345, 0x3456, nullptr, 0);
    Require(ERR_get_error_nid_postfix() == 0x32345456, "pack masks oversized fields");
    LibreSslPutError_nid_no_patch(0, 0, 0, nullptr, 0);
    LibreSslPutError_nid_no_patch(0, 0, 1, nullptr, 0);
    Require(ERR_get_error_nid_postfix() == 0 && ERR_get_error_nid_postfix() == 1, "zero code occupies a slot");
    for (int index = 1; index <= 100; ++index) LibreSslPutError_nid_no_patch(128, index, index, nullptr, index);
    for (std::uint64_t index = 86; index <= 100; ++index) {
        Require(ERR_get_error_nid_postfix() == (0x80000000 | (index << 12) | index), "retain newest fifteen after overflow");
    }
    Require(ERR_get_error_nid_postfix() == 0, "overflow queue drained");
    LibreSslPutError_nid_no_patch(3, 1, 2, nullptr, 0);
    ERR_clear_error_nid_postfix();
    Require(ERR_peek_error_nid_postfix() == 0 && ERR_peek_last_error_nid_postfix() == 0, "clear both queue ends");
    LibreSslPutError_nid_no_patch(32, 0xfff, 67, nullptr, 0);
    ERR_remove_thread_state_nid_postfix(nullptr);
    Require(ERR_get_error_nid_postfix() == 0, "remove current state");
    LibreSslPutError_nid_no_patch(32, 0xfff, 67, nullptr, 0);
    Require(ERR_get_error_nid_postfix() == 0x20fff043, "reuse after remove");
}

static void TestThreads() {
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(128, 7, 19, nullptr, 0);
    std::atomic<int> ready{0};
    std::array<std::thread, 8> threads;
    for (std::size_t index = 0; index < threads.size(); ++index) {
        threads[index] = std::thread([&, index] {
            Require(ERR_get_error_nid_postfix() == 0, "new thread starts empty");
            LibreSslPutError_nid_no_patch(129, 0, static_cast<int>(index + 1), nullptr, 0);
            ++ready;
            while (ready.load() != static_cast<int>(threads.size())) std::this_thread::yield();
            ERR_free_strings_nid_postfix();
            ERR_load_crypto_strings_nid_postfix();
            ExpectString(0x0dfff0a8, "error:0DFFF0A8:lib(13):func(4095):reason(168)");
            Require(ERR_peek_last_error_nid_postfix() == 0x81000001 + index, "thread-local newest");
            if (index % 2) ERR_clear_error_nid_postfix();
            else ERR_remove_thread_state_nid_postfix(nullptr);
            Require(ERR_get_error_nid_postfix() == 0, "thread-local cleanup");
            LibreSslPutError_nid_no_patch(130, 1, 3, nullptr, 0);
        });
    }
    for (auto& thread : threads) thread.join();
    Require(ERR_get_error_nid_postfix() == 0x80007013, "child queues leave parent unchanged");
    std::thread replacement([] { Require(ERR_get_error_nid_postfix() == 0, "no stale state after thread exit"); });
    replacement.join();
}

int main(int argc, char** argv) {
    if (argc == 2) {
        if (std::strcmp(argv[1], "free") == 0) TestStrings();
        LibreSslPutError_nid_no_patch(32, 0xfff, 67, nullptr, 0);
        ERR_free_strings_nid_postfix();
        TestStrings();
        Require(ERR_peek_error_nid_postfix() == 0x20fff043, "free strings preserves queued error");
        ERR_load_crypto_strings_nid_postfix();
        TestStrings();
        ERR_free_strings_nid_postfix();
        ERR_load_crypto_strings_nid_postfix();
        TestStrings();
        Require(ERR_get_error_nid_postfix() == 0x20fff043, "free strings preserves queue");
        return 0;
    }
    TestStrings();
    ERR_load_crypto_strings_nid_postfix();
    TestStrings();
    TestQueue();
    TestThreads();
}
