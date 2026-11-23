#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include "unicode/ucnv_cb.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using WriteChars = void (APS5_VABI*)(Icu::ConverterToUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t, std::int32_t*);
using WriteSub = void (APS5_VABI*)(Icu::ConverterToUnicodeArgs*, std::int32_t, std::int32_t*);

static_assert(std::is_same_v<decltype(&ucnv_cbToUWriteUChars_nid_postfix), WriteChars>);
static_assert(std::is_same_v<decltype(&ucnv_cbToUWriteSub_nid_postfix), WriteSub>);
const auto Chars = &ucnv_cbToUWriteUChars_nid_postfix;
const auto Substitute = &ucnv_cbToUWriteSub_nid_postfix;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU to-Unicode writers: %s\n", message);
    std::abort();
}

struct Release {
    void operator()(UConverter* converter) const { u_releaseDefaultConverter_nid_postfix(converter); }
};

using Converter = std::unique_ptr<UConverter, Release>;

Converter Open() {
    ucnv_setDefaultName_nid_postfix("UTF-8");
    std::int32_t status = 0;
    Converter converter(u_getDefaultConverter_nid_postfix(&status));
    Require(converter && status <= 0, "public default-converter fixture opens");
    return converter;
}

UConverterToUnicodeArgs Arguments(UConverter* converter, char16_t* target, const char16_t* limit, std::int32_t* offsets) {
    UConverterToUnicodeArgs args{};
    args.size = sizeof(args);
    args.converter = converter;
    args.target = target;
    args.targetLimit = limit;
    args.offsets = offsets;
    return args;
}

std::vector<char16_t> Drain(UConverter* converter) {
    std::array<char16_t, 64> output{};
    auto* target = output.data();
    const char empty = 0;
    const char* source = &empty;
    UErrorCode status = U_ZERO_ERROR;
    ucnv_toUnicode(converter, &target, output.data() + output.size(), &source, &empty, nullptr, true, &status);
    Require(U_SUCCESS(status) && source == &empty, "real converter drains buffered Unicode output");
    return {output.data(), target};
}

void NativeControl() {
    auto converter = Open();
    std::array<char16_t, 3> source{u'A', 0xd83d, 0xde42};
    std::array<char16_t, 4> output{0x5555, 0x5555, 0x5555, 0x5555};
    std::array<std::int32_t, 4> offsets{-55, -55, -55, -55};
    auto args = Arguments(converter.get(), output.data() + 1, output.data() + 2, offsets.data() + 1);
    UErrorCode status = U_ZERO_ERROR;
    ucnv_cbToUWriteUChars(&args, source.data(), source.size(), 37, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && args.target == output.data() + 2 && args.offsets == offsets.data() + 2,
            "native fixture partial cursors");
    Require(output[1] == u'A' && output[0] == 0x5555 && output[2] == 0x5555 && offsets[1] == 37 && offsets[2] == -55,
            "native fixture partial data and offset");
    Require(Drain(converter.get()) == std::vector<char16_t>({0xd83d, 0xde42}), "native fixture buffers remaining surrogate pair");
    args = Arguments(converter.get(), output.data() + 1, output.data() + 3, nullptr);
    status = U_ZERO_ERROR;
    ucnv_cbToUWriteSub(&args, 11, &status);
    Require(U_SUCCESS(status) && output[1] == 0xfffd && args.target == output.data() + 2, "native UTF8 substitution fixture");
}

void CharsCase() {
    auto converter = Open();
    std::array<char16_t, 3> source{u'A', 0xd83d, 0xde42};
    std::array<char16_t, 4> output{0x5555, 0x5555, 0x5555, 0x5555};
    std::array<std::int32_t, 4> offsets{-55, -55, -55, -55};
    auto args = Arguments(converter.get(), output.data() + 1, output.data() + 2, offsets.data() + 1);
    std::int32_t status = 0;
    Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), source.data(), source.size(), 37, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && args.target == output.data() + 2 && args.offsets == offsets.data() + 2,
            "public writer updates partial cursors");
    Require(output[1] == u'A' && output[0] == 0x5555 && output[2] == 0x5555 && offsets[1] == 37 && offsets[2] == -55,
            "public writer copies data and relative offsets within capacity");
    Require(Drain(converter.get()) == std::vector<char16_t>({0xd83d, 0xde42}), "public writer populates real converter overflow state");
}

