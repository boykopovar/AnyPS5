#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

static int APS5_VABI Open(void*) { return 1; }
static int APS5_VABI Close(void*) { return 2; }
static int APS5_VABI Write(void*, void*) { return 3; }
static int APS5_VABI Read(void*, void*) { return 4; }

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
    LibreSslUiMethod* method = UI_create_method_nid_postfix(name);
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

static void TestSetOpener() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_set_opener_nid_postfix(method, Open) == 0 && method->openSession == Open);
    Require(!method->writeString && !method->readString && !method->closeSession && !method->flush && !method->constructPrompt);
    Require(UI_method_set_opener_nid_postfix(method, nullptr) == 0 && method->openSession == nullptr);
    Require(UI_method_set_opener_nid_postfix(nullptr, Open) == -1);
    UI_destroy_method_nid_postfix(method);
}

static void TestGetOpener() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_get_opener_nid_postfix(method) == nullptr);
    Require(UI_method_set_opener_nid_postfix(method, Open) == 0);
    Require(UI_method_get_opener_nid_postfix(method) == Open);
    Require(UI_method_get_opener_nid_postfix(method)(nullptr) == 1);
    Require(UI_method_get_opener_nid_postfix(nullptr) == nullptr);
    UI_destroy_method_nid_postfix(method);
}

static void TestSetWriter() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_set_writer_nid_postfix(method, Write) == 0 && method->writeString == Write);
    Require(!method->openSession && !method->readString && !method->closeSession && !method->flush && !method->constructPrompt);
    Require(UI_method_set_writer_nid_postfix(method, nullptr) == 0 && method->writeString == nullptr);
    Require(UI_method_set_writer_nid_postfix(nullptr, Write) == -1);
    UI_destroy_method_nid_postfix(method);
}

static void TestGetWriter() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_get_writer_nid_postfix(method) == nullptr);
    Require(UI_method_set_writer_nid_postfix(method, Write) == 0);
    Require(UI_method_get_writer_nid_postfix(method) == Write);
    Require(UI_method_get_writer_nid_postfix(method)(nullptr, nullptr) == 3);
    Require(UI_method_get_writer_nid_postfix(nullptr) == nullptr);
    UI_destroy_method_nid_postfix(method);
}

static void TestSetReader() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_set_reader_nid_postfix(method, Read) == 0 && method->readString == Read);
    Require(!method->openSession && !method->writeString && !method->closeSession && !method->flush && !method->constructPrompt);
    Require(UI_method_set_reader_nid_postfix(method, nullptr) == 0 && method->readString == nullptr);
    Require(UI_method_set_reader_nid_postfix(nullptr, Read) == -1);
    UI_destroy_method_nid_postfix(method);
}

static void TestGetReader() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_get_reader_nid_postfix(method) == nullptr);
    Require(UI_method_set_reader_nid_postfix(method, Read) == 0);
    Require(UI_method_get_reader_nid_postfix(method) == Read);
    Require(UI_method_get_reader_nid_postfix(method)(nullptr, nullptr) == 4);
    Require(UI_method_get_reader_nid_postfix(nullptr) == nullptr);
    UI_destroy_method_nid_postfix(method);
}

static void TestSetCloser() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_set_closer_nid_postfix(method, Close) == 0 && method->closeSession == Close);
    Require(!method->openSession && !method->writeString && !method->readString && !method->flush && !method->constructPrompt);
    Require(UI_method_set_closer_nid_postfix(method, nullptr) == 0 && method->closeSession == nullptr);
    Require(UI_method_set_closer_nid_postfix(nullptr, Close) == -1);
    UI_destroy_method_nid_postfix(method);
}

static void TestGetCloser() {
    LibreSslUiMethod* method = UI_create_method_nid_postfix(nullptr);
    Require(UI_method_get_closer_nid_postfix(method) == nullptr);
    Require(UI_method_set_closer_nid_postfix(method, Close) == 0);
    Require(UI_method_get_closer_nid_postfix(method) == Close);
    Require(UI_method_get_closer_nid_postfix(method)(nullptr) == 2);
    Require(UI_method_get_closer_nid_postfix(nullptr) == nullptr);
    UI_destroy_method_nid_postfix(method);
}

static std::uint64_t UiError(std::uint64_t reason) {
    return std::uint64_t{40} << 24 | std::uint64_t{0xfff} << 12 | reason;
}

