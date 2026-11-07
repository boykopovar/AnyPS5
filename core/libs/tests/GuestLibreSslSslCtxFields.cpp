#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

constexpr std::size_t CtxSize = 120;
constexpr std::size_t InternalSize = 464;

constexpr std::size_t CtxExtraCerts = 40;
constexpr std::size_t CtxVerifyMode = 48;
constexpr std::size_t CtxSidCtxLength = 52;
constexpr std::size_t CtxSidCtx = 56;
constexpr std::size_t CtxInternal = 112;

constexpr std::size_t MinVersion = 0;
constexpr std::size_t MaxVersion = 2;
constexpr std::size_t Options = 8;
constexpr std::size_t Mode = 16;
constexpr std::size_t MsgCallback = 96;
constexpr std::size_t MsgCallbackArg = 104;
constexpr std::size_t DefaultVerifyCallback = 112;
constexpr std::size_t ServernameCallback = 128;
constexpr std::size_t ServernameArg = 136;
constexpr std::size_t TicketKeyCallback = 144;
constexpr std::size_t StatusCallback = 152;
constexpr std::size_t StatusArg = 160;
constexpr std::size_t SessionCacheSize = 176;
constexpr std::size_t SessionCacheMode = 200;
constexpr std::size_t StatsConnect = 204;
constexpr std::size_t StatsConnectRenegotiate = 208;
constexpr std::size_t StatsConnectGood = 212;
constexpr std::size_t StatsAccept = 216;
constexpr std::size_t StatsAcceptRenegotiate = 220;
constexpr std::size_t StatsAcceptGood = 224;
constexpr std::size_t StatsMiss = 228;
constexpr std::size_t StatsTimeout = 232;
constexpr std::size_t StatsCacheFull = 236;
constexpr std::size_t StatsHit = 240;
constexpr std::size_t StatsCbHit = 244;
constexpr std::size_t MaxCertList = 280;
constexpr std::size_t ReadAhead = 288;
constexpr std::size_t MaxSendFragment = 296;
constexpr std::size_t TicketKeys = 312;

constexpr std::uint64_t ShouldNotHaveBeenCalled = 0x14FFF042;
constexpr std::uint64_t InvalidTicketKeysLength = 0x14FFF145;
constexpr std::uint64_t SessionIdContextTooLong = 0x14FFF111;

static_assert(sizeof(LibreSslCtx) == CtxSize && sizeof(LibreSslCtxInternal) == InternalSize);
static_assert(StatsCbHit + 4 == 248 && TicketKeys + 48 == 360 && CtxSidCtx + 32 == 88);

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL_CTX fields check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void* Pointer(std::uintptr_t value) {
    return reinterpret_cast<void*>(value);
}

template<typename TValue> static void Store(unsigned char* base, std::size_t offset, TValue value) {
    std::memcpy(base + offset, &value, sizeof value);
}

template<typename TValue> static TValue Load(const unsigned char* base, std::size_t offset) {
    TValue value;
    std::memcpy(&value, base + offset, sizeof value);
    return value;
}

struct Bytes {
    alignas(8) unsigned char ctx[CtxSize];
    alignas(8) unsigned char internal[InternalSize];
};

struct Fixture {
    Bytes actual;
    Bytes expected;

