#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL alert/engine check failed at line %u\n", location.line());
    std::abort();
}

class UnreadablePage {
public:
    UnreadablePage() {
#ifdef _WIN32
        SYSTEM_INFO info {};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(_page != nullptr);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        void* mapped = mmap(nullptr, _size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        _page = mapped;
#endif
    }

    ~UnreadablePage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    UnreadablePage(const UnreadablePage&) = delete;
    UnreadablePage& operator=(const UnreadablePage&) = delete;

    const void* Address() const { return _page; }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void TestEngineGetId() {
    struct FakeEngine {
        const char* id;
    };
    const char* expectedId = "mock-engine";
    FakeEngine engine {expectedId};
    errno = 71;
    Require(ENGINE_get_id_nid_postfix(&engine) == expectedId && errno == 71);

    errno = 71;
    bool threwInvalidArgument = false;
    try {
        static_cast<void>(ENGINE_get_id_nid_postfix(nullptr));
    } catch (const std::invalid_argument&) {
        threwInvalidArgument = true;
    }
    Require(threwInvalidArgument && errno == 71);

    UnreadablePage unreadable;
    errno = 71;
    bool threwNotImplemented = false;
    try {
        static_cast<void>(ENGINE_get_id_nid_postfix(unreadable.Address()));
    } catch (const std::invalid_argument&) {
        Require(false);
    } catch (const std::runtime_error&) {
        threwNotImplemented = true;
    }
    Require(threwNotImplemented && errno == 71);
}

}

void Check(int value, const char* expected) {
    const char* actual = SSL_alert_desc_string_long_nid_postfix(value);
    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::fprintf(stderr, "LibreSSL alert %d: expected %s, got %s\n", value, expected, actual ? actual : "(null)");
        std::abort();
    }
}

int main() {
    std::array<const char*, 256> expected;
    expected.fill("unknown");
    expected[0] = "close notify";
    expected[10] = "unexpected_message";
    expected[20] = "bad record mac";
    expected[30] = "decompression failure";
    expected[40] = "handshake failure";
    expected[41] = "no certificate";
    expected[42] = "bad certificate";
    expected[43] = "unsupported certificate";
    expected[44] = "certificate revoked";
    expected[45] = "certificate expired";
    expected[46] = "certificate unknown";
    expected[47] = "illegal parameter";
    expected[21] = "decryption failed";
    expected[22] = "record overflow";
    expected[48] = "unknown CA";
    expected[49] = "access denied";
    expected[50] = "decode error";
    expected[51] = "decrypt error";
    expected[60] = "export restriction";
    expected[70] = "protocol version";
    expected[71] = "insufficient security";
    expected[80] = "internal error";
    expected[90] = "user canceled";
    expected[100] = "no renegotiation";
    expected[110] = "unsupported extension";
    expected[111] = "certificate unobtainable";
    expected[112] = "unrecognized name";
    expected[113] = "bad certificate status response";
    expected[114] = "bad certificate hash value";
    expected[115] = "unknown PSK identity";
    for (int code = 0; code < 256; ++code) {
        Check(code, expected[code]);
        Check(0x100 | code, expected[code]);
        Check(0x200 | code, expected[code]);
        Check(0x123400 | code, expected[code]);
        Check(code - 256, expected[code]);
    }
    Check(std::numeric_limits<int>::min(), "close notify");
    Check(std::numeric_limits<int>::max(), "unknown");
    const char* retained = SSL_alert_desc_string_long_nid_postfix(40);
    Check(42, "bad certificate");
    if (std::strcmp(retained, "handshake failure") != 0) std::abort();
    TestEngineGetId();
    return 0;
}
