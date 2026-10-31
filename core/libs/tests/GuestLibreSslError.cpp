#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
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

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
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

    ERR_clear_error_nid_postfix();
    const std::uint64_t nullEngine = Pack(38, 0xfff, 67);
    Require(ENGINE_get_first_nid_postfix() == nullptr, "no engine is listed");
    ENGINE_load_builtin_engines_nid_postfix();
    Require(ENGINE_get_first_nid_postfix() == nullptr, "the built-in engines add none");
    ENGINE_cleanup_nid_postfix();
    Require(ENGINE_get_first_nid_postfix() == nullptr, "cleanup leaves the list empty");
    Require(ERR_peek_error_nid_postfix() == 0, "listing, loading and cleaning up engines pushes no error");
    Require(ENGINE_by_id_nid_postfix("dynamic") == nullptr, "an engine id is found");
    Require(ERR_get_error_nid_postfix() == Pack(38, 0xfff, 116), "an unknown engine id pushes ENGINE_R_NO_SUCH_ENGINE");
    Require(ENGINE_by_id_nid_postfix(nullptr) == nullptr, "a null engine id is found");
    Require(ERR_get_error_nid_postfix() == nullEngine, "a null engine id pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_get_next_nid_postfix(nullptr) == nullptr, "a null engine has a next one");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_get_next of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_init_nid_postfix(nullptr) == 0, "a null engine initializes");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_init of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_finish_nid_postfix(nullptr) == 0, "a null engine finishes");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_finish of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_free_nid_postfix(nullptr) == 0, "a null engine is freed");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_free of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_ctrl_nid_postfix(nullptr, 1, 0, nullptr, nullptr) == 0, "a null engine takes a control");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_ctrl of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_ctrl_cmd_nid_postfix(nullptr, "SO_PATH", 0, nullptr, nullptr, 1) == 0, "an optional command on a null engine succeeds");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_ctrl_cmd of null pushes ERR_R_PASSED_NULL_PARAMETER");
    int notAnEngine = 0;
    Require(ENGINE_ctrl_cmd_nid_postfix(&notAnEngine, nullptr, 0, nullptr, nullptr, 0) == 0, "a null command name succeeds");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_ctrl_cmd with a null command name pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ENGINE_load_private_key_nid_postfix(nullptr, "key", nullptr, nullptr) == nullptr, "a null engine loads a key");
    Require(ERR_get_error_nid_postfix() == nullEngine, "ENGINE_load_private_key of null pushes ERR_R_PASSED_NULL_PARAMETER");
    Require(ERR_get_error_nid_postfix() == 0, "each call pushes one error");
    RequireThrows([&] { ENGINE_get_next_nid_postfix(&notAnEngine); }, "ENGINE_get_next of a pointer that is no engine");
    RequireThrows([&] { ENGINE_init_nid_postfix(&notAnEngine); }, "ENGINE_init of a pointer that is no engine");
    RequireThrows([&] { ENGINE_finish_nid_postfix(&notAnEngine); }, "ENGINE_finish of a pointer that is no engine");
    RequireThrows([&] { ENGINE_free_nid_postfix(&notAnEngine); }, "ENGINE_free of a pointer that is no engine");
    RequireThrows([&] { ENGINE_ctrl_nid_postfix(&notAnEngine, 1, 0, nullptr, nullptr); }, "ENGINE_ctrl of a pointer that is no engine");
    RequireThrows([&] { ENGINE_ctrl_cmd_nid_postfix(&notAnEngine, "SO_PATH", 0, nullptr, nullptr, 1); }, "ENGINE_ctrl_cmd of a pointer that is no engine");
    RequireThrows([&] { ENGINE_load_private_key_nid_postfix(&notAnEngine, "key", nullptr, nullptr); }, "ENGINE_load_private_key of a pointer that is no engine");
    Require(ERR_peek_error_nid_postfix() == 0, "a refused engine pushes no error");

    std::puts("guest_libressl_error: ok");
    return 0;
}