void SubCase() {
    auto converter = Open();
    std::array<char16_t, 3> output{0x5555, 0x5555, 0x5555};
    std::array<std::int32_t, 3> offsets{-55, -55, -55};
    auto args = Arguments(converter.get(), output.data() + 1, output.data() + 1, offsets.data() + 1);
    std::int32_t status = 0;
    Substitute(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), -1, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && args.target == output.data() + 1 && args.offsets == offsets.data() + 1,
            "substitution with empty destination reports overflow without advancing");
    Require(std::all_of(output.begin(), output.end(), [](auto unit) { return unit == 0x5555; }) &&
            std::all_of(offsets.begin(), offsets.end(), [](auto offset) { return offset == -55; }), "empty substitution output preserves canaries");
    Require(Drain(converter.get()) == std::vector<char16_t>({0xfffd}), "public substitution is buffered in the real converter");
}

struct CallbackContext {
    bool guest;
    bool substitute;
    int calls = 0;
};

void U_CALLCONV Callback(const void* raw, UConverterToUnicodeArgs* args, const char*, std::int32_t,
    UConverterCallbackReason reason, UErrorCode* error) {
    if (reason > UCNV_IRREGULAR) return;
    auto& context = *static_cast<CallbackContext*>(const_cast<void*>(raw));
    ++context.calls;
    *error = U_ZERO_ERROR;
    constexpr std::array<char16_t, 4> replacement{u'[', 0xd83d, 0xde42, u']'};
    if (context.guest) {
        if (context.substitute)
            Substitute(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(args), 0, reinterpret_cast<std::int32_t*>(error));
        else
            Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(args), replacement.data(), replacement.size(), 0,
                reinterpret_cast<std::int32_t*>(error));
    } else if (context.substitute) {
        ucnv_cbToUWriteSub(args, 0, error);
    } else {
        ucnv_cbToUWriteUChars(args, replacement.data(), replacement.size(), 0, error);
    }
}

struct CallbackGuard {
    UConverter* converter;
    UConverterToUCallback previous;
    const void* previousContext;

    CallbackGuard(UConverter* value, CallbackContext* context) : converter(value) {
        UErrorCode status = U_ZERO_ERROR;
        ucnv_setToUCallBack(converter, Callback, context, &previous, &previousContext, &status);
        Require(U_SUCCESS(status), "install actual native converter callback fixture");
    }

    ~CallbackGuard() {
        UErrorCode status = U_ZERO_ERROR;
        ucnv_setToUCallBack(converter, previous, previousContext, nullptr, nullptr, &status);
    }
};

struct Converted {
    std::vector<char16_t> output;
    std::vector<std::int32_t> offsets;
    std::vector<std::int32_t> statuses;
    int callbacks;

    bool operator==(const Converted&) const = default;
};

