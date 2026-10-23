#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

using SessionFunction = int (APS5_VABI*)(void*);
using StringFunction = int (APS5_VABI*)(void*, void*);

struct TestUiMethod {
    char* name;
    SessionFunction openSession;
    StringFunction writeString;
    SessionFunction flush;
    StringFunction readString;
    SessionFunction closeSession;
    void* constructPrompt;
};

extern "C" {
TestUiMethod* APS5_VABI UI_create_method_nid_postfix(char*);
void APS5_VABI UI_destroy_method_nid_postfix(TestUiMethod*);
void APS5_VABI free_nid_postfix(void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL UI method check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static void TestCreate() {
    char name[] = "title prompt";
    TestUiMethod* method = UI_create_method_nid_postfix(name);
    Require(method != nullptr);
    Require(method->name != nullptr && method->name != name && !std::strcmp(method->name, "title prompt"));
    name[0] = 'X';
    Require(!std::strcmp(method->name, "title prompt"));
    Require(!method->openSession && !method->writeString && !method->flush && !method->readString && !method->closeSession && !method->constructPrompt);
    free_nid_postfix(method->name);
    free_nid_postfix(method);
    method = UI_create_method_nid_postfix(nullptr);
    Require(method != nullptr && method->name == nullptr);
    free_nid_postfix(method);
    char empty[] = "";
    method = UI_create_method_nid_postfix(empty);
    Require(method != nullptr && method->name != nullptr && method->name != empty && method->name[0] == '\0');
    free_nid_postfix(method->name);
    free_nid_postfix(method);
}

static void TestDestroy() {
    char name[] = "prompt";
    UI_destroy_method_nid_postfix(UI_create_method_nid_postfix(name));
    UI_destroy_method_nid_postfix(UI_create_method_nid_postfix(nullptr));
    RequireThrows([] { UI_destroy_method_nid_postfix(nullptr); });
}

int main() {
    InitializeHeap();
    TestCreate();
    TestDestroy();
    std::puts("libSceLibreSsl UI method tests passed");
}
