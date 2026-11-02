#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

struct TestSslInternal {
    unsigned char beforeOptions[8];
    std::uint64_t options;
    std::uint64_t mode;
    unsigned char beforeHandshakeFunc[32];
    void* handshakeFunc;
    void* msgCallback;
    void* msgCallbackArg;
    unsigned char beforeVerifyCallback[8];
    void* verifyCallback;
    unsigned char beforeShutdown[72];
    int shutdown;
    unsigned char beforeReadAhead[40];
    int readAhead;
    unsigned char beforeAeadReadCtx[24];
    void* aeadReadCtx;
    void* aeadWriteCtx;
    void* encWriteCtx;
    void* writeHash;
    unsigned char beforeMaxCertList[24];
    std::int64_t maxCertList;
    unsigned char beforeState[108];
    int state;
    int rstate;
    unsigned char rest[12];
};

struct TestSsl3Internal {
    unsigned char beforeSendConnectionBinding[1012];
    int sendConnectionBinding;
    unsigned char beforeAlpnSelected[8];
    const unsigned char* alpnSelected;
    unsigned int alpnSelectedLength;
    unsigned char rest[4];
};

struct TestSsl3 {
    unsigned char beforeInternal[136];
    TestSsl3Internal* internal;
};

struct TestDtls1Internal {
    unsigned char beforeMtu[636];
    std::uint32_t mtu;
    unsigned char rest[4];
};

struct TestDtls1 {
    unsigned char beforeInternal[32];
    TestDtls1Internal* internal;
};

struct TestMethodInternal {
    int version;
    unsigned char beforeAccept[28];
    void* accept;
    void* connect;
    unsigned char rest[112];
};

struct TestMethod {
    unsigned char beforeInternal[40];
    TestMethodInternal* internal;
};

struct TestSsl {
    int version;
    unsigned char beforeMethod[4];
    TestMethod* method;
    unsigned char beforeServer[24];
    int server;
    unsigned char beforeS3[4];
    TestSsl3* s3;
    TestDtls1* d1;
    unsigned char beforeVerifyMode[72];
    int verifyMode;
    unsigned char beforeClientVersion[32];
    int clientVersion;
    unsigned int maxSendFragment;
    unsigned char beforeTlsextHostname[4];
    char* tlsextHostname;
    unsigned char beforeEncReadCtx[16];
    void* encReadCtx;
    void* readHash;
    TestSslInternal* internal;
};

static_assert(sizeof(TestSslInternal) == 432 && offsetof(TestSslInternal, options) == 8 && offsetof(TestSslInternal, mode) == 16);
static_assert(offsetof(TestSslInternal, handshakeFunc) == 56 && offsetof(TestSslInternal, msgCallback) == 64 && offsetof(TestSslInternal, msgCallbackArg) == 72);
static_assert(offsetof(TestSslInternal, verifyCallback) == 88 && offsetof(TestSslInternal, shutdown) == 168 && offsetof(TestSslInternal, readAhead) == 212);
static_assert(offsetof(TestSslInternal, aeadReadCtx) == 240 && offsetof(TestSslInternal, aeadWriteCtx) == 248 && offsetof(TestSslInternal, encWriteCtx) == 256);
static_assert(offsetof(TestSslInternal, writeHash) == 264 && offsetof(TestSslInternal, maxCertList) == 296);
static_assert(offsetof(TestSslInternal, state) == 412 && offsetof(TestSslInternal, rstate) == 416);
static_assert(sizeof(TestSsl3Internal) == 1040 && offsetof(TestSsl3Internal, sendConnectionBinding) == 1012);
static_assert(offsetof(TestSsl3Internal, alpnSelected) == 1024 && offsetof(TestSsl3Internal, alpnSelectedLength) == 1032);
static_assert(sizeof(TestSsl3) == 144 && sizeof(TestDtls1) == 40 && offsetof(TestDtls1Internal, mtu) == 636);
static_assert(sizeof(TestMethodInternal) == 160 && offsetof(TestMethodInternal, accept) == 32 && offsetof(TestMethodInternal, connect) == 40);
static_assert(sizeof(TestMethod) == 48 && sizeof(TestSsl) == 232 && offsetof(TestSsl, method) == 8 && offsetof(TestSsl, server) == 40);
static_assert(offsetof(TestSsl, s3) == 48 && offsetof(TestSsl, d1) == 56 && offsetof(TestSsl, verifyMode) == 136);
static_assert(offsetof(TestSsl, clientVersion) == 172 && offsetof(TestSsl, maxSendFragment) == 176 && offsetof(TestSsl, tlsextHostname) == 184);
static_assert(offsetof(TestSsl, encReadCtx) == 208 && offsetof(TestSsl, readHash) == 216 && offsetof(TestSsl, internal) == 224);

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL settings check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