Converted Convert(bool guest, bool substitute) {
    auto converter = Open();
    CallbackContext context{guest, substitute};
    CallbackGuard guard(converter.get(), &context);
    const std::array<char, 2> bytes{static_cast<char>(0xff), 'q'};
    const char* source = bytes.data();
    Converted result;
    for (int step = 0; step < 8; ++step) {
        std::array<char16_t, 4> output{0x5555, 0x5555, 0x5555, 0x5555};
        std::array<std::int32_t, 4> offsets{-55, -55, -55, -55};
        auto* target = output.data() + 1;
        UErrorCode status = U_ZERO_ERROR;
        ucnv_toUnicode(converter.get(), &target, output.data() + 2, &source, bytes.data() + bytes.size(),
            offsets.data() + 1, true, &status);
        const auto written = target - (output.data() + 1);
        Require(written >= 0 && written <= 1 && output[0] == 0x5555 && output[2] == 0x5555 &&
                offsets[0] == -55 && offsets[2] == -55, "real callback conversion keeps output bounded");
        result.output.insert(result.output.end(), output.data() + 1, target);
        result.offsets.insert(result.offsets.end(), offsets.data() + 1, offsets.data() + 1 + written);
        result.statuses.push_back(status);
        if (U_SUCCESS(status)) {
            Require(source == bytes.data() + bytes.size(), "actual callback conversion consumes input");
            result.callbacks = context.calls;
            return result;
        }
        Require(status == U_BUFFER_OVERFLOW_ERROR, "only output overflow interrupts callback conversion");
    }
    Require(false, "callback conversion terminates");
    return {};
}

void CallbackCase() {
    for (const bool substitute : {false, true}) {
        const auto expected = Convert(false, substitute);
        Require(expected.callbacks == 1 && !expected.output.empty() && expected.output.back() == u'q',
                "native fixture invokes callback and resumes ASCII conversion");
        Require(Convert(true, substitute) == expected, "guest public writers match native callback output, offsets, statuses and buffering");
    }
}

void Compare(bool substitute, std::int32_t length, std::int32_t capacity, bool withOffsets,
    std::int32_t incoming, std::int32_t offsetIndex) {
    auto expectedConverter = Open();
    auto actualConverter = Open();
    std::vector<char16_t> source(static_cast<std::size_t>(std::max(length, 1)));
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<char16_t>((index * 997u) & 0xffffu);
    std::vector<char16_t> expected(static_cast<std::size_t>(capacity) + 2, 0x5555);
    auto actual = expected;
    std::vector<std::int32_t> expectedOffsets(expected.size(), 0x12345678);
    auto actualOffsets = expectedOffsets;
    auto native = Arguments(expectedConverter.get(), expected.data() + 1, expected.data() + 1 + capacity,
        withOffsets ? expectedOffsets.data() + 1 : nullptr);
    auto guest = Arguments(actualConverter.get(), actual.data() + 1, actual.data() + 1 + capacity,
        withOffsets ? actualOffsets.data() + 1 : nullptr);
    const char unused[] = "xy";
    native.size = guest.size = 7;
    native.flush = guest.flush = -7;
    native.source = guest.source = unused;
    native.sourceLimit = guest.sourceLimit = unused + 2;
    UErrorCode expectedStatus = static_cast<UErrorCode>(incoming);
    std::int32_t actualStatus = incoming;
    if (substitute) {
        ucnv_cbToUWriteSub(&native, offsetIndex, &expectedStatus);
        Substitute(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&guest), offsetIndex, &actualStatus);
    } else {
        ucnv_cbToUWriteUChars(&native, source.data(), length, offsetIndex, &expectedStatus);
        Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&guest), source.data(), length, offsetIndex, &actualStatus);
    }
    Require(actualStatus == expectedStatus && actual == expected && actualOffsets == expectedOffsets,
            "native output, offset and status matrix parity");
    Require(guest.target - (actual.data() + 1) == native.target - (expected.data() + 1), "target cursor matrix parity");
    Require(withOffsets ? guest.offsets - (actualOffsets.data() + 1) == native.offsets - (expectedOffsets.data() + 1) : !guest.offsets,
            "offset cursor matrix parity");
    Require(guest.size == 7 && guest.flush == -7 && guest.source == unused && guest.sourceLimit == unused + 2 &&
            guest.converter == actualConverter.get() && guest.targetLimit == actual.data() + 1 + capacity,
            "unrelated callback argument fields remain unchanged");
    Require(Drain(actualConverter.get()) == Drain(expectedConverter.get()), "buffered converter state matrix parity");
}

