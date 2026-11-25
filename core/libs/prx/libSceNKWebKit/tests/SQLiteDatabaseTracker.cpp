#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::SQLiteAbi::SQLiteDatabaseTrackerClient;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::SQLiteDatabaseTracker check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using SetClient = void (APS5_VABI*)(SQLiteDatabaseTrackerClient*);
using Count = void (APS5_VABI*)();
using HasTransaction = bool (APS5_VABI*)();
static_assert(std::is_same_v<decltype(&_ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE), SetClient>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21SQLiteDatabaseTracker35incrementTransactionInProgressCountEv), Count>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21SQLiteDatabaseTracker35decrementTransactionInProgressCountEv), Count>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21SQLiteDatabaseTracker24hasTransactionInProgressEv), HasTransaction>);

struct FakeClient {
    SQLiteDatabaseTrackerClient base;
    int willBegin;
    int didFinish;
};

void APS5_VABI FakeDestructor(SQLiteDatabaseTrackerClient*) {
    Require(false);
}

void APS5_VABI FakeWillBeginFirstTransaction(SQLiteDatabaseTrackerClient* self) {
    ++reinterpret_cast<FakeClient*>(self)->willBegin;
}

void APS5_VABI FakeDidFinishLastTransaction(SQLiteDatabaseTrackerClient* self) {
    ++reinterpret_cast<FakeClient*>(self)->didFinish;
}

void* const fakeVtable[] = {
    reinterpret_cast<void*>(&FakeDestructor),
    reinterpret_cast<void*>(&FakeDestructor),
    reinterpret_cast<void*>(&FakeWillBeginFirstTransaction),
    reinterpret_cast<void*>(&FakeDidFinishLastTransaction),
};

void Increment() {
    _ZN7WebCore21SQLiteDatabaseTracker35incrementTransactionInProgressCountEv();
}

void Decrement() {
    _ZN7WebCore21SQLiteDatabaseTracker35decrementTransactionInProgressCountEv();
}

bool HasTransactionInProgress() {
    return _ZN7WebCore21SQLiteDatabaseTracker24hasTransactionInProgressEv();
}

void TestWithoutClient() {
    Require(HasTransactionInProgress());
    Increment();
    Decrement();
    Decrement();
    Require(HasTransactionInProgress());
    std::printf("WebCore::SQLiteDatabaseTracker without a client: counting is a no-op and a transaction is reported passed\n");
}

void TestCounting(FakeClient& client) {
    _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(&client.base);
    Require(!HasTransactionInProgress());
    Increment();
    Require(client.willBegin == 1 && client.didFinish == 0);
    Require(HasTransactionInProgress());
    Increment();
    Require(client.willBegin == 1 && client.didFinish == 0);
    Decrement();
    Require(client.willBegin == 1 && client.didFinish == 0);
    Require(HasTransactionInProgress());
    Decrement();
    Require(client.willBegin == 1 && client.didFinish == 1);
    Require(!HasTransactionInProgress());
    Increment();
    Require(client.willBegin == 2 && client.didFinish == 1);
    Decrement();
    Require(client.willBegin == 2 && client.didFinish == 2);
    std::printf("WebCore::SQLiteDatabaseTracker with a client: first increment and last decrement call the client passed\n");
}

void TestUnderflow(FakeClient& client) {
    RequireThrows([] { Decrement(); });
    Require(client.willBegin == 2 && client.didFinish == 2);
    Require(!HasTransactionInProgress());
    std::printf("WebCore::SQLiteDatabaseTracker decrement at zero: throws without a callback passed\n");
}

void TestClearClient(FakeClient& client) {
    Increment();
    Require(client.willBegin == 3);
    _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(nullptr);
    Require(HasTransactionInProgress());
    Decrement();
    Increment();
    Require(client.willBegin == 3 && client.didFinish == 2);
    _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(&client.base);
    Require(HasTransactionInProgress());
    Decrement();
    Require(client.willBegin == 3 && client.didFinish == 3);
    Require(!HasTransactionInProgress());
    _ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE(nullptr);
    Require(HasTransactionInProgress());
    std::printf("WebCore::SQLiteDatabaseTracker client cleared: count is kept while counting stops passed\n");
}

}

int main() {
    FakeClient client{{fakeVtable}, 0, 0};
    TestWithoutClient();
    TestCounting(client);
    TestUnderflow(client);
    TestClearClient(client);
}