template<typename T> static void Fill(T& value, unsigned char seed) {
    auto* bytes = reinterpret_cast<unsigned char*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) bytes[index] = static_cast<unsigned char>(seed + index);
}

template<typename T> static bool Same(const T& left, const T& right) {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
}

struct Connection {
    TestSsl ssl;
    TestSslInternal internal;

    Connection() {
        Fill(ssl, 0x10);
        Fill(internal, 0x40);
        ssl.method = nullptr;
        ssl.s3 = nullptr;
        ssl.d1 = nullptr;
        ssl.encReadCtx = nullptr;
        ssl.readHash = nullptr;
        ssl.internal = &internal;
        internal.aeadReadCtx = nullptr;
        internal.aeadWriteCtx = nullptr;
        internal.encWriteCtx = nullptr;
        internal.writeHash = nullptr;
    }

    LibreSsl* Ssl() {
        return reinterpret_cast<LibreSsl*>(&ssl);
    }
};

static std::int64_t Control(Connection& connection, int command, std::int64_t argument, void* pointer = nullptr) {
    return SSL_ctrl_nid_postfix(connection.Ssl(), command, argument, pointer);
}

static void TestSetVerify() {
    int first = 0;
    int second = 0;
    Connection connection;
    Connection expected = connection;
    RequireThrows([&] { SSL_set_verify_nid_postfix(nullptr, 1, &first); });
    SSL_set_verify_nid_postfix(connection.Ssl(), 3, &first);
    expected.ssl.verifyMode = 3;
    expected.internal.verifyCallback = &first;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    SSL_set_verify_nid_postfix(connection.Ssl(), -1, nullptr);
    expected.ssl.verifyMode = -1;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    SSL_set_verify_nid_postfix(connection.Ssl(), 1, &second);
    expected.ssl.verifyMode = 1;
    expected.internal.verifyCallback = &second;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    connection.ssl.internal = nullptr;
    SSL_set_verify_nid_postfix(connection.Ssl(), 2, nullptr);
    Require(connection.ssl.verifyMode == 2);
    RequireThrows([&] { SSL_set_verify_nid_postfix(connection.Ssl(), 5, &first); });
    Require(connection.ssl.verifyMode == 2);
}

static void TestGetAlpnSelected() {
    static const unsigned char protocol[] = "h2";
    TestSsl3Internal stateInternal {};
    TestSsl3 state {};
    Connection connection;
    const Connection expected = connection;
    const unsigned char* data = reinterpret_cast<const unsigned char*>(1);
    unsigned int length = 77;
    RequireThrows([&] { SSL_get0_alpn_selected_nid_postfix(nullptr, &data, &length); });
    RequireThrows([&] { SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), nullptr, &length); });
    RequireThrows([&] { SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), &data, nullptr); });
    SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), &data, &length);
    Require(data == nullptr && length == 0);
    connection.ssl.s3 = &state;
    RequireThrows([&] { SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), &data, &length); });
    state.internal = &stateInternal;
    Fill(stateInternal, 0x70);
    stateInternal.alpnSelected = protocol;
    stateInternal.alpnSelectedLength = 2;
    data = nullptr;
    SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), &data, &length);
    Require(data == protocol && length == 2);
    stateInternal.alpnSelected = nullptr;
    stateInternal.alpnSelectedLength = 0;
    SSL_get0_alpn_selected_nid_postfix(connection.Ssl(), &data, &length);
    Require(data == nullptr && length == 0);
    connection.ssl.s3 = nullptr;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
}

static void TestSetConnectState() {
    int accept = 0;
    int connect = 0;
    TestMethodInternal methodInternal {};
    TestMethod method {};
    Fill(methodInternal, 0x30);
    methodInternal.accept = &accept;
    methodInternal.connect = &connect;
    method.internal = &methodInternal;
    Connection connection;
    connection.ssl.method = &method;
    connection.ssl.server = 1;
    connection.internal.shutdown = 3;
    connection.internal.state = 0x1234;
    Connection expected = connection;
    RequireThrows([] { SSL_set_connect_state_nid_postfix(nullptr); });
    SSL_set_connect_state_nid_postfix(connection.Ssl());
    expected.ssl.server = 0;
    expected.internal.shutdown = 0;
    expected.internal.state = 0x5000;
    expected.internal.handshakeFunc = &connect;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    methodInternal.connect = nullptr;
    connection.internal.handshakeFunc = &accept;
    SSL_set_connect_state_nid_postfix(connection.Ssl());
    Require(connection.internal.handshakeFunc == nullptr);
    connection.ssl.server = 1;
    method.internal = nullptr;
    RequireThrows([&] { SSL_set_connect_state_nid_postfix(connection.Ssl()); });
    connection.ssl.method = nullptr;
    RequireThrows([&] { SSL_set_connect_state_nid_postfix(connection.Ssl()); });
    connection.ssl.method = &method;
    method.internal = &methodInternal;
    connection.ssl.internal = nullptr;
    RequireThrows([&] { SSL_set_connect_state_nid_postfix(connection.Ssl()); });
    Require(connection.ssl.server == 1);
}

