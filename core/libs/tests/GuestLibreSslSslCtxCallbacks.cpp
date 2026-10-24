#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestCallbacks {
    void* slots[58];
};

struct TestCtx {
    unsigned char beforeInternal[112];
    TestCallbacks* internal;
};

static_assert(sizeof(TestCallbacks) == 464 && sizeof(TestCtx) == 120);

static int Marker[4];

static bool OnlySlots(const TestCallbacks& callbacks, int first, void* firstValue, int second, void* secondValue) {
    for (int slot = 0; slot < 58; ++slot) {
        void* expected = slot == first ? firstValue : slot == second ? secondValue : nullptr;
        if (callbacks.slots[slot] != expected) return false;
    }
    return true;
}

extern "C" {
void APS5_VABI SSL_CTX_set_info_callback_nid_postfix(TestCtx*, void*);
void APS5_VABI SSL_CTX_sess_set_new_cb_nid_postfix(TestCtx*, void*);
void APS5_VABI SSL_CTX_set_cert_verify_callback_nid_postfix(TestCtx*, void*, void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL_CTX callback check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestInfoCallback() {
    TestCallbacks callbacks {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_set_info_callback_nid_postfix(nullptr, &Marker[0]); });
    RequireThrows([&] { SSL_CTX_set_info_callback_nid_postfix(&ctx, &Marker[0]); });
    ctx.internal = &callbacks;
    SSL_CTX_set_info_callback_nid_postfix(&ctx, &Marker[0]);
    Require(OnlySlots(callbacks, 11, &Marker[0], -1, nullptr));
    SSL_CTX_set_info_callback_nid_postfix(&ctx, nullptr);
    Require(OnlySlots(callbacks, -1, nullptr, -1, nullptr));
}

static void TestNewSessionCallback() {
    TestCallbacks callbacks {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_sess_set_new_cb_nid_postfix(nullptr, &Marker[0]); });
    RequireThrows([&] { SSL_CTX_sess_set_new_cb_nid_postfix(&ctx, &Marker[0]); });
    ctx.internal = &callbacks;
    SSL_CTX_sess_set_new_cb_nid_postfix(&ctx, &Marker[0]);
    Require(OnlySlots(callbacks, 3, &Marker[0], -1, nullptr));
    SSL_CTX_sess_set_new_cb_nid_postfix(&ctx, nullptr);
    Require(OnlySlots(callbacks, -1, nullptr, -1, nullptr));
}

static void TestCertVerifyCallback() {
    TestCallbacks callbacks {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_set_cert_verify_callback_nid_postfix(nullptr, &Marker[0], &Marker[1]); });
    RequireThrows([&] { SSL_CTX_set_cert_verify_callback_nid_postfix(&ctx, &Marker[0], &Marker[1]); });
    ctx.internal = &callbacks;
    SSL_CTX_set_cert_verify_callback_nid_postfix(&ctx, &Marker[0], &Marker[1]);
    Require(OnlySlots(callbacks, 6, &Marker[0], 7, &Marker[1]));
    SSL_CTX_set_cert_verify_callback_nid_postfix(&ctx, nullptr, &Marker[2]);
    Require(OnlySlots(callbacks, 6, nullptr, 7, &Marker[2]));
    SSL_CTX_set_cert_verify_callback_nid_postfix(&ctx, nullptr, nullptr);
    Require(OnlySlots(callbacks, -1, nullptr, -1, nullptr));
}

int main() {
    TestInfoCallback();
    TestNewSessionCallback();
    TestCertVerifyCallback();
    return 0;
}