void MatrixCase() {
    for (const int length : {-1, 0, 1, 2, 5, 31, 32, 33, 80, 9000}) {
        for (const int capacity : {0, 1, std::max(length - 1, 0), std::max(length, 0), std::max(length + 1, 0)}) {
            if (length - capacity > 32) continue;
            for (const bool offsets : {false, true}) {
                for (const int incoming : {-128, 0, 1}) Compare(false, length, capacity, offsets, incoming, -17);
            }
        }
    }
    for (const int capacity : {0, 1, 2})
        for (const bool offsets : {false, true})
            for (const int incoming : {-128, 0, 1}) Compare(true, 1, capacity, offsets, incoming, INT32_MAX);
}

void OverlapCase() {
    for (const int shift : {-2, -1, 0, 1, 2}) {
        for (const int capacity : {1, 3, 6}) {
            auto expectedConverter = Open();
            auto actualConverter = Open();
            std::array<char16_t, 20> expected{};
            for (std::size_t index = 0; index < expected.size(); ++index) expected[index] = static_cast<char16_t>(u'A' + index);
            auto actual = expected;
            auto* source = expected.data() + 6;
            auto native = Arguments(expectedConverter.get(), source + shift, source + shift + capacity, nullptr);
            auto guest = Arguments(actualConverter.get(), actual.data() + 6 + shift, actual.data() + 6 + shift + capacity, nullptr);
            UErrorCode expectedStatus = U_ZERO_ERROR;
            std::int32_t status = 0;
            ucnv_cbToUWriteUChars(&native, source, 6, 11, &expectedStatus);
            Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&guest), actual.data() + 6, 6, 11, &status);
            Require(expectedStatus == status && actual == expected, "overlap follows native forward-copy order");
            Require(Drain(actualConverter.get()) == Drain(expectedConverter.get()), "overlap preserves native buffered remainder");
        }
    }
}

struct Guarded {
    char* memory;
    std::size_t page;

    Guarded() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "allocate guarded pages");
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0, "protect guard page");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = static_cast<char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED, "allocate guarded pages");
        Require(mprotect(memory + page, page, PROT_NONE) == 0, "protect guard page");
#endif
    }

    ~Guarded() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0, "make first page read-only");
#else
        Require(mprotect(memory, page, PROT_READ) == 0, "make first page read-only");
#endif
    }

    void* Forbidden() const { return memory + page; }
};

template<class Call>
void Unreadable(Call call) {
    errno = 73;
    try {
        call();
    } catch (const std::runtime_error&) {
        Require(errno == 73, "refused guest memory preserves errno");
        return;
    }
    Require(false, "unusable guest memory is refused");
}

