#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <barrier>
#include <cstdint>
#include <deque>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static void Exercise(unsigned seed) {
    std::deque<std::uint64_t> expected;
    ERR_clear_error_nid_postfix();
    for (unsigned step = 0; step < 12000; ++step) {
        seed = seed * 1664525u + 1013904223u;
        const unsigned operation = (seed >> 24) % 16;
        if (operation < 11) {
            const int library = static_cast<int>(seed & 0x1ff);
            const int function = static_cast<int>((seed >> 9) & 0x1fff);
            const int reason = static_cast<int>((seed >> 18) & 0x1fff);
            LibreSslPutError_nid_no_patch(library, function, reason);
            expected.push_back((std::uint64_t(library & 0xff) << 24) | (std::uint64_t(function & 0xfff) << 12) | (reason & 0xfff));
            if (expected.size() > 15) expected.pop_front();
        } else if (operation < 14) {
            Require(ERR_get_error_nid_postfix() == (expected.empty() ? 0 : expected.front()));
            if (!expected.empty()) expected.pop_front();
        } else if (operation == 15) {
            ERR_clear_error_nid_postfix();
            expected.clear();
        }
        for (int repeat = 0; repeat < 2; ++repeat)
            Require(ERR_peek_error_nid_postfix() == (expected.empty() ? 0 : expected.front()));
    }
    while (!expected.empty()) {
        Require(ERR_get_error_nid_postfix() == expected.front());
        expected.pop_front();
    }
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestPkcs12FileErrors() {
    constexpr const char* path = "guest_libre_ssl_pkcs12_fp.tmp";
    auto* stream = fopen_nid_postfix(path, "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix("", 1, 0, stream) == 0);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(path, "rb");
    Require(stream != nullptr);
    void* const sentinel = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    void* output = sentinel;
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(3, 1, 41);
    Require(d2i_PKCS12_fp_nid_postfix(stream, &output) == nullptr);
    Require(output == sentinel && feof_nid_postfix(stream) != 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix(path, "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix("x", 1, 1, stream) == 1);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(path, "rb");
    Require(stream != nullptr);
    bool threw = false;
    try {
        d2i_PKCS12_fp_nid_postfix(stream, &output);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && output == sentinel && stream->IsOpen());
    Require(fclose_nid_postfix(stream) == 0);

    threw = false;
    try {
        d2i_PKCS12_fp_nid_postfix(nullptr, &output);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && output == sentinel);
    Require(remove_nid_postfix(path) == 0);
}

int main() {
    const std::array<unsigned char, 16> sessionBytes {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    auto sessionCopy = sessionBytes;
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(3, 1, 41);
    SSL_SESSION_free_nid_postfix(nullptr);
    Require(ERR_peek_error_nid_postfix() == ((3ull << 24) | (1ull << 12) | 41));
    try {
        SSL_SESSION_free_nid_postfix(sessionCopy.data());
        Require(false);
    } catch (const std::exception&) {
    }
    Require(sessionCopy == sessionBytes);
    Require(ERR_peek_error_nid_postfix() == ((3ull << 24) | (1ull << 12) | 41));
    ERR_clear_error_nid_postfix();

    TestPkcs12FileErrors();
    std::barrier boundary(3);
    const auto worker = [&](unsigned marker) {
        ERR_clear_error_nid_postfix();
        LibreSslPutError_nid_no_patch(3, 1, static_cast<int>(marker));
        boundary.arrive_and_wait();
        Require(ERR_get_error_nid_postfix() == ((3ull << 24) | (1ull << 12) | marker));
        Exercise(marker);
        boundary.arrive_and_wait();
    };
    std::thread one(worker, 41), two(worker, 73);
    worker(99);
    one.join();
    two.join();
}
