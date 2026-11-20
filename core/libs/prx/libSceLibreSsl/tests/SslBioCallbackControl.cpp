#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

using LibreSsl::Bio;
using LibreSsl::BioInfoCallback;
using LibreSsl::BioMethod;

constexpr int SetCallback = 14;
constexpr int GetCallback = 15;
constexpr std::uint64_t UnsupportedMethodError = 0x20fff079;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

int allocations = 0;

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) ++allocations;
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count ? count : 1, size ? size : 1);
    if (pointer) ++allocations;
    return pointer;
}

void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    void* moved = std::realloc(pointer, size ? size : 1);
    if (moved && !pointer) ++allocations;
    return moved;
}

void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected posix_memalign"); }

void InitializeHeap() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(AllocateZeroed),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void APS5_VABI FirstInfo(Bio*, int, const char*, int, std::int64_t, std::int64_t) {
    std::abort();
}

void APS5_VABI SecondInfo(Bio*, int, const char*, int, std::int64_t, std::int64_t) {
    std::exit(3);
}

struct MethodCall {
    Bio* bio;
    int command;
    BioInfoCallback callback;
};

struct CallbackCall {
    Bio* bio;
    int operation;
    BioInfoCallback callback;
    int command;
    std::int64_t argument;
    std::int64_t result;
};

int methodCalls = 0;
MethodCall lastMethodCall{};
std::int64_t methodResult = 0;

int callbackCalls = 0;
CallbackCall callbackLog[2]{};
std::int64_t callbackResults[2]{};

std::int64_t APS5_VABI RecordCallbackControl(Bio* bio, int command, BioInfoCallback callback) {
    ++methodCalls;
    lastMethodCall = {bio, command, callback};
    return methodResult;
}

std::int64_t APS5_VABI RecordBioCallback(Bio* bio, int operation, const char* pointer, int command, std::int64_t argument, std::int64_t result) {
    Require(callbackCalls < 2, "the BIO callback runs at most twice per call");
    BioInfoCallback callback = nullptr;
    std::memcpy(&callback, pointer, sizeof(callback));
    callbackLog[callbackCalls] = {bio, operation, callback, command, argument, result};
    return callbackResults[callbackCalls++];
}

const BioMethod ForwardMethod{0x7f, "forward", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RecordCallbackControl};

void Reset(std::int64_t result, std::int64_t first, std::int64_t second) {
    methodCalls = 0;
    lastMethodCall = {};
    methodResult = result;
    callbackCalls = 0;
    callbackLog[0] = {};
    callbackLog[1] = {};
    callbackResults[0] = first;
    callbackResults[1] = second;
}

void* StoredInfo(const LibreSslSsl* ssl) {
    void* value = nullptr;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(ssl->internal) + 96, sizeof(value));
    return value;
}

std::int64_t Control(Bio* bio, int command, BioInfoCallback callback) {
    return BIO_f_ssl_nid_postfix()->callbackControl(bio, command, callback);
}

template <class TFunction>
bool ThrowsInvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestSetCallback(Bio* sslBio, LibreSslSsl* ssl, Bio* forward) {
    Require(StoredInfo(ssl) == nullptr, "a new SSL has no info callback");
    ssl->rbio = nullptr;
    Reset(7, 1, 1);
    Require(Control(sslBio, SetCallback, FirstInfo) == 1, "setting the info callback without a read BIO returns 1");
    Require(StoredInfo(ssl) == reinterpret_cast<void*>(FirstInfo), "the info callback is stored at offset 96 of the internal state");
    ssl->rbio = forward;
    forward->callback = RecordBioCallback;
    Require(Control(sslBio, SetCallback, SecondInfo) == 1, "setting the info callback returns 1");
    Require(StoredInfo(ssl) == reinterpret_cast<void*>(SecondInfo), "setting replaces the info callback");
    Require(methodCalls == 0 && callbackCalls == 0, "setting the info callback does not reach the read BIO");
    Require(Control(sslBio, SetCallback, nullptr) == 1 && StoredInfo(ssl) == nullptr, "a null callback clears the info callback");
    forward->callback = nullptr;
    LibreSslSslInternal* internal = ssl->internal;
    ssl->internal = nullptr;
    Require(ThrowsInvalidArgument([&] { Control(sslBio, SetCallback, FirstInfo); }), "setting through an SSL without internal state throws");
    Require(Control(sslBio, GetCallback, FirstInfo) == 7 && methodCalls == 1, "forwarding does not read the internal state");
    ssl->internal = internal;
    ssl->rbio = nullptr;
}