    Fixture() {
        for (std::size_t i = 0; i < CtxSize; ++i) actual.ctx[i] = static_cast<unsigned char>(i * 7 + 3);
        for (std::size_t i = 0; i < InternalSize; ++i) actual.internal[i] = static_cast<unsigned char>(i * 13 + 5);
        Store<void*>(actual.ctx, CtxExtraCerts, nullptr);
        Store<int>(actual.ctx, CtxVerifyMode, 0);
        Store<unsigned int>(actual.ctx, CtxSidCtxLength, 0);
        Store<void*>(actual.ctx, CtxInternal, actual.internal);
        Store<std::uint16_t>(actual.internal, MinVersion, 0x301);
        Store<std::uint16_t>(actual.internal, MaxVersion, 0x303);
        Store<std::uint64_t>(actual.internal, Options, 0x4);
        Store<std::uint64_t>(actual.internal, Mode, 0);
        for (std::size_t offset : {MsgCallback, MsgCallbackArg, DefaultVerifyCallback, ServernameCallback, ServernameArg, TicketKeyCallback, StatusCallback, StatusArg})
            Store<void*>(actual.internal, offset, nullptr);
        Store<std::uint64_t>(actual.internal, SessionCacheSize, 20480);
        Store<int>(actual.internal, SessionCacheMode, 2);
        for (std::size_t offset = StatsConnect; offset <= StatsCbHit; offset += 4) Store<int>(actual.internal, offset, 0);
        Store<std::int64_t>(actual.internal, MaxCertList, 102400);
        Store<int>(actual.internal, ReadAhead, 0);
        Store<unsigned int>(actual.internal, MaxSendFragment, 16384);
        expected = actual;
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    LibreSslCtx* Context() {
        return reinterpret_cast<LibreSslCtx*>(actual.ctx);
    }

    template<typename TValue> void SetCtx(std::size_t offset, TValue value) {
        Store(actual.ctx, offset, value);
        Store(expected.ctx, offset, value);
    }

    template<typename TValue> void SetInternal(std::size_t offset, TValue value) {
        Store(actual.internal, offset, value);
        Store(expected.internal, offset, value);
    }

    template<typename TValue> void ExpectCtx(std::size_t offset, TValue value) {
        Store(expected.ctx, offset, value);
    }

    template<typename TValue> void ExpectInternal(std::size_t offset, TValue value) {
        Store(expected.internal, offset, value);
    }

    void Verify(std::uint64_t error, std::source_location location) {
        Require(std::memcmp(actual.ctx, expected.ctx, CtxSize) == 0, location);
        Require(std::memcmp(actual.internal, expected.internal, InternalSize) == 0, location);
        Require(ERR_get_error_nid_postfix() == error, location);
        Require(ERR_get_error_nid_postfix() == 0, location);
    }

    void Ctrl(int cmd, std::int64_t larg, void* parg, std::int64_t result, std::uint64_t error = 0, std::source_location location = std::source_location::current()) {
        Require(SSL_CTX_ctrl_nid_postfix(Context(), cmd, larg, parg) == result, location);
        Verify(error, location);
    }

    void CallbackCtrl(int cmd, void* callback, std::int64_t result, std::uint64_t error = 0, std::source_location location = std::source_location::current()) {
        Require(SSL_CTX_callback_ctrl_nid_postfix(Context(), cmd, callback) == result, location);
        Verify(error, location);
    }

    void SessionIdContext(const unsigned char* sidCtx, unsigned int length, int result, std::uint64_t error = 0, std::source_location location = std::source_location::current()) {
        Require(SSL_CTX_set_session_id_context_nid_postfix(Context(), sidCtx, length) == result, location);
        Verify(error, location);
    }

    template<typename TAction> void Throws(TAction action, std::source_location location = std::source_location::current()) {
        RequireThrows(action, location);
        Verify(0, location);
    }
};

static void TestOptionsAndMode() {
    Fixture f;
    f.Ctrl(32, 0x4, nullptr, 0x4);
    f.ExpectInternal<std::uint64_t>(Options, 0x80000004);
    f.Ctrl(32, 0x80000000, nullptr, 0x80000004);
    f.ExpectInternal<std::uint64_t>(Options, 0x80000000);
    f.Ctrl(77, 0x4, nullptr, 0x80000000);
    f.ExpectInternal<std::uint64_t>(Mode, 0x2);
    f.Ctrl(33, 0x2, nullptr, 0x2);
    f.ExpectInternal<std::uint64_t>(Mode, 0);
    f.Ctrl(78, 0x2, nullptr, 0);
    f.ExpectInternal<std::uint64_t>(Options, UINT64_MAX);
    f.Ctrl(32, -1, nullptr, -1);
    f.ExpectInternal<std::uint64_t>(Options, 0);
    f.Ctrl(77, -1, nullptr, 0);
    f.ExpectInternal<std::uint64_t>(Mode, UINT64_MAX);
    f.Ctrl(33, -1, nullptr, -1);
    f.ExpectInternal<std::uint64_t>(Mode, 0x8000000000000000);
    f.Ctrl(78, INT64_MAX, nullptr, INT64_MIN);
}

static void TestReadAhead() {
    Fixture f;
    f.Ctrl(40, 0, nullptr, 0);
    f.ExpectInternal<int>(ReadAhead, 1);
    f.Ctrl(41, 1, nullptr, 0);
    f.Ctrl(41, 0x100000001, nullptr, 1);
    f.Ctrl(40, 0, nullptr, 1);
    f.ExpectInternal<int>(ReadAhead, 0);
    f.Ctrl(41, 0x100000000, nullptr, 1);
    f.Ctrl(40, 0, nullptr, 0);
    f.ExpectInternal<int>(ReadAhead, -1);
    f.Ctrl(41, -1, nullptr, 0);
    f.Ctrl(40, 0, nullptr, -1);
}

static void TestMaxCertList() {
    Fixture f;
    f.Ctrl(50, 0, nullptr, 102400);
    f.ExpectInternal<std::int64_t>(MaxCertList, 1234);
    f.Ctrl(51, 1234, nullptr, 102400);
    f.ExpectInternal<std::int64_t>(MaxCertList, -5);
    f.Ctrl(51, -5, nullptr, 1234);
    f.Ctrl(50, 0, nullptr, -5);
}

static void TestSessionCache() {
    Fixture f;
    f.Ctrl(43, 0, nullptr, 20480);
    f.ExpectInternal<std::uint64_t>(SessionCacheSize, 7);
    f.Ctrl(42, 7, nullptr, 20480);
    f.ExpectInternal<std::uint64_t>(SessionCacheSize, UINT64_MAX);
    f.Ctrl(42, -1, nullptr, 7);
    f.Ctrl(43, 0, nullptr, -1);
    f.Ctrl(45, 0, nullptr, 2);
    f.ExpectInternal<int>(SessionCacheMode, 3);
    f.Ctrl(44, 3, nullptr, 2);
    f.ExpectInternal<int>(SessionCacheMode, 2);
    f.Ctrl(44, 0x100000002, nullptr, 3);
    f.Ctrl(45, 0, nullptr, 2);
}

static void TestMaxSendFragment() {
    Fixture f;
    f.Ctrl(52, 511, nullptr, 0);
    f.ExpectInternal<unsigned int>(MaxSendFragment, 512);
    f.Ctrl(52, 512, nullptr, 1);
    f.ExpectInternal<unsigned int>(MaxSendFragment, 16384);
    f.Ctrl(52, 16384, nullptr, 1);
    f.Ctrl(52, 16385, nullptr, 0);
    f.Ctrl(52, -1, nullptr, 0);
    f.Ctrl(52, 0x100000200, nullptr, 0);
}

static void TestStats() {
    Fixture f;
    for (int cmd = 21; cmd <= 31; ++cmd) f.Ctrl(cmd, 0, nullptr, 0);
    const std::size_t offsets[] = {StatsConnect, StatsConnectRenegotiate, StatsConnectGood, StatsAccept, StatsAcceptRenegotiate, StatsAcceptGood,
                                   StatsMiss, StatsTimeout, StatsCacheFull, StatsHit, StatsCbHit};
    for (int i = 0; i < 11; ++i) f.SetInternal<int>(offsets[i], 101 + i);
    f.Ctrl(21, 0, nullptr, 101);
    f.Ctrl(22, 0, nullptr, 103);
    f.Ctrl(23, 0, nullptr, 102);
    f.Ctrl(24, 0, nullptr, 104);
    f.Ctrl(25, 0, nullptr, 106);
    f.Ctrl(26, 0, nullptr, 105);
    f.Ctrl(27, 0, nullptr, 110);
    f.Ctrl(28, 0, nullptr, 111);
    f.Ctrl(29, 0, nullptr, 107);
    f.Ctrl(30, 0, nullptr, 108);
    f.Ctrl(31, 0, nullptr, 109);
    f.SetInternal<int>(StatsConnect, -1);
    f.Ctrl(21, 0, nullptr, -1);
}

static void TestCallbackArgs() {
    Fixture f;
    f.ExpectInternal<void*>(MsgCallbackArg, Pointer(0x2222));
    f.Ctrl(16, 0, Pointer(0x2222), 1);
    f.ExpectInternal<void*>(ServernameArg, Pointer(0x3333));
    f.Ctrl(54, 0, Pointer(0x3333), 1);
    f.ExpectInternal<void*>(StatusArg, Pointer(0x4444));
    f.Ctrl(64, 0, Pointer(0x4444), 1);
    f.ExpectInternal<void*>(MsgCallbackArg, nullptr);
    f.Ctrl(16, 5, nullptr, 1);
}

static void TestTicketKeys() {
    Fixture f;
    f.Ctrl(58, 0, nullptr, 48);
    f.Ctrl(59, 0, nullptr, 48);
    f.Ctrl(58, 47, nullptr, 48);
    f.Ctrl(59, 47, nullptr, 48);
    unsigned char buffer[48];
    std::memset(buffer, 0xee, sizeof buffer);
    f.Ctrl(58, 47, buffer, 0, InvalidTicketKeysLength);
    for (unsigned char byte : buffer) Require(byte == 0xee);
    f.Ctrl(59, 47, buffer, 0, InvalidTicketKeysLength);
    f.Ctrl(59, 49, buffer, 0, InvalidTicketKeysLength);
    f.Ctrl(58, 48, buffer, 1);
    Require(std::memcmp(buffer, f.actual.internal + TicketKeys, 48) == 0);
    for (int i = 0; i < 48; ++i) buffer[i] = static_cast<unsigned char>(i);
    for (int i = 0; i < 48; ++i) f.ExpectInternal<unsigned char>(TicketKeys + i, static_cast<unsigned char>(i));
    f.Ctrl(59, 48, buffer, 1);
    unsigned char keys[48];
    std::memset(keys, 0xee, sizeof keys);
    f.Ctrl(58, 48, keys, 1);
    for (int i = 0; i < 48; ++i) Require(keys[i] == i);
}

static void TestExtraChainCerts() {
    Fixture f;
    void* slot = Pointer(0x5555);
    f.Ctrl(82, 0, &slot, 1);
    Require(slot == nullptr);
    f.SetCtx<void*>(CtxExtraCerts, Pointer(0x7777));
    f.Ctrl(82, 0, &slot, 1);
    Require(slot == Pointer(0x7777));
    f.Throws([&] { SSL_CTX_ctrl_nid_postfix(f.Context(), 82, 0, nullptr); });
}

static void TestTemporaryKeys() {
    Fixture f;
    f.Ctrl(1, 0, nullptr, 0);
    f.Ctrl(2, 0, nullptr, 0, ShouldNotHaveBeenCalled);
    f.Ctrl(5, 0, nullptr, 0, ShouldNotHaveBeenCalled);
    f.Ctrl(6, 0, nullptr, 0, ShouldNotHaveBeenCalled);
    f.Ctrl(7, 0, nullptr, 0, ShouldNotHaveBeenCalled);
}

static void TestOtherCtrlCommands() {
    Fixture f;
    for (int cmd : {0, 8, 13, 15, 17, 55, 65, 999, -1}) f.Ctrl(cmd, 0, nullptr, 0);
}

static void TestCallbackCtrl() {
    Fixture f;
    f.ExpectInternal<void*>(MsgCallback, Pointer(0x6666));
    f.CallbackCtrl(15, Pointer(0x6666), 1);
    f.ExpectInternal<void*>(ServernameCallback, Pointer(0x7777));
    f.CallbackCtrl(53, Pointer(0x7777), 1);
    f.ExpectInternal<void*>(StatusCallback, Pointer(0x8888));
    f.CallbackCtrl(63, Pointer(0x8888), 1);
    f.ExpectInternal<void*>(TicketKeyCallback, Pointer(0x9999));
    f.CallbackCtrl(72, Pointer(0x9999), 1);
    f.CallbackCtrl(5, Pointer(0x1111), 0, ShouldNotHaveBeenCalled);
    f.CallbackCtrl(0, Pointer(0x1111), 0);
    f.CallbackCtrl(16, Pointer(0x1111), 0);
    f.CallbackCtrl(999, Pointer(0x1111), 0);
}

static void TestSetMsgCallback() {
    Fixture f;
    f.ExpectInternal<void*>(MsgCallback, Pointer(0xaaaa));
    SSL_CTX_set_msg_callback_nid_postfix(f.Context(), Pointer(0xaaaa));
    f.Verify(0, std::source_location::current());
    f.ExpectInternal<void*>(MsgCallback, nullptr);
    SSL_CTX_set_msg_callback_nid_postfix(f.Context(), nullptr);
    f.Verify(0, std::source_location::current());
}

static void TestSetVerify() {
    Fixture f;
    f.ExpectCtx<int>(CtxVerifyMode, 3);
    f.ExpectInternal<void*>(DefaultVerifyCallback, Pointer(0xbbbb));
    SSL_CTX_set_verify_nid_postfix(f.Context(), 3, Pointer(0xbbbb));
    f.Verify(0, std::source_location::current());
    f.ExpectCtx<int>(CtxVerifyMode, -1);
    f.ExpectInternal<void*>(DefaultVerifyCallback, nullptr);
    SSL_CTX_set_verify_nid_postfix(f.Context(), -1, nullptr);
    f.Verify(0, std::source_location::current());
    f.ExpectCtx<int>(CtxVerifyMode, 0x7fffffff);
    f.ExpectInternal<void*>(DefaultVerifyCallback, Pointer(0xcccc));
    SSL_CTX_set_verify_nid_postfix(f.Context(), 0x7fffffff, Pointer(0xcccc));
    f.Verify(0, std::source_location::current());
}

static void TestSetSessionIdContext() {
    Fixture f;
    const unsigned char abc[] = {'a', 'b', 'c'};
    unsigned char sequence[33];
    for (int i = 0; i < 33; ++i) sequence[i] = static_cast<unsigned char>(i + 1);
    const unsigned char q[] = {'q'};
    const unsigned char zz[] = {'z', 'z'};
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 3);
    for (int i = 0; i < 3; ++i) f.ExpectCtx<unsigned char>(CtxSidCtx + i, abc[i]);
    f.SessionIdContext(abc, 3, 1);
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 32);
    for (int i = 0; i < 32; ++i) f.ExpectCtx<unsigned char>(CtxSidCtx + i, sequence[i]);
    f.SessionIdContext(sequence, 32, 1);
    f.SessionIdContext(sequence, 33, 0, SessionIdContextTooLong);
    f.SessionIdContext(sequence, UINT32_MAX, 0, SessionIdContextTooLong);
    f.SessionIdContext(nullptr, 33, 0, SessionIdContextTooLong);
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 0);
    f.SessionIdContext(q, 0, 1);
    f.SessionIdContext(nullptr, 0, 1);
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 32);
    f.SessionIdContext(sequence, 32, 1);
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 2);
    f.ExpectCtx<unsigned char>(CtxSidCtx, 'z');
    f.ExpectCtx<unsigned char>(CtxSidCtx + 1, 'z');
    f.SessionIdContext(zz, 2, 1);
    for (int i = 2; i < 32; ++i) Require(f.actual.ctx[CtxSidCtx + i] == i + 1);
    f.Throws([&] { SSL_CTX_set_session_id_context_nid_postfix(f.Context(), nullptr, 1); });
}