static void TestControlReadAhead() {
    Connection connection;
    Connection expected = connection;
    RequireThrows([] { SSL_ctrl_nid_postfix(nullptr, 40, 0, nullptr); });
    connection.internal.readAhead = -1;
    expected.internal.readAhead = -1;
    Require(Control(connection, 40, 5) == -1);
    Require(Control(connection, 41, INT64_C(0x123456789ABCDEF0)) == -1);
    expected.internal.readAhead = static_cast<int>(0x9ABCDEF0u);
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    Require(Control(connection, 40, 0) == INT64_C(-1698898192));
    Require(Control(connection, 41, 1) == INT64_C(-1698898192));
    Require(Control(connection, 40, 0) == 1);
    connection.ssl.internal = nullptr;
    RequireThrows([&] { Control(connection, 40, 0); });
    RequireThrows([&] { Control(connection, 41, 0); });
}

static void TestControlMessageCallbackArgument() {
    int argument = 0;
    Connection connection;
    Connection expected = connection;
    Require(Control(connection, 16, 9, &argument) == 1);
    expected.internal.msgCallbackArg = &argument;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    Require(Control(connection, 16, 0, nullptr) == 1);
    Require(connection.internal.msgCallbackArg == nullptr);
    connection.ssl.internal = nullptr;
    RequireThrows([&] { Control(connection, 16, 0, &argument); });
}

static void TestControlOptionsAndMode() {
    Connection connection;
    connection.internal.options = 0x4;
    connection.internal.mode = 0;
    Connection expected = connection;
    Require(Control(connection, 32, 0x10) == 0x14);
    Require(Control(connection, 32, INT64_C(0x100000000)) == INT64_C(0x100000014));
    Require(Control(connection, 77, 0x4) == INT64_C(0x100000010));
    Require(Control(connection, 32, 0) == INT64_C(0x100000010));
    Require(Control(connection, 32, -1) == -1);
    Require(Control(connection, 77, -1) == 0);
    Require(Control(connection, 33, 0x2) == 0x2);
    Require(Control(connection, 33, INT64_C(0x800000000)) == INT64_C(0x800000002));
    Require(Control(connection, 78, 0x2) == INT64_C(0x800000000));
    Require(Control(connection, 78, -1) == 0);
    Require(Control(connection, 33, 0x5) == 0x5);
    expected.internal.options = 0;
    expected.internal.mode = 0x5;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    connection.ssl.internal = nullptr;
    RequireThrows([&] { Control(connection, 32, 1); });
    RequireThrows([&] { Control(connection, 77, 1); });
    RequireThrows([&] { Control(connection, 33, 1); });
    RequireThrows([&] { Control(connection, 78, 1); });
}

static void TestControlMaxCertList() {
    Connection connection;
    connection.internal.maxCertList = 102400;
    Connection expected = connection;
    Require(Control(connection, 50, 7) == 102400);
    Require(Control(connection, 51, INT64_C(0x123456789)) == 102400);
    Require(Control(connection, 50, 0) == INT64_C(0x123456789));
    Require(Control(connection, 51, -5) == INT64_C(0x123456789));
    expected.internal.maxCertList = -5;
    Require(Same(connection.ssl, expected.ssl) && Same(connection.internal, expected.internal));
    connection.ssl.internal = nullptr;
    RequireThrows([&] { Control(connection, 50, 0); });
    RequireThrows([&] { Control(connection, 51, 0); });
}

