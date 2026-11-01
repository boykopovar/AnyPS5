#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static std::uint64_t Pack(std::uint64_t library, std::uint64_t function, std::uint64_t reason) {
    return library << 24 | function << 12 | reason;
}

int main() {
    Require(ERR_get_error_nid_postfix() == 0, "empty queue get");
    Require(ERR_peek_error_nid_postfix() == 0, "empty queue peek");

    LibreSslPutError_nid_no_patch(3, 100, 65);
    LibreSslPutError_nid_no_patch(4, 101, 66);
    LibreSslPutError_nid_no_patch(20, 102, 67);
    Require(ERR_peek_error_nid_postfix() == Pack(3, 100, 65), "peek returns the earliest error");
    Require(ERR_peek_error_nid_postfix() == Pack(3, 100, 65), "peek keeps the earliest error");
    Require(ERR_get_error_nid_postfix() == Pack(3, 100, 65), "get returns the earliest error");
    Require(ERR_get_error_nid_postfix() == Pack(4, 101, 66), "get returns the second error");
    Require(ERR_peek_error_nid_postfix() == Pack(20, 102, 67), "peek follows get");
    Require(ERR_get_error_nid_postfix() == Pack(20, 102, 67), "get returns the third error");
    Require(ERR_get_error_nid_postfix() == 0, "drained queue get");
    Require(ERR_peek_error_nid_postfix() == 0, "drained queue peek");

    for (int i = 1; i <= 16; ++i) LibreSslPutError_nid_no_patch(i, 0, i);
    for (int i = 2; i <= 16; ++i) Require(ERR_get_error_nid_postfix() == Pack(i, 0, i), "full queue drops the oldest error");
    Require(ERR_get_error_nid_postfix() == 0, "full queue holds 15 errors");

    LibreSslPutError_nid_no_patch(0x1ff, 0x1fff, 0x1fff);
    Require(ERR_get_error_nid_postfix() == 0xFFFFFFFFu, "fields are masked and the code keeps 32 bits");

    LibreSslPutError_nid_no_patch(5, 1, 2);
    LibreSslPutError_nid_no_patch(6, 1, 2);
    ERR_clear_error_nid_postfix();
    Require(ERR_peek_error_nid_postfix() == 0, "clear empties the queue");
    LibreSslPutError_nid_no_patch(7, 1, 2);
    Require(ERR_get_error_nid_postfix() == Pack(7, 1, 2), "queue works after clear");

    LibreSslPutError_nid_no_patch(8, 1, 2);
    bool otherEmpty = false;
    bool otherOwn = false;
    std::thread other([&] {
        otherEmpty = ERR_peek_error_nid_postfix() == 0;
        LibreSslPutError_nid_no_patch(9, 1, 2);
        otherOwn = ERR_get_error_nid_postfix() == Pack(9, 1, 2);
    });
    other.join();
    Require(otherEmpty, "another thread starts with an empty queue");
    Require(otherOwn, "another thread reads its own error");
    Require(ERR_get_error_nid_postfix() == Pack(8, 1, 2), "this thread keeps its error");
    Require(ERR_get_error_nid_postfix() == 0, "the other thread's error stays on that thread");

    LibreSslPutError_nid_no_patch(10, 1, 2);
    ERR_remove_thread_state_nid_postfix(nullptr);
    Require(ERR_peek_error_nid_postfix() == 0, "removing the thread state empties the queue");

    static_assert(std::is_void_v<decltype(PKCS12_PBE_add_nid_postfix())>);
    static_assert(std::is_void_v<decltype(OBJ_cleanup_nid_postfix())>);
    static_assert(std::is_void_v<decltype(SSL_load_error_strings_nid_postfix())>);
    PKCS12_PBE_add_nid_postfix();
    OBJ_cleanup_nid_postfix();
    SSL_load_error_strings_nid_postfix();
    SSL_load_error_strings_nid_postfix();
    OBJ_cleanup_nid_postfix();
    Require(ERR_peek_error_nid_postfix() == 0, "PKCS12_PBE_add, OBJ_cleanup and SSL_load_error_strings queue no error");

    std::puts("guest_libressl_error: ok");
    return 0;
}
