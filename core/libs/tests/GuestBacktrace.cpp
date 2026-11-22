#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#include <filesystem>
#else
#include <dlfcn.h>
#endif
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <vector>

extern "C" {

#if defined(_WIN32)
std::string BacktraceModulePath_nid_no_patch(HMODULE, std::size_t);
#endif
}

namespace {

constexpr std::size_t NameOffset = 0x10;
constexpr unsigned char Fill = 0xcc;

struct Record {
    const std::byte* next;
    std::uint32_t offset;
    std::uint32_t reserved;
    std::string module;
    std::size_t bytes;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest backtrace: %s\n", message);
    std::abort();
}

std::vector<Record> Parse(const std::vector<unsigned char>& buffer, std::size_t used, int count) {
    std::vector<Record> records;
    const auto* base = reinterpret_cast<const std::byte*>(buffer.data());
    const auto* cursor = count == 0 ? nullptr : base;
    while (cursor != nullptr) {
        Require(cursor >= base && static_cast<std::size_t>(cursor - base) + NameOffset < used, "record outside the written bytes");
        Require(reinterpret_cast<std::uintptr_t>(cursor) % 8 == 0, "record is not 8-byte aligned");
        Record record{};
        std::memcpy(&record.next, cursor, sizeof(record.next));
        std::memcpy(&record.offset, cursor + 8, sizeof(record.offset));
        std::memcpy(&record.reserved, cursor + 12, sizeof(record.reserved));
        const auto* name = reinterpret_cast<const char*>(cursor + NameOffset);
        const auto limit = used - static_cast<std::size_t>(cursor - base) - NameOffset;
        const auto length = strnlen(name, limit);
        Require(length < limit, "module name is not terminated inside the record");
        record.module.assign(name, length);
        record.bytes = (NameOffset + length + 1 + 7) & ~std::size_t{7};
        Require(static_cast<std::size_t>(cursor - base) + record.bytes <= used, "record runs past the written bytes");
        for (std::size_t index = NameOffset + length + 1; index < record.bytes; ++index) Require(cursor[index] == std::byte{0}, "record padding is not zeroed");
        Require(record.next == nullptr || record.next == cursor + record.bytes, "next does not point to the following record");
        records.push_back(record);
        cursor = record.next;
    }
    Require(static_cast<int>(records.size()) == count, "record list length differs from the reported count");
    return records;
}

std::size_t Used(const std::vector<Record>& records) {
    std::size_t total = 0;
    for (const auto& record : records) total += record.bytes;
    return total;
}

[[gnu::noinline]] std::uintptr_t CurrentIp() {
    asm volatile("" ::: "memory");
    return reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
}

}

[[gnu::noinline]] void CheckBacktrace() {
#if defined(_WIN32)
    HMODULE handle = nullptr;
    Require(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&CheckBacktrace), &handle) != 0 && handle != nullptr,
            "GetModuleHandleExA of the test failed");
    std::string executable = BacktraceModulePath_nid_no_patch(handle, 1);
    const auto slash = executable.find_last_of("/\\");
    Require(!executable.empty() && slash != std::string::npos, "BacktraceModulePath of the test failed");
    executable.erase(0, slash + 1);
#else
    Dl_info self{};
    Require(dladdr(reinterpret_cast<void*>(&CheckBacktrace), &self) != 0 && self.dli_fname != nullptr && self.dli_saddr != nullptr, "dladdr of the test failed");
    std::string executable = self.dli_fname;
    executable.erase(0, executable.find_last_of('/') + 1);