void TestForwarding(Bio* sslBio, LibreSslSsl* ssl, Bio* forward) {
    ssl->rbio = forward;
    for (const int command : {GetCallback, 1, 99, -1}) {
        Reset(command * 3 + 5, 1, 1);
        Require(Control(sslBio, command, SecondInfo) == command * 3 + 5, "the read BIO method result is returned");
        Require(methodCalls == 1 && lastMethodCall.bio == forward && lastMethodCall.command == command && lastMethodCall.callback == SecondInfo,
            "other commands reach the read BIO method with the same arguments");
    }
    Require(StoredInfo(ssl) == nullptr, "forwarded commands leave the info callback alone");
    ssl->rbio = nullptr;
    Reset(7, 1, 1);
    Require(Control(sslBio, GetCallback, SecondInfo) == 0 && Control(sslBio, 99, SecondInfo) == 0 && methodCalls == 0, "a missing read BIO returns 0");
}

void TestUnsupportedReadBio(Bio* sslBio, LibreSslSsl* ssl) {
    Bio* memory = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(memory != nullptr, "memory BIO");
    ssl->rbio = memory;
    ERR_clear_error_nid_postfix();
    Require(Control(sslBio, GetCallback, FirstInfo) == -2, "a read BIO without callback control returns -2");
    Require(ERR_get_error_nid_postfix() == UnsupportedMethodError && ERR_get_error_nid_postfix() == 0, "a read BIO without callback control queues UNSUPPORTED_METHOD");
    ssl->rbio = nullptr;
    Require(BIO_free_nid_postfix(memory) == 1, "free memory BIO");
}

void TestReadBioCallback(Bio* sslBio, LibreSslSsl* ssl, Bio* forward) {
    ssl->rbio = forward;
    forward->callback = RecordBioCallback;
    for (const std::int64_t veto : {0, -5}) {
        Reset(7, veto, 1);
        Require(Control(sslBio, 99, FirstInfo) == veto, "a callback result of at most 0 is returned");
        Require(methodCalls == 0 && callbackCalls == 1, "a vetoing callback skips the method");
        const CallbackCall& before = callbackLog[0];
        Require(before.bio == forward && before.operation == 6 && before.callback == FirstInfo && before.command == 99 && before.argument == 0 && before.result == 1,
            "the callback first sees BIO_CB_CTRL with the callback pointer, command, 0 and 1");
    }
    Reset(7, 2, 42);
    Require(Control(sslBio, GetCallback, SecondInfo) == 42, "the second callback result is returned");
    Require(methodCalls == 1 && lastMethodCall.bio == forward && lastMethodCall.command == GetCallback && lastMethodCall.callback == SecondInfo,
        "the method runs between the callbacks");
    Require(callbackCalls == 2, "the callback runs before and after the method");
    const CallbackCall& after = callbackLog[1];
    Require(after.bio == forward && after.operation == 0x86 && after.callback == SecondInfo && after.command == GetCallback && after.argument == 0 && after.result == 7,
        "the callback then sees BIO_CB_CTRL|BIO_CB_RETURN with the method result");
    forward->callback = nullptr;
    ssl->rbio = nullptr;
}

void TestUnattached() {
    Bio* bio = BIO_new_nid_postfix(BIO_f_ssl_nid_postfix());
    Require(bio != nullptr, "SSL BIO");
    Require(ThrowsInvalidArgument([&] { Control(bio, SetCallback, FirstInfo); }), "setting the info callback on an unattached SSL BIO throws");
    Require(ThrowsInvalidArgument([&] { Control(bio, GetCallback, FirstInfo); }), "forwarding from an unattached SSL BIO throws");
    Require(ThrowsInvalidArgument([&] { Control(bio, 99, nullptr); }), "any other command on an unattached SSL BIO throws");
    Require(BIO_free_nid_postfix(bio) == 1, "free unattached SSL BIO");
}

}

int main() {
    InitializeHeap();
    LibreSslCtx* context = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    Require(context != nullptr, "SSL_CTX_new");
    LibreSslSsl* ssl = SSL_new_nid_postfix(context);
    Require(ssl != nullptr && ssl->internal != nullptr && ssl->rbio == nullptr, "SSL_new");
    Bio* sslBio = BIO_new_nid_postfix(BIO_f_ssl_nid_postfix());
    Bio* forward = BIO_new_nid_postfix(&ForwardMethod);
    Require(sslBio != nullptr && forward != nullptr, "BIO_new");
    *static_cast<void**>(sslBio->data) = ssl;

    TestSetCallback(sslBio, ssl, forward);
    TestForwarding(sslBio, ssl, forward);
    TestUnsupportedReadBio(sslBio, ssl);
    TestReadBioCallback(sslBio, ssl, forward);

    *static_cast<void**>(sslBio->data) = nullptr;
    Require(BIO_free_nid_postfix(sslBio) == 1 && BIO_free_nid_postfix(forward) == 1, "BIO_free");
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(context);
    TestUnattached();
    Require(allocations == 0, "every allocation is released");
    return 0;
}