static void TestNullContext() {
    const unsigned char sidCtx[] = {1};
    RequireThrows([] { SSL_CTX_ctrl_nid_postfix(nullptr, 32, 1, nullptr); });
    RequireThrows([] { SSL_CTX_callback_ctrl_nid_postfix(nullptr, 15, nullptr); });
    RequireThrows([] { SSL_CTX_set_msg_callback_nid_postfix(nullptr, nullptr); });
    RequireThrows([] { SSL_CTX_set_verify_nid_postfix(nullptr, 1, nullptr); });
    RequireThrows([&] { SSL_CTX_set_session_id_context_nid_postfix(nullptr, sidCtx, 1); });
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestNullInternal() {
    Fixture f;
    f.SetCtx<void*>(CtxInternal, nullptr);
    unsigned char keys[48] {};
    for (int cmd = -1; cmd <= 120; ++cmd) {
        f.Throws([&] { SSL_CTX_ctrl_nid_postfix(f.Context(), cmd, 48, keys); });
        f.Throws([&] { SSL_CTX_callback_ctrl_nid_postfix(f.Context(), cmd, Pointer(0x1111)); });
    }
    f.Throws([&] { SSL_CTX_ctrl_nid_postfix(f.Context(), 999, 0, nullptr); });
    f.Throws([&] { SSL_CTX_callback_ctrl_nid_postfix(f.Context(), 999, nullptr); });
    f.Throws([&] { SSL_CTX_set_msg_callback_nid_postfix(f.Context(), Pointer(0xaaaa)); });
    f.Throws([&] { SSL_CTX_set_verify_nid_postfix(f.Context(), 3, Pointer(0xbbbb)); });
    const unsigned char abc[] = {'a', 'b', 'c'};
    f.ExpectCtx<unsigned int>(CtxSidCtxLength, 3);
    for (int i = 0; i < 3; ++i) f.ExpectCtx<unsigned char>(CtxSidCtx + i, abc[i]);
    f.SessionIdContext(abc, 3, 1);
}

int main() {
    ERR_clear_error_nid_postfix();
    TestOptionsAndMode();
    TestReadAhead();
    TestMaxCertList();
    TestSessionCache();
    TestMaxSendFragment();
    TestStats();
    TestCallbackArgs();
    TestTicketKeys();
    TestExtraChainCerts();
    TestTemporaryKeys();
    TestOtherCtrlCommands();
    TestCallbackCtrl();
    TestSetMsgCallback();
    TestSetVerify();
    TestSetSessionIdContext();
    TestNullContext();
    TestNullInternal();
    return 0;
}
