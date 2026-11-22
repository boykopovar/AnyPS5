#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/StreamRuntime.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal stream runtime: %s\n", message);
    std::abort();
}

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

std::string Take(GuestLocale::String& text) {
    std::string copy(text.capacity > 15 ? text.pointer : text.buffer, text.size);
    if (text.capacity > 15) _ZdlPv_nid_postfix(text.pointer);
    return copy;
}

void CheckCategories() {
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    const auto* system = _ZSt15system_categoryv_nid_postfix();
    GuestLocale::String text{};
    text.capacity = 15;

    Require(_ZNKSt24_Iostream_error_category7messageEi_nid_postfix(&text, nullptr, 1) == &text, "message returns the caller's buffer");
    Require(Take(text) == "iostream stream error", "iostream error 1 is the stream error");
    text = {};
    text.capacity = 15;
    _ZNKSt24_Iostream_error_category7messageEi_nid_postfix(&text, nullptr, 2);
    Require(Take(text) == "no such file or directory", "other iostream values use the system table");
    text = {};
    text.capacity = 15;
    _ZNKSt24_Iostream_error_category7messageEi_nid_postfix(&text, nullptr, 0);
    Require(Take(text) == "unknown error", "an unlisted iostream value is unknown");

    struct Expected {
        std::int32_t value;
        const char* message;
    };
    const Expected table[] = {
        {1, "operation not permitted"}, {35, "operation would block"}, {45, "not supported"}, {25, "inappropriate io control operation"},
        {107, "state not recoverable"}, {1074, "stream timeout"}, {95, "unknown error"}, {0, "unknown error"}, {-5, "unknown error"},
    };
    for (const auto& entry : table) {
        text = {};
        text.capacity = 15;
        Require(_ZNKSt22_System_error_category7messageEi_nid_postfix(&text, system, entry.value) == &text, "system message returns the caller's buffer");
        Require(Take(text) == entry.message, "a system message follows the bundled module's table");
    }

    for (std::int32_t value : {1, 2, 107, 1040}) {
        const auto condition = _ZNKSt22_System_error_category23default_error_conditionEi_nid_postfix(system, value);
        Require(condition.value == value && condition.category == generic, "a listed system code maps to the generic category");
    }
    for (std::int32_t value : {0, 95, 999, -1}) {
        const auto condition = _ZNKSt22_System_error_category23default_error_conditionEi_nid_postfix(system, value);
        Require(condition.value == value && condition.category == system, "an unlisted system code stays in the system category");
    }
    _ZNSt24_Iostream_error_categoryD1Ev_nid_postfix(nullptr);
}

struct FakeBuffer {
    alignas(16) unsigned char bytes[0xc0];

    template<typename T> T& At(std::size_t offset) { return *reinterpret_cast<T*>(bytes + offset); }
};

int g_overflowResult = 0;
int g_overflowCalls = 0;
int g_overflowArgument = 0;
int g_alwaysNoconvCalls = 0;
bool g_alwaysNoconv = false;
int g_unshiftCalls = 0;
int g_unshiftMode = 0;
std::size_t g_largestWindow = 0;

int APS5_VABI FakeOverflow(void*, int character) {
    ++g_overflowCalls;
    g_overflowArgument = character;
    return g_overflowResult;
}

bool APS5_VABI FakeAlwaysNoconv(void*) {
    ++g_alwaysNoconvCalls;
    return g_alwaysNoconv;
}

int APS5_VABI FakeUnshift(void*, void*, char* to, char* toEnd, char** toNext) {
    ++g_unshiftCalls;
    const std::size_t window = static_cast<std::size_t>(toEnd - to);
    if (window > g_largestWindow) g_largestWindow = window;
    switch (g_unshiftMode) {
    case 0:
        to[0] = 'X';
        to[1] = 'Y';
        *toNext = to + 2;
        return 0;
    case 1:
        if (window < 16) {
            *toNext = to;
            return 1;
        }
        std::memcpy(to, "abc", 3);
        *toNext = to + 3;
        return 0;
    case 2:
        *toNext = to;
        return 2;
    default:
        *toNext = to;
        return 3;
    }
}