static void TestSetResult() {
    ERR_clear_error_nid_postfix();
    char buffer[8] {};
    LibreSslUi ui {};
    LibreSslUiString prompt {};
    prompt.type = 1;
    prompt.resultBuffer = buffer;
    prompt.data.stringData.resultMinimum = 2;
    prompt.data.stringData.resultMaximum = 5;
    ui.flags = 0x0101;
    Require(UI_set_result_nid_postfix(&ui, &prompt, "abc") == 0);
    Require(!std::strcmp(buffer, "abc") && ui.flags == 0x0100 && ERR_peek_error_nid_postfix() == 0);
    Require(UI_set_result_nid_postfix(&ui, &prompt, "abcde") == 0 && !std::strcmp(buffer, "abcde") && ERR_peek_error_nid_postfix() == 0);
    Require(UI_set_result_nid_postfix(&ui, &prompt, "ab") == 0 && !std::strcmp(buffer, "ab") && buffer[3] == 'd');

    std::memcpy(buffer, "kept", 5);
    Require(UI_set_result_nid_postfix(&ui, &prompt, "a") == -1);
    Require(ui.flags == 0x0101 && !std::strcmp(buffer, "kept"));
    Require(ERR_get_error_nid_postfix() == UiError(101) && ERR_get_error_nid_postfix() == 0);
    Require(UI_set_result_nid_postfix(&ui, &prompt, "abcdef") == -1);
    Require(ui.flags == 0x0101 && !std::strcmp(buffer, "kept"));
    Require(ERR_get_error_nid_postfix() == UiError(100) && ERR_get_error_nid_postfix() == 0);
    Require(UI_set_result_nid_postfix(&ui, &prompt, "") == -1 && ui.flags == 0x0101);
    Require(ERR_get_error_nid_postfix() == UiError(101));

    Require(UI_set_result_nid_postfix(&ui, nullptr, "abc") == -1);
    Require(ui.flags == 0x0100 && ERR_peek_error_nid_postfix() == 0);

    LibreSslUiString verify = prompt;
    verify.type = 2;
    Require(UI_set_result_nid_postfix(&ui, &verify, "wxyz") == 0 && !std::strcmp(buffer, "wxyz"));
    ui.flags = 0x0001;
    Require(UI_set_result_nid_postfix(&ui, &verify, "uvwxyz") == -1 && ui.flags == 0x0001);
    Require(ERR_get_error_nid_postfix() == UiError(100));

    LibreSslUiString unbuffered = prompt;
    unbuffered.resultBuffer = nullptr;
    ui.flags = 0x0001;
    Require(UI_set_result_nid_postfix(&ui, &unbuffered, "abc") == -1 && ui.flags == 0);
    Require(ERR_get_error_nid_postfix() == UiError(105) && ERR_get_error_nid_postfix() == 0);

    LibreSslUiString boolean {};
    boolean.type = 3;
    boolean.resultBuffer = buffer;
    boolean.data.booleanData.okCharacters = "yY";
    boolean.data.booleanData.cancelCharacters = "nN";
    Require(UI_set_result_nid_postfix(&ui, &boolean, "xN") == 0 && buffer[0] == 'n');
    Require(UI_set_result_nid_postfix(&ui, &boolean, "Yes") == 0 && buffer[0] == 'y');
    Require(UI_set_result_nid_postfix(&ui, &boolean, "xz") == 0 && buffer[0] == '\0');
    std::memcpy(buffer, "kept", 5);
    Require(UI_set_result_nid_postfix(&ui, &boolean, "") == 0 && buffer[0] == '\0' && !std::strcmp(buffer + 1, "ept"));
    boolean.resultBuffer = nullptr;
    Require(UI_set_result_nid_postfix(&ui, &boolean, "y") == -1 && ERR_get_error_nid_postfix() == UiError(105));

    std::memcpy(buffer, "kept", 5);
    LibreSslUiString info = prompt;
    info.type = 4;
    ui.flags = 0x0001;
    Require(UI_set_result_nid_postfix(&ui, &info, "a") == 0 && ui.flags == 0 && !std::strcmp(buffer, "kept"));
    Require(ERR_peek_error_nid_postfix() == 0);
}

int main() {
    InitializeHeap();
    TestCreate();
    TestDestroy();
    TestSetOpener();
    TestGetOpener();
    TestSetWriter();
    TestGetWriter();
    TestSetReader();
    TestGetReader();
    TestSetCloser();
    TestGetCloser();
    TestSetResult();
    std::puts("libSceLibreSsl UI method tests passed");
}
