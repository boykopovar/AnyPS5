#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/StreamRuntime.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

extern "C" [[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

namespace {

struct SystemErrorMessage {
    std::int32_t value;
    const char* text;
};

constexpr SystemErrorMessage g_systemMessages[] = {
    {47, "address family not supported"},
    {48, "address in use"},
    {49, "address not available"},
    {56, "already connected"},
    {7, "argument list too long"},
    {33, "argument out of domain"},
    {14, "bad address"},
    {9, "bad file descriptor"},
    {89, "bad message"},
    {32, "broken pipe"},
    {53, "connection aborted"},
    {37, "connection already in progress"},
    {61, "connection refused"},
    {54, "connection reset"},
    {18, "cross device link"},
    {39, "destination address required"},
    {16, "device or resource busy"},
    {66, "directory not empty"},
    {8, "executable format error"},
    {17, "file exists"},
    {27, "file too large"},
    {63, "filename too long"},
    {78, "function not supported"},
    {65, "host unreachable"},
    {82, "identifier removed"},
    {86, "illegal byte sequence"},
    {25, "inappropriate io control operation"},
    {4, "interrupted"},
    {22, "invalid argument"},
    {29, "invalid seek"},
    {5, "io error"},
    {21, "is a directory"},
    {40, "message size"},
    {50, "network down"},
    {52, "network reset"},
    {51, "network unreachable"},
    {55, "no buffer space"},
    {10, "no child process"},
    {91, "no link"},
    {77, "no lock available"},
    {1040, "no message available"},
    {83, "no message"},
    {42, "no protocol option"},
    {28, "no space on device"},
    {1050, "no stream resources"},
    {6, "no such device or address"},
    {19, "no such device"},
    {2, "no such file or directory"},
    {3, "no such process"},
    {20, "not a directory"},
    {38, "not a socket"},
    {1051, "not a stream"},
    {57, "not connected"},
    {12, "not enough memory"},
    {45, "not supported"},
    {85, "operation canceled"},
    {36, "operation in progress"},
    {1, "operation not permitted"},
    {45, "operation not supported"},
    {35, "operation would block"},
    {108, "owner dead"},
    {13, "permission denied"},
    {92, "protocol error"},
    {43, "protocol not supported"},
    {30, "read only file system"},
    {11, "resource deadlock would occur"},
    {35, "resource unavailable try again"},
    {34, "result out of range"},
    {107, "state not recoverable"},
    {1074, "stream timeout"},
    {26, "text file busy"},
    {60, "timed out"},
    {23, "too many files open in system"},
    {24, "too many files open"},
    {31, "too many links"},
    {62, "too many symbolic link levels"},
    {84, "value too large"},
    {41, "wrong protocol type"},
};

constexpr std::int32_t IostreamErrorStream = 1;
constexpr const char* IostreamErrorText = "iostream stream error";
constexpr const char* UnknownErrorText = "unknown error";

const char* FindSystemMessage(std::int32_t value) {
    for (const auto& entry : g_systemMessages)
        if (entry.value == value) return entry.text;
    return nullptr;
}

constexpr std::size_t FilebufConverterOffset = 0x80;
constexpr std::size_t FilebufMycharOffset = 0x88;
constexpr std::size_t FilebufStateOffset = 0x90;
constexpr std::size_t FilebufFileOffset = 0xa8;
constexpr std::size_t FilebufSavedFirstOffset = 0x70;
constexpr std::size_t FilebufSavedEndOffset = 0x78;
constexpr std::size_t OverflowSlot = 4;
constexpr std::size_t AlwaysNoconvSlot = 4;
constexpr std::size_t UnshiftSlot = 9;
constexpr int ConversionOk = 0;
constexpr int ConversionPartial = 1;
constexpr int ConversionNoconv = 3;
constexpr int EndOfFile = -1;
constexpr std::size_t UnshiftBufferStep = 8;

using FilebufOverflow = int (APS5_VABI*)(void* self, int character);
using ConverterAlwaysNoconv = bool (APS5_VABI*)(void* converter);
using ConverterUnshift = int (APS5_VABI*)(void* converter, void* state, char* to, char* toEnd, char** toNext);

template<typename Element> struct FilebufView {
    unsigned char* base;

    FilebufView(void* self, const char* function) : base(static_cast<unsigned char*>(self)) {
        if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    }

    template<typename T> T& At(std::size_t offset) const { return *reinterpret_cast<T*>(base + offset); }

    Element**& IgFirst() const { return At<Element**>(0x18); }
    Element**& IgNext() const { return At<Element**>(0x38); }
    Element**& IgEnd() const { return At<Element**>(0x58); }
    void*& Converter() const { return At<void*>(FilebufConverterOffset); }
    FileStream*& File() const { return At<FileStream*>(FilebufFileOffset); }
    Element& Mychar() const { return At<Element>(FilebufMycharOffset); }
    bool& Wrotesome() const { return At<bool>(FilebufMycharOffset + sizeof(Element)); }
    void* State() const { return base + FilebufStateOffset; }

    int Overflow(int character, const char* function) const {
        auto* vtable = At<void* const*>(0);
        if (vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null vtable");
        return reinterpret_cast<FilebufOverflow>(vtable[OverflowSlot])(base, character);
    }
};

void* const* ConverterSlots(void* converter, const char* function) {
    auto* vtable = *static_cast<void* const* const*>(converter);
    if (vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null converter vtable");
    return vtable;
}

template<typename Element> int FilebufSync(void* self, const char* function) {
    FilebufView<Element> buffer(self, function);
    if (buffer.File() == nullptr) return 0;
    if (buffer.Overflow(EndOfFile, function) == EndOfFile) return 0;
    return fflush_nid_postfix(buffer.File()) < 0 ? -1 : 0;
}

template<typename Element> int FilebufPbackfail(void* self, int character, const char* function) {
    FilebufView<Element> buffer(self, function);
    Element** const first = buffer.IgFirst();
    Element** const next = buffer.IgNext();
    Element** const end = buffer.IgEnd();
    if (first == nullptr || next == nullptr || end == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized get area");
    Element* position = *next;
    if (position != nullptr && *first < position) {
        if (character == EndOfFile) {
            *next = position - 1;
            return 0;
        }
        if (static_cast<std::make_unsigned_t<Element>>(position[-1]) == static_cast<std::uint32_t>(character)) {
            *next = position - 1;
            return character;
        }
    }
    if (character == EndOfFile || buffer.File() == nullptr) return EndOfFile;
    if (buffer.Converter() == nullptr) {
        int pushed;
        if constexpr (sizeof(Element) == 1) pushed = ungetc_nid_postfix(static_cast<unsigned char>(character), buffer.File());
        else pushed = ungetwc_nid_postfix(static_cast<std::uint16_t>(character), buffer.File());
        if (pushed != EndOfFile) return character;
        position = *next;
    }
    Element* const mychar = &buffer.Mychar();
    if (position == mychar) return EndOfFile;
    *mychar = static_cast<Element>(character);
    if (*first != mychar) {
        buffer.template At<Element*>(FilebufSavedFirstOffset) = *first;
        buffer.template At<Element*>(FilebufSavedEndOffset) = *end;
    }
    *first = mychar;
    *next = mychar;
    *end = mychar + 1;
    return character;
}

template<typename Element> bool FilebufEndwrite(void* self, const char* function) {
    FilebufView<Element> buffer(self, function);
    if (buffer.Converter() == nullptr || !buffer.Wrotesome()) return true;
    if (buffer.Overflow(EndOfFile, function) == EndOfFile) return false;
    std::vector<char> pending(UnshiftBufferStep, '\0');
    bool succeeded = true;
    while (true) {
        void* const converter = buffer.Converter();
        if constexpr (sizeof(Element) == 1) {
            if (reinterpret_cast<ConverterAlwaysNoconv>(ConverterSlots(converter, function)[AlwaysNoconvSlot])(converter)) break;
        }
        char* const begin = pending.data();
        char* next = nullptr;
        const int result = reinterpret_cast<ConverterUnshift>(ConverterSlots(converter, function)[UnshiftSlot])(converter, buffer.State(), begin, begin + pending.size(), &next);
        if (result == ConversionOk) buffer.Wrotesome() = false;
        else if (result != ConversionPartial) {
            if (result != ConversionNoconv) succeeded = false;
            break;
        }
        const std::size_t count = static_cast<std::size_t>(next - begin);
        if (count != 0) {
            if (fwrite_nid_postfix(begin, 1, count, buffer.File()) != count) {
                succeeded = false;
                break;
            }
            if (!buffer.Wrotesome()) break;
        } else {
            if (!buffer.Wrotesome()) break;
            pending.resize(pending.size() + UnshiftBufferStep, '\0');
        }
    }
    return succeeded;
}

}

extern "C" {

void APS5_VABI _ZNSt24_Iostream_error_categoryD1Ev_nid_postfix(LibcException::CategoryObject*) {
}

GuestLocale::String* APS5_VABI _ZNKSt24_Iostream_error_category7messageEi_nid_postfix(GuestLocale::String* result, const LibcException::CategoryObject*, std::int32_t value) {
    if (result == nullptr) throw std::invalid_argument("std::_Iostream_error_category::message: null result");
    if (value == IostreamErrorStream) return LibcAssignCString_nid_no_patch(result, IostreamErrorText);
    const char* text = FindSystemMessage(value);
    return LibcAssignCString_nid_no_patch(result, text != nullptr ? text : UnknownErrorText);
}

GuestLocale::String* APS5_VABI _ZNKSt22_System_error_category7messageEi_nid_postfix(GuestLocale::String* result, const LibcException::CategoryObject*, std::int32_t value) {
    if (result == nullptr) throw std::invalid_argument("std::_System_error_category::message: null result");
    const char* text = FindSystemMessage(value);
    return LibcAssignCString_nid_no_patch(result, text != nullptr ? text : UnknownErrorText);
}

LibcException::ErrorCode APS5_VABI _ZNKSt22_System_error_category23default_error_conditionEi_nid_postfix(const LibcException::CategoryObject*, std::int32_t value) {
    if (FindSystemMessage(value) != nullptr) return {0, value, _ZSt16generic_categoryv_nid_postfix()};
    return {0, value, _ZSt15system_categoryv_nid_postfix()};
}

[[noreturn]] void APS5_VABI _ZSt14_Debug_messagePKcS0_j_nid_postfix(const char* message, const char* file, std::uint32_t line) {
    fflush_nid_postfix(nullptr);
    fprintf_nid_postfix(&_Stderr_nid_postfix, "%s(%u) : error : %s\n", file, line, message);
    abort_nid_postfix(0, 0, 0, 0, 0, 0);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE4syncEv_nid_postfix(void* self) {
    return FilebufSync<char>(self, "std::basic_filebuf<char>::sync");
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE9pbackfailEi_nid_postfix(void* self, std::int32_t character) {
    return FilebufPbackfail<char>(self, character, "std::basic_filebuf<char>::pbackfail");
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE9pbackfailEi_nid_postfix(void* self, std::int32_t character) {
    return FilebufPbackfail<char16_t>(self, character, "std::basic_filebuf<wchar_t>::pbackfail");
}

bool APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE9_EndwriteEv_nid_postfix(void* self) {
    return FilebufEndwrite<char>(self, "std::basic_filebuf<char>::_Endwrite");
}

bool APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE9_EndwriteEv_nid_postfix(void* self) {
    return FilebufEndwrite<char16_t>(self, "std::basic_filebuf<wchar_t>::_Endwrite");
}

}