#endif

    int count = -1;
    std::size_t size = 0;
    Require(sceLibcBacktraceGetBufferSize_nid_postfix(-1, &count, &size) == 0, "GetBufferSize failed");
    Require(count > 1 && size >= static_cast<std::size_t>(count) * NameOffset, "GetBufferSize reported too little");

    std::vector<unsigned char> buffer(size + 64, Fill);
    int written = -1;
    const auto before = CurrentIp();
    const int selfStatus = sceLibcBacktraceSelf_nid_postfix(-1, buffer.data(), size, &written);
    const auto after = CurrentIp();
    Require(selfStatus == 0, "Self failed");
    Require(written == count, "Self wrote a different count than GetBufferSize reported");
    const auto records = Parse(buffer, size, written);
    Require(Used(records) == size, "Self used a different size than GetBufferSize reported");
    for (std::size_t index = size; index < buffer.size(); ++index) Require(buffer[index] == Fill, "Self wrote past the buffer size");
    for (const auto& record : records) Require(record.reserved == 0 && !record.module.empty(), "record has a nonzero reserved word or no module");
    Require(records.front().module == executable, "first frame is not in the calling module");
#if defined(_WIN32)
    const auto address = reinterpret_cast<std::uintptr_t>(handle) + records.front().offset;
    const auto callerReturn = reinterpret_cast<std::uintptr_t>(handle) + records[1].offset;
    Require(address > before && address < after, "first frame is not the caller");
    Require(callerReturn == reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), "second frame is not the caller of CheckBacktrace");
#else
    Dl_info caller{};
    const auto address = reinterpret_cast<std::uintptr_t>(self.dli_fbase) + records.front().offset;
    Require(dladdr(reinterpret_cast<void*>(address), &caller) != 0 && caller.dli_saddr == self.dli_saddr, "first frame is not the caller");
#endif

    for (const int limit : {0, 1, 2}) {
        Require(sceLibcBacktraceGetBufferSize_nid_postfix(limit, &count, &size) == 0 && count == limit, "GetBufferSize ignored the frame limit");
        std::vector<unsigned char> limited(size + 64, Fill);
        Require(sceLibcBacktraceSelf_nid_postfix(limit, limited.data(), limited.size(), &written) == 0 && written == limit, "Self ignored the frame limit");
        const auto limitedRecords = Parse(limited, limited.size(), written);
        Require(Used(limitedRecords) == size, "limited Self used a different size than GetBufferSize reported");
        for (int index = 0; index < limit; ++index) Require(limitedRecords[index].module == records[index].module, "limited frames differ from the full trace");
    }

    const auto first = records.front().bytes;
    for (const std::size_t available : {std::size_t{0}, first - 1, first, first + records[1].bytes - 1, first + records[1].bytes}) {
        std::vector<unsigned char> small(available + 64, Fill);
        written = -1;
        Require(sceLibcBacktraceSelf_nid_postfix(-1, small.data(), available, &written) == 0, "Self with a small buffer failed");
        const int expected = available < first ? 0 : available < first + records[1].bytes ? 1 : 2;
        Require(written == expected, "Self with a small buffer wrote the wrong number of records");
        const auto smallRecords = Parse(small, available, written);
        Require(written == 0 || smallRecords.back().next == nullptr, "last record is not terminated");
        for (std::size_t index = Used(smallRecords); index < small.size(); ++index) Require(small[index] == Fill, "Self wrote past the last record that fits");
    }
}

void CheckUnreadableOutputs() {
    auto* invalidCount = reinterpret_cast<int*>(std::uintptr_t{0x10});
    auto* invalidSize = reinterpret_cast<std::size_t*>(std::uintptr_t{0x10});
    auto* invalidBuffer = reinterpret_cast<void*>(std::uintptr_t{0x10});
    int count = -1;
    std::size_t size = 0;
    bool rejected = false;
    try {
        sceLibcBacktraceGetBufferSize_nid_postfix(1, invalidCount, &size);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "GetBufferSize accepted an unreadable count output");
    rejected = false;
    try {
        sceLibcBacktraceGetBufferSize_nid_postfix(1, &count, invalidSize);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "GetBufferSize accepted an unreadable size output");
    rejected = false;
    try {
        sceLibcBacktraceSelf_nid_postfix(1, invalidBuffer, 4096, &count);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "Self accepted an unreadable frame output");
    std::vector<unsigned char> buffer(4096, Fill);
    rejected = false;
    try {
        sceLibcBacktraceSelf_nid_postfix(1, buffer.data(), buffer.size(), invalidCount);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "Self accepted an unreadable count output");
}