static void TestControlMtu() {
    TestMethodInternal methodInternal {};
    TestMethod method {};
    TestDtls1Internal dtlsInternal {};
    TestDtls1 dtls {};
    Fill(dtlsInternal, 0x50);
    method.internal = &methodInternal;
    methodInternal.version = 0x0303;
    Connection connection;
    connection.ssl.internal = nullptr;
    Connection expected = connection;
    Require(Control(connection, 17, 227) == 0);
    Require(Control(connection, 17, -1) == 0);
    Require(Control(connection, 17, INT64_MIN) == 0);
    Require(SSL_ctrl_nid_postfix(nullptr, 17, 227, nullptr) == 0);
    Require(SSL_ctrl_nid_postfix(nullptr, 17, -1, nullptr) == 0);
    RequireThrows([] { SSL_ctrl_nid_postfix(nullptr, 17, 228, nullptr); });
    RequireThrows([&] { Control(connection, 17, 228); });
    connection.ssl.method = &method;
    expected.ssl.method = &method;
    Require(Control(connection, 17, 228) == 0);
    Require(Control(connection, 17, 1500) == 0);
    Require(Same(connection.ssl, expected.ssl));
    methodInternal.version = 0xFEFF;
    RequireThrows([&] { Control(connection, 17, 1500); });
    connection.ssl.d1 = &dtls;
    RequireThrows([&] { Control(connection, 17, 1500); });
    dtls.internal = &dtlsInternal;
    TestDtls1Internal dtlsExpected = dtlsInternal;
    Require(Control(connection, 17, 227) == 0);
    Require(Same(dtlsInternal, dtlsExpected));
    Require(Control(connection, 17, 228) == 228);
    dtlsExpected.mtu = 228;
    Require(Same(dtlsInternal, dtlsExpected));
    Require(Control(connection, 17, INT64_C(0x100000500)) == INT64_C(0x100000500));
    dtlsExpected.mtu = 0x500;
    Require(Same(dtlsInternal, dtlsExpected));
    method.internal = nullptr;
    RequireThrows([&] { Control(connection, 17, 1500); });
}

static void TestControlMaxSendFragment() {
    Connection connection;
    connection.ssl.internal = nullptr;
    connection.ssl.maxSendFragment = 16384;
    Connection expected = connection;
    for (std::int64_t argument : {INT64_C(511), INT64_C(16385), INT64_C(0), INT64_C(-1), INT64_C(0x100000200), INT64_C(1) << 60}) {
        Require(Control(connection, 52, argument) == 0);
        Require(Same(connection.ssl, expected.ssl));
    }
    Require(SSL_ctrl_nid_postfix(nullptr, 52, 511, nullptr) == 0);
    Require(SSL_ctrl_nid_postfix(nullptr, 52, 16385, nullptr) == 0);
    RequireThrows([] { SSL_ctrl_nid_postfix(nullptr, 52, 512, nullptr); });
    Require(Control(connection, 52, 512) == 1);
    expected.ssl.maxSendFragment = 512;
    Require(Same(connection.ssl, expected.ssl));
    Require(Control(connection, 52, 16384) == 1);
    expected.ssl.maxSendFragment = 16384;
    Require(Same(connection.ssl, expected.ssl));
    Require(Control(connection, 52, 4096) == 1);
    expected.ssl.maxSendFragment = 4096;
    Require(Same(connection.ssl, expected.ssl));
}

static void TestControlRenegotiationInfoSupport() {
    TestSsl3Internal stateInternal {};
    TestSsl3 state {};
    Connection connection;
    connection.ssl.internal = nullptr;
    Connection expected = connection;
    RequireThrows([] { SSL_ctrl_nid_postfix(nullptr, 76, 0, nullptr); });
    Require(Control(connection, 76, 1) == 0);
    connection.ssl.s3 = &state;
    RequireThrows([&] { Control(connection, 76, 0); });
    state.internal = &stateInternal;
    Fill(stateInternal, 0x60);
    stateInternal.sendConnectionBinding = 1;
    Require(Control(connection, 76, 0) == 1);
    stateInternal.sendConnectionBinding = -1;
    Require(Control(connection, 76, 0) == -1);
    stateInternal.sendConnectionBinding = 0;
    Require(Control(connection, 76, 0) == 0);
    connection.ssl.s3 = nullptr;
    Require(Same(connection.ssl, expected.ssl));
}

static void TestControlNullConnection() {
    for (int command : {16, 32, 33, 50, 51, 77, 78}) RequireThrows([&] { SSL_ctrl_nid_postfix(nullptr, command, 1, nullptr); });
}

int main() {
    TestSetVerify();
    TestGetAlpnSelected();
    TestSetConnectState();
    TestControlReadAhead();
    TestControlMessageCallbackArgument();
    TestControlOptionsAndMode();
    TestControlMaxCertList();
    TestControlMtu();
    TestControlMaxSendFragment();
    TestControlRenegotiationInfoSupport();
    TestControlNullConnection();
    return 0;
}
