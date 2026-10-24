#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
void APS5_VABI ERR_load_crypto_strings_nid_postfix();
std::uint64_t APS5_VABI ERR_peek_last_error_nid_postfix();
int* APS5_VABI __error_nid_postfix();
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

static void TestStrings() {
    ERR_load_crypto_strings_nid_postfix();
    ExpectString(0, "error:00000000:lib(0):func(0):reason(0)");
    ExpectString(0x20fff043, "error:20FFF043:BIO routines:CRYPTO_internal:passed a null parameter");
    ExpectString(0x20fff07e, "error:20FFF07E:BIO routines:CRYPTO_internal:write to read only BIO");
    ExpectString(0x03fff06c, "error:03FFF06C:bignum routines:CRYPTO_internal:no inverse");
    ExpectString(0xe1abc789, "error:E1ABC789:lib(225):func(2748):reason(1929)");
    ExpectString(0x12345678e1abc789, "error:12345678E1ABC789:lib(225):func(2748):reason(1929)");
    ExpectString(0x02002023, "error:02002023:system library:connect:Resource temporarily unavailabl");
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
        ExpectString(0x20fff043, "error:20FFF043:lib(32):func(4095):reason(67)");
        Require(ERR_peek_error_nid_postfix() == 0x20fff043, "free strings preserves queued error");
        ERR_load_crypto_strings_nid_postfix();
        ExpectString(0x20fff043, "error:20FFF043:lib(32):func(4095):reason(67)");
        ERR_free_strings_nid_postfix();
        ERR_load_crypto_strings_nid_postfix();
        ExpectString(0x20fff043, "error:20FFF043:lib(32):func(4095):reason(67)");
        Require(ERR_get_error_nid_postfix() == 0x20fff043, "free strings preserves queue");
        return 0;
    }
    TestStrings();
    TestQueue();
    TestThreads();
}