#if defined(_WIN32)
void CheckUnicodeModulePath() {
    std::wstring source(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, source.data(), static_cast<DWORD>(source.size()));
    Require(length != 0 && length < source.size(), "cannot locate test executable");
    const auto directory = std::filesystem::temp_directory_path();
    wchar_t name[MAX_PATH]{};
    Require(GetTempFileNameW(directory.c_str(), L"\u96EA", 0, name) != 0, "cannot create temporary module name");
    const bool copied = CopyFileW(source.c_str(), name, FALSE) != 0;
    const auto module = copied ? LoadLibraryExW(name, nullptr, DONT_RESOLVE_DLL_REFERENCES) : nullptr;
    const auto actual = module ? BacktraceModulePath_nid_no_patch(module, 1) : std::string{};
    if (module) FreeLibrary(module);
    DeleteFileW(name);
    Require(module != nullptr, "cannot load temporary module");
    const auto expected = std::filesystem::path(name).filename().u8string();
    Require(actual.substr(actual.find_last_of("/\\") + 1) == std::string(expected.begin(), expected.end()),
            "backtrace module name does not preserve Unicode");
}
#endif

void CheckNullArguments() {
    int count = -1;
    std::size_t size = 0;
    std::vector<unsigned char> buffer(4096, Fill);
    Require(sceLibcBacktraceGetBufferSize_nid_postfix(1, nullptr, &size) == 1, "GetBufferSize accepted a null count");
    Require(sceLibcBacktraceGetBufferSize_nid_postfix(1, &count, nullptr) == 1, "GetBufferSize accepted a null size");
    Require(sceLibcBacktraceSelf_nid_postfix(1, nullptr, buffer.size(), &count) == 1, "Self accepted a null buffer");
    Require(sceLibcBacktraceSelf_nid_postfix(1, buffer.data(), buffer.size(), nullptr) == 1, "Self accepted a null count");
    Require(count == -1 && size == 0, "a rejected call wrote an output");
    for (const auto byte : buffer) Require(byte == Fill, "a rejected call wrote the buffer");
}

