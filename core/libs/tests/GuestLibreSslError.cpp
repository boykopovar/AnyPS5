#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

static std::uint64_t Pack(std::uint64_t library, std::uint64_t function, std::uint64_t reason) {
    return library << 24 | function << 12 | reason;
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

template<typename TAction> static void RequireThrowsInvalidArgument(TAction action, const char* what) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, what);
}

struct GuardedPagePair {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPagePair() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr, "allocate guarded pages");
        DWORD oldProtect = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "query page size");
        pageSize = static_cast<std::size_t>(size);
        void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "allocate guarded pages");
        Require(mprotect(static_cast<unsigned char*>(mapping) + pageSize, pageSize, PROT_NONE) == 0, "protect guard page");
        base = static_cast<unsigned char*>(mapping);
#endif
    }

    ~GuardedPagePair() {
        if (base == nullptr) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 2);
#endif
    }

    GuardedPagePair(const GuardedPagePair&) = delete;
    GuardedPagePair& operator=(const GuardedPagePair&) = delete;
};

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

    void* key = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    void* certificate = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2345));
    void* authorities = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3456));
    LibreSslPutError_nid_no_patch(3, 1, 2);
    Require(PKCS12_parse_nid_postfix(nullptr, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(1)), &key, &certificate, &authorities) == 0, "null PKCS12 returns failure");
    Require(key == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234)), "null PKCS12 leaves key output unchanged");
    Require(certificate == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2345)), "null PKCS12 leaves certificate output unchanged");
    Require(authorities == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3456)), "null PKCS12 leaves authorities output unchanged");
    Require(ERR_get_error_nid_postfix() == Pack(3, 1, 2), "null PKCS12 preserves earlier error ordering");
    Require(ERR_get_error_nid_postfix() == Pack(35, 0xfff, 105), "null PKCS12 publishes LibreSSL invalid pointer error");
    Require(ERR_get_error_nid_postfix() == 0, "null PKCS12 error queue drained");

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
    static_assert(std::is_void_v<decltype(PKCS12_free_nid_postfix(nullptr))>);
    PKCS12_PBE_add_nid_postfix();
    OBJ_cleanup_nid_postfix();
    SSL_load_error_strings_nid_postfix();
    SSL_load_error_strings_nid_postfix();
    OBJ_cleanup_nid_postfix();
    Require(ERR_peek_error_nid_postfix() == 0, "PKCS12_PBE_add, OBJ_cleanup and SSL_load_error_strings queue no error");

    LibreSslPutError_nid_no_patch(11, 12, 13);
    PKCS12_free_nid_postfix(nullptr);
    Require(ERR_peek_error_nid_postfix() == Pack(11, 12, 13), "PKCS12_free null preserves the error queue");
    Require(ERR_get_error_nid_postfix() == Pack(11, 12, 13), "PKCS12_free null does not consume the error");
    static_assert(std::is_void_v<decltype(OPENSSL_add_all_algorithms_noconf_nid_postfix())>);
    static_assert(std::is_void_v<decltype(OPENSSL_load_builtin_modules_nid_postfix())>);
    static_assert(std::is_void_v<decltype(CONF_modules_free_nid_postfix())>);
    OPENSSL_add_all_algorithms_noconf_nid_postfix();
    OPENSSL_add_all_algorithms_noconf_nid_postfix();
    OPENSSL_load_builtin_modules_nid_postfix();
    OPENSSL_load_builtin_modules_nid_postfix();
    CONF_modules_free_nid_postfix();
    CONF_modules_free_nid_postfix();
    Require(ERR_peek_error_nid_postfix() == 0, "the algorithm and configuration module setup and cleanup calls queue no error");

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

    static_assert(std::is_same_v<decltype(ENGINE_get_id_nid_postfix(nullptr)), const char*>);
    static_assert(std::is_same_v<decltype(ENGINE_set_default_nid_postfix(nullptr, 0u)), int>);

    struct SyntheticEngine {
        const char* id;
        const char* name;
    };
    const char* const expectedId = "synthetic-engine";
    const SyntheticEngine namedEngine{expectedId, "Synthetic Engine"};
    const SyntheticEngine unnamedEngine{nullptr, "Unnamed Engine"};
    errno = 42;
    Require(ENGINE_get_id_nid_postfix(&namedEngine) == expectedId, "ENGINE_get_id returns offset-0 id pointer");
    Require(errno == 42, "ENGINE_get_id preserves errno on valid id");
    errno = 43;
    Require(ENGINE_get_id_nid_postfix(&unnamedEngine) == nullptr, "ENGINE_get_id returns null when id is null");
    Require(errno == 43, "ENGINE_get_id preserves errno on null id");

    GuardedPagePair guarded;
    unsigned char* const lastReadableSlot = guarded.base + guarded.pageSize - sizeof(const char*);
    std::memcpy(lastReadableSlot, &expectedId, sizeof(expectedId));
    errno = 44;
    Require(ENGINE_get_id_nid_postfix(lastReadableSlot) == expectedId, "ENGINE_get_id reads exact page-end slot");
    Require(errno == 44, "ENGINE_get_id preserves errno at page boundary");

    const void* const invalidEngines[] = {
        nullptr,
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10)),
        guarded.base + guarded.pageSize,
        guarded.base + guarded.pageSize - 4,
    };
    for (const void* invalidEngine : invalidEngines) {
        errno = 45;
        RequireThrowsInvalidArgument([&] { ENGINE_get_id_nid_postfix(invalidEngine); }, "ENGINE_get_id rejects null or unreadable engine");
        Require(errno == 45, "ENGINE_get_id preserves errno on rejected engine");
    }
    Require(ERR_peek_error_nid_postfix() == 0, "ENGINE_get_id pushes no error");

    constexpr unsigned int engineMethodDefaultMask = 0x06FFu;
    void* const guardPage = guarded.base + guarded.pageSize;
    for (unsigned int flags : {0u, 0x0100u, 0x0800u, 0xFFFFF900u}) {
        Require(ENGINE_set_default_nid_postfix(nullptr, flags) == 1, "ENGINE_set_default ignores unset mask bits on null engine");
        Require(ENGINE_set_default_nid_postfix(&notAnEngine, flags) == 1, "ENGINE_set_default ignores unset mask bits on non-null engine");
        Require(ENGINE_set_default_nid_postfix(guardPage, flags) == 1, "ENGINE_set_default ignores unset mask bits without reading engine");
    }
    for (unsigned int bit = 0; bit < 32; ++bit) {
        const unsigned int flag = 1u << bit;
        if ((flag & engineMethodDefaultMask) == 0u) {
            Require(ENGINE_set_default_nid_postfix(nullptr, flag) == 1, "ENGINE_set_default ignores non-2.5.4 flag bit on null engine");
            Require(ENGINE_set_default_nid_postfix(&notAnEngine, flag) == 1, "ENGINE_set_default ignores non-2.5.4 flag bit on non-null engine");
        } else {
            RequireThrowsInvalidArgument([&] { ENGINE_set_default_nid_postfix(nullptr, flag); }, "ENGINE_set_default throws invalid_argument for active bit on null engine");
            RequireThrows([&] { ENGINE_set_default_nid_postfix(&notAnEngine, flag); }, "ENGINE_set_default throws for active bit on non-null engine");
        }
    }
    for (unsigned int flags : {0x06FFu, 0xFFFFu, 0xFFFFFFFFu}) {
        RequireThrowsInvalidArgument([&] { ENGINE_set_default_nid_postfix(nullptr, flags); }, "ENGINE_set_default throws invalid_argument for combined flags on null engine");
        RequireThrows([&] { ENGINE_set_default_nid_postfix(&notAnEngine, flags); }, "ENGINE_set_default throws for combined flags on non-null engine");
    }
    Require(ERR_peek_error_nid_postfix() == 0, "ENGINE_set_default pushes no error");

    std::puts("guest_libressl_error: ok");
    return 0;
}