void* g_filebufSlots[8];
void* g_codecvtSlots[16];
const void* g_codecvtObject = g_codecvtSlots;

void SetupSlots() {
    g_filebufSlots[4] = reinterpret_cast<void*>(&FakeOverflow);
    g_codecvtSlots[4] = reinterpret_cast<void*>(&FakeAlwaysNoconv);
    g_codecvtSlots[9] = reinterpret_cast<void*>(&FakeUnshift);
}

void ResetFakes() {
    g_overflowResult = 0;
    g_overflowCalls = 0;
    g_overflowArgument = 0;
    g_alwaysNoconvCalls = 0;
    g_alwaysNoconv = false;
    g_unshiftCalls = 0;
    g_unshiftMode = 0;
    g_largestWindow = 0;
}

std::string ReadAll(FileStream& stream) {
    fflush_nid_postfix(&stream);
    fseek_nid_postfix(&stream, 0, 0);
    char data[64]{};
    const std::size_t count = fread_nid_postfix(data, 1, sizeof(data), &stream);
    return {data, count};
}

void CheckSync(int (APS5_VABI* sync)(void*)) {
    FakeBuffer buffer{};
    buffer.At<void*>(0) = g_filebufSlots;
    ResetFakes();
    Require(sync(&buffer) == 0 && g_overflowCalls == 0, "a buffer without a file syncs without writing");

    FileStream stream(TemporaryFile::Open());
    buffer.At<FileStream*>(0xa8) = &stream;
    fwrite_nid_postfix("data", 1, 4, &stream);
    g_overflowResult = -1;
    Require(sync(&buffer) == 0 && g_overflowCalls == 1 && g_overflowArgument == -1, "a failed overflow(EOF) ends the sync with 0");
    g_overflowResult = 0;
    Require(sync(&buffer) == 0 && g_overflowCalls == 2, "a successful overflow(EOF) is followed by a flush");
    Require(ReadAll(stream) == "data", "the flush reaches the file");

    bool threw = false;
    try {
        sync(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null buffer is rejected");
}

template<typename Element> void CheckPbackfail(int (APS5_VABI* pbackfail)(void*, std::int32_t)) {
    FakeBuffer buffer{};
    buffer.At<void*>(0) = g_filebufSlots;
    Element text[4] = {'a', 'b', 'c', 0};
    Element* first = text;
    Element* next = text + 2;
    Element* end = text + 3;
    buffer.At<Element**>(0x18) = &first;
    buffer.At<Element**>(0x38) = &next;
    buffer.At<Element**>(0x58) = &end;

    Require(pbackfail(&buffer, -1) == 0 && next == text + 1, "pbackfail(EOF) with room backs up and reports success");
    next = text + 2;
    Require(pbackfail(&buffer, 'b') == 'b' && next == text + 1, "putting back the previous element only backs up");
    next = text + 2;
    Require(pbackfail(&buffer, 'z') == -1 && next == text + 2, "a different element without a file fails");
    next = text;
    Require(pbackfail(&buffer, -1) == -1, "pbackfail(EOF) without room fails");

    if constexpr (sizeof(Element) == 1) {
        FileStream stream(TemporaryFile::Open());
        fwrite_nid_postfix("hello", 1, 5, &stream);
        fseek_nid_postfix(&stream, 0, 0);
        fgetc_nid_postfix(&stream);
        buffer.At<FileStream*>(0xa8) = &stream;
        next = text;
        Require(pbackfail(&buffer, 'h') == 'h', "an unconverted buffer pushes the element back into the file");
        Require(fgetc_nid_postfix(&stream) == 'h', "the pushed back element is read again");
    }
    first = text;
    next = text;
    end = text + 3;
    buffer.At<FileStream*>(0xa8) = nullptr;
    FileStream cellStream(TemporaryFile::Open());
    buffer.At<FileStream*>(0xa8) = &cellStream;
    buffer.At<void*>(0x80) = &g_codecvtObject;
    Element* const mychar = &buffer.At<Element>(0x88);
    Require(pbackfail(&buffer, 'q') == 'q', "a converting buffer stores the element in its own cell");
    Require(*mychar == 'q' && first == mychar && next == mychar && end == mychar + 1, "the get area becomes the one element cell");
    Require(buffer.At<Element*>(0x70) == text && buffer.At<Element*>(0x78) == text + 3, "the previous get area is saved");
    first = mychar;
    next = mychar;
    Require(pbackfail(&buffer, 'r') == -1, "the cell cannot be used twice");
    Require(pbackfail(&buffer, -1) == -1, "pbackfail(EOF) at the start of the cell fails");

    bool threw = false;
    try {
        pbackfail(nullptr, 'a');
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null buffer is rejected");
}

template<typename Element> void CheckEndwrite(bool (APS5_VABI* endwrite)(void*), std::size_t wroteOffset, bool narrow) {
    FakeBuffer buffer{};
    buffer.At<void*>(0) = g_filebufSlots;
    FileStream stream(TemporaryFile::Open());
    buffer.At<FileStream*>(0xa8) = &stream;

    ResetFakes();
    Require(endwrite(&buffer), "a buffer without a converter has nothing to finish");
    buffer.At<void*>(0x80) = &g_codecvtObject;
    Require(endwrite(&buffer) && g_overflowCalls == 0, "a buffer that wrote nothing needs no shift sequence");

    buffer.At<bool>(wroteOffset) = true;
    g_overflowResult = -1;
    Require(!endwrite(&buffer) && g_overflowCalls == 1, "a failing overflow(EOF) fails the end of the write");
    g_overflowResult = 0;

    if (narrow) {
        g_alwaysNoconv = true;
        Require(endwrite(&buffer) && g_unshiftCalls == 0 && g_alwaysNoconvCalls == 1, "an always-noconv converter needs no shift sequence");
        g_alwaysNoconv = false;
    }

    ResetFakes();
    g_unshiftMode = 0;
    Require(endwrite(&buffer) && g_unshiftCalls == 1 && !buffer.At<bool>(wroteOffset), "a complete shift sequence is written and clears the flag");
    Require(ReadAll(stream) == "XY", "the shift sequence reaches the file");
    fseek_nid_postfix(&stream, 0, 2);

    buffer.At<bool>(wroteOffset) = true;
    ResetFakes();
    g_unshiftMode = 1;
    Require(endwrite(&buffer) && g_unshiftCalls == 2 && g_largestWindow >= 16 && !buffer.At<bool>(wroteOffset), "a partial result without output grows the window");
    Require(ReadAll(stream) == "XYabc", "the grown shift sequence reaches the file");
    fseek_nid_postfix(&stream, 0, 2);

    buffer.At<bool>(wroteOffset) = true;
    ResetFakes();
    g_unshiftMode = 2;
    Require(!endwrite(&buffer), "a conversion error fails the end of the write");
    buffer.At<bool>(wroteOffset) = true;
    g_unshiftMode = 3;
    Require(endwrite(&buffer) && buffer.At<bool>(wroteOffset), "a no-conversion result succeeds and keeps the flag");

    bool threw = false;
    try {
        endwrite(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null buffer is rejected");
}

}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&HeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&HeapFree);
    heapApi[2] = reinterpret_cast<void*>(&HeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&HeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&HeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&HeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    SetupSlots();
    CheckCategories();
    CheckSync(_ZNSt13basic_filebufIcSt11char_traitsIcEE4syncEv_nid_postfix);
    CheckPbackfail<char>(_ZNSt13basic_filebufIcSt11char_traitsIcEE9pbackfailEi_nid_postfix);
    CheckPbackfail<char16_t>(_ZNSt13basic_filebufIwSt11char_traitsIwEE9pbackfailEi_nid_postfix);
    CheckEndwrite<char>(_ZNSt13basic_filebufIcSt11char_traitsIcEE9_EndwriteEv_nid_postfix, 0x89, true);
    CheckEndwrite<char16_t>(_ZNSt13basic_filebufIwSt11char_traitsIwEE9_EndwriteEv_nid_postfix, 0x8a, false);
    return 0;
}