void CheckPersonalityEdgeCases() {
    constexpr std::uint64_t foreignClass = 0;
    constexpr std::uintptr_t region = 0x1000;
    constexpr std::uintptr_t ip = 0x1008;

    const unsigned char missingLsda[] = {
        255, 255, 1, 4,
        0x10, 4, 0x20, 0
    };
    _Unwind_Exception exception{};
    _Unwind_Context missingContext{nullptr};
    missingContext.registers[16] = ip;
    missingContext.region = region;
    missingContext.lsda = reinterpret_cast<std::uintptr_t>(missingLsda);
    Require(
        __gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, foreignClass, &exception, &missingContext) == _URC_HANDLER_FOUND,
        "missing call-site entry did not stop search phase at noexcept boundary"
    );
    Require(
        __gxx_personality_v0_nid_postfix(
            1, static_cast<_Unwind_Action>(_UA_CLEANUP_PHASE | _UA_FORCE_UNWIND), foreignClass, &exception, &missingContext
        ) == _URC_CONTINUE_UNWIND,
        "missing call-site entry did not continue during force unwind"
    );

    const unsigned char emptySpecLsda[] = {
        255, 0, 8, 1, 4,
        0, 0x10, 0x24, 1,
        0x7f, 0,
        0
    };
    _Unwind_Context emptySearchContext{nullptr};
    emptySearchContext.registers[16] = ip;
    emptySearchContext.region = region;
    emptySearchContext.lsda = reinterpret_cast<std::uintptr_t>(emptySpecLsda);
    Require(
        __gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, foreignClass, &exception, &emptySearchContext) == _URC_HANDLER_FOUND,
        "empty exception specification did not match in search phase"
    );
    _Unwind_Context emptyHandlerContext{nullptr};
    emptyHandlerContext.registers[16] = ip;
    emptyHandlerContext.region = region;
    emptyHandlerContext.lsda = reinterpret_cast<std::uintptr_t>(emptySpecLsda);
    Require(
        __gxx_personality_v0_nid_postfix(
            1, static_cast<_Unwind_Action>(_UA_CLEANUP_PHASE | _UA_HANDLER_FRAME), foreignClass, &exception, &emptyHandlerContext
        ) == _URC_INSTALL_CONTEXT,
        "empty exception specification did not install handler context"
    );
    Require(emptyHandlerContext.registers[0] == reinterpret_cast<std::uintptr_t>(&exception), "empty spec wrote wrong exception pointer");
    Require(
        emptyHandlerContext.registers[1] == static_cast<std::uintptr_t>(std::intptr_t{-1}),
        "empty spec wrote wrong selector register"
    );
    Require(emptyHandlerContext.registers[16] == region + 0x24, "empty spec wrote wrong landing pad IP");

    const unsigned char chainedCleanupLsda[] = {
        255, 0, 10, 1, 4,
        0, 0x10, 0x30, 1,
        0x7f, 1,
        0, 0,
        0
    };
    _Unwind_Context forceContext{nullptr};
    forceContext.registers[16] = ip;
    forceContext.region = region;
    forceContext.lsda = reinterpret_cast<std::uintptr_t>(chainedCleanupLsda);
    forceContext.registers[1] = 0x55;
    Require(
        __gxx_personality_v0_nid_postfix(
            1, static_cast<_Unwind_Action>(_UA_CLEANUP_PHASE | _UA_FORCE_UNWIND), foreignClass, &exception, &forceContext
        ) == _URC_INSTALL_CONTEXT,
        "force unwind over exception spec did not install chained cleanup"
    );
    Require(forceContext.registers[0] == reinterpret_cast<std::uintptr_t>(&exception), "force unwind cleanup wrote wrong exception pointer");
    Require(forceContext.registers[1] == 0, "force unwind cleanup did not clear selector register");
    Require(forceContext.registers[16] == region + 0x30, "force unwind cleanup wrote wrong landing pad IP");

    std::vector<unsigned char> typedSpecLsda = {
        255, 0, static_cast<unsigned char>(8 + sizeof(std::uintptr_t)), 1, 4,
        0, 0x10, 0x40, 1,
        0x7f, 0
    };
    const std::size_t typeSlotOffset = typedSpecLsda.size();
    typedSpecLsda.resize(typeSlotOffset + sizeof(std::uintptr_t) + 2, 0);
    typedSpecLsda[typeSlotOffset + sizeof(std::uintptr_t)] = 1;
    typedSpecLsda[typeSlotOffset + sizeof(std::uintptr_t) + 1] = 0;

    const std::uintptr_t intType = reinterpret_cast<std::uintptr_t>(&typeid(int));
    std::memcpy(typedSpecLsda.data() + typeSlotOffset, &intType, sizeof(intType));
    _Unwind_Context typedContext{nullptr};
    typedContext.registers[16] = ip;
    typedContext.region = region;
    typedContext.lsda = reinterpret_cast<std::uintptr_t>(typedSpecLsda.data());
    Require(
        __gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, foreignClass, &exception, &typedContext) == _URC_HANDLER_FOUND,
        "non-null exception spec allowed a foreign exception"
    );

    const std::uintptr_t nullType = 0;
    std::memcpy(typedSpecLsda.data() + typeSlotOffset, &nullType, sizeof(nullType));
    Require(
        __gxx_personality_v0_nid_postfix(1, _UA_SEARCH_PHASE, foreignClass, &exception, &typedContext) == _URC_CONTINUE_UNWIND,
        "null exception spec entry did not allow a foreign exception"
    );
}

int main() {
#if defined(_WIN32)
    CheckUnicodeModulePath();
#endif
    CheckNullArguments();
    CheckBacktrace();
    CheckUnreadableOutputs();
    CheckPersonalityEdgeCases();
}