void GuardsCase() {
    Guarded guard;
    auto converter = Open();
    auto* output = reinterpret_cast<char16_t*>(guard.memory + guard.page) - 1;
    auto args = Arguments(converter.get(), output, output + 1, nullptr);
    std::array<char16_t, 33> source{};
    source.fill(u'X');
    std::int32_t status = 0;
    Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), source.data(), 33, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && *output == u'X' && args.target == output + 1,
            "one guarded output unit and exactly32 buffered units");
    Require(Drain(converter.get()) == std::vector<char16_t>(32, u'X'), "actual native overflow-buffer boundary drains");
    args = Arguments(converter.get(), output, output, nullptr);
    status = 0;
    Unreadable([&] { Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), source.data(), 33, 0, &status); });
    Require(args.target == output && status == 0 && Drain(converter.get()).empty(), "oversized native buffered remainder fails closed before state mutation");
    for (int protectedField = 0; protectedField < 4; ++protectedField) {
        Guarded page;
        char16_t first = u'?';
        std::int32_t offset = -1;
        auto* text = protectedField == 0 ? reinterpret_cast<char16_t*>(page.memory) : &first;
        auto* offsets = protectedField == 1 ? reinterpret_cast<std::int32_t*>(page.memory) : &offset;
        auto writable = Arguments(converter.get(), text, text + 1, offsets);
        auto* record = protectedField == 2 ? reinterpret_cast<UConverterToUnicodeArgs*>(page.memory) : &writable;
        auto* error = protectedField == 3 ? reinterpret_cast<std::int32_t*>(page.memory) : &status;
        *text = u'?';
        *offsets = -1;
        *record = writable;
        *error = 0;
        page.ReadOnly();
        auto* guest = reinterpret_cast<Icu::ConverterToUnicodeArgs*>(record);
        Unreadable([&] { Chars(guest, u"abc", 3, 4, error); });
        Require(Drain(converter.get()).empty(), "refused write leaves no suffix in the converter");
        Require(*text == u'?' && *offsets == -1 && record->target == text && record->offsets == offsets && *error == 0,
                "refused write preserves output and cursors for retry");
        writable = Arguments(converter.get(), &first, &first + 1, &offset);
        status = 0;
        Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&writable), u"abc", 3, 4, &status);
        Require(first == u'a' && offset == 4 && status == U_BUFFER_OVERFLOW_ERROR &&
                Drain(converter.get()) == std::vector<char16_t>{u'b', u'c'}, "retry buffers and drains just one suffix");
    }
    Guarded readOnlyStatus;
    auto* initialStatus = reinterpret_cast<std::int32_t*>(readOnlyStatus.memory);
    *initialStatus = U_USING_DEFAULT_WARNING;
    readOnlyStatus.ReadOnly();
    args = Arguments(converter.get(), output, output + 1, nullptr);
    Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), u"Q", 1, 0, initialStatus);
    Require(*output == u'Q' && *initialStatus == U_USING_DEFAULT_WARNING, "full write preserves a read-only warning");
    auto* forbiddenArgs = static_cast<Icu::ConverterToUnicodeArgs*>(guard.Forbidden());
    auto* forbiddenSource = static_cast<const char16_t*>(guard.Forbidden());
    auto* forbiddenStatus = static_cast<std::int32_t*>(guard.Forbidden());
    status = INT32_MAX;
    Chars(forbiddenArgs, forbiddenSource, INT32_MAX, 0, &status);
    Substitute(forbiddenArgs, 0, &status);
    Require(status == INT32_MAX, "incoming failure avoids unrelated guest memory");
    status = 0;
    Unreadable([&] { Chars(forbiddenArgs, source.data(), 1, 0, &status); });
    Unreadable([&] { Substitute(forbiddenArgs, 0, &status); });
    args = Arguments(converter.get(), output, output + 1, nullptr);
    Unreadable([&] { Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), forbiddenSource, 1, 0, &status); });
    Unreadable([&] { Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), source.data(), 1, 0, forbiddenStatus); });
    args = Arguments(converter.get(), static_cast<char16_t*>(guard.Forbidden()), static_cast<char16_t*>(guard.Forbidden()) + 1, nullptr);
    Unreadable([&] { Substitute(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), 0, &status); });
    args = Arguments(nullptr, output, output, nullptr);
    status = 0;
    Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), forbiddenSource, INT32_MAX, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && args.target == output, "discarded null-converter overflow reads no unused source");
    args = Arguments(converter.get(), output, output + 1, nullptr);
    status = INT32_MIN;
    Chars(reinterpret_cast<Icu::ConverterToUnicodeArgs*>(&args), source.data(), 1, INT32_MIN, &status);
    Require(status == INT32_MIN && *output == u'X', "out-of-range negative warning bits are preserved on success");
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "chars";
        if (mode == "control") NativeControl();
        else if (mode == "chars") CharsCase();
        else if (mode == "sub") SubCase();
        else if (mode == "callback") CallbackCase();
        else if (mode == "matrix") MatrixCase();
        else if (mode == "overlap") OverlapCase();
        else if (mode == "guards") GuardsCase();
        else return 2;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ICU to-Unicode writer runtime failure: %s\n", error.what());
        return 1;
    }
}
