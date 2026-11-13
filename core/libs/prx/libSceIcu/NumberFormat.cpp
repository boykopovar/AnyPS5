#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/NumberFormatBackend.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace Backend = IcuNumberBackend;
using Native = std::unique_ptr<void, void (*)(void*)>;
struct Entry {
    Native native;
    std::mutex mutex;
    explicit Entry(Native value) : native(std::move(value)) {}
};
struct Token { unsigned char value; };
std::mutex RegistryMutex;
std::unordered_map<const UNumberFormat*, std::shared_ptr<Entry>> Registry;
std::vector<std::unique_ptr<Token>> Tokens;
constexpr std::int32_t Unwritten = INT32_MIN;

template<class T>
T* Offset(T* pointer, std::size_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (count > (UINTPTR_MAX - address) / sizeof(T)) NotImplemented_nid_no_patch(__func__);
    return reinterpret_cast<T*>(address + count * sizeof(T));
}

std::int32_t ReadStatus(const std::int32_t* pointer) {
    std::int32_t value;
    if (!pointer || !GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch(__func__);
    return value;
}

void WriteStatus(std::int32_t* pointer, std::int32_t before, std::int32_t after) {
    if (before != after && !GuestCopyOut_nid_no_patch(pointer, &after, sizeof(after))) NotImplemented_nid_no_patch(__func__);
}

template<class T>
std::basic_string<T> ReadString(const T* pointer, std::int32_t length) {
    std::basic_string<T> result;
    if (!pointer) return result;
    if (length < 0) {
        for (std::size_t index = 0;; ++index) {
            T value;
            if (!GuestCopyIn_nid_no_patch(&value, Offset(pointer, index), sizeof(value))) NotImplemented_nid_no_patch(__func__);
            if (!value) return result;
            if (result.size() == INT32_MAX) NotImplemented_nid_no_patch(__func__);
            result.push_back(value);
        }
    }
    std::array<T, 256> block;
    while (result.size() < static_cast<std::size_t>(length)) {
        const auto count = std::min(block.size(), static_cast<std::size_t>(length) - result.size());
        if (!GuestCopyIn_nid_no_patch(block.data(), Offset(pointer, result.size()), count * sizeof(T))) NotImplemented_nid_no_patch(__func__);
        result.append(block.data(), count);
    }
    return result;
}

std::shared_ptr<Entry> Find(const UNumberFormat* token) {
    const std::lock_guard lock(RegistryMutex);
    const auto found = Registry.find(token);
    if (found == Registry.end()) NotImplemented_nid_no_patch(__func__);
    return found->second;
}

UNumberFormat* Publish(Native native) {
    if (!native) return nullptr;
    auto owner = std::make_shared<Entry>(std::move(native));
    auto storage = std::make_unique<Token>();
    auto* token = reinterpret_cast<UNumberFormat*>(storage.get());
    const std::lock_guard lock(RegistryMutex);
    Tokens.push_back(std::move(storage));
    Registry.emplace(token, std::move(owner));
    return token;
}

bool Overlap(const void* left, std::size_t leftSize, const void* right, std::size_t rightSize) {
    if (!leftSize || !rightSize) return false;
    const auto a = reinterpret_cast<std::uintptr_t>(left);
    const auto b = reinterpret_cast<std::uintptr_t>(right);
    if (leftSize > UINTPTR_MAX - a || rightSize > UINTPTR_MAX - b) NotImplemented_nid_no_patch(__func__);
    return a < b + rightSize && b < a + leftSize;
}

Backend::Field ReadField(UFieldPosition* pointer) {
    Backend::Field value{0, Unwritten, Unwritten};
    if (pointer && !GuestCopyIn_nid_no_patch(&value.field, pointer, sizeof(value.field))) NotImplemented_nid_no_patch(__func__);
    return value;
}

bool WritesField(const Backend::Field& value) {
    return value.begin != Unwritten || value.end != Unwritten;
}

void FieldAlias(UFieldPosition* pointer, const Backend::Field& value, std::int32_t* status) {
    if (!pointer || !WritesField(value)) return;
    if (value.begin == Unwritten || value.end == Unwritten ||
        Overlap(Offset(reinterpret_cast<unsigned char*>(pointer), 4), 8, status, 4)) NotImplemented_nid_no_patch(__func__);
}

void WriteField(UFieldPosition* pointer, const Backend::Field& value) {
    if (!pointer || !WritesField(value)) return;
    const std::array indices{value.begin, value.end};
    if (!GuestCopyOut_nid_no_patch(Offset(reinterpret_cast<unsigned char*>(pointer), 4), indices.data(), sizeof(indices))) NotImplemented_nid_no_patch(__func__);
}

enum class Operation { Double, Decimal, Symbol, Text };

bool EarlyError(Operation operation, std::int32_t error) {
    if (error == Icu::UnsupportedError) return operation == Operation::Symbol || operation == Operation::Text;
    if (error == 0x10110) return operation == Operation::Decimal;
    return error == 0x10111 && (operation == Operation::Double || operation == Operation::Decimal);
}

template<class Function>
std::int32_t Output(Operation operation, char16_t* output, std::int32_t capacity, UFieldPosition* position,
                    std::int32_t* status, std::int32_t before, Function&& function) {
    const auto initial = ReadField(position);
    auto field = initial;
    auto error = before;
    if (!output || capacity <= 0) {
        char16_t dummy = 0xa5a5;
        const auto count = function(output ? &dummy : nullptr, capacity, position ? &field : nullptr, &error);
        if (dummy != 0xa5a5) NotImplemented_nid_no_patch(__func__);
        FieldAlias(position, field, status);
        WriteField(position, field);
        WriteStatus(status, before, error);
        return count;
    }
    const auto required = function(nullptr, 0, position ? &field : nullptr, &error);
    if (error > 0 && error != Icu::BufferOverflow) {
        if (!EarlyError(operation, error)) NotImplemented_nid_no_patch(__func__);
        FieldAlias(position, field, status);
        WriteField(position, field);
        WriteStatus(status, before, error);
        return required;
    }
    if (required < 0) NotImplemented_nid_no_patch(__func__);
    const auto count = std::min<std::size_t>(static_cast<std::size_t>(capacity), static_cast<std::size_t>(required) + 1);
    if (count > INT32_MAX) NotImplemented_nid_no_patch(__func__);
    std::vector<char16_t> first(count, 0xa5a5);
    std::vector<char16_t> second(count, 0x5a5a);
    field = initial;
    auto secondField = initial;
    error = before;
    auto secondError = before;
    const auto length = function(first.data(), static_cast<std::int32_t>(count), position ? &field : nullptr, &error);
    const auto secondLength = function(second.data(), static_cast<std::int32_t>(count), position ? &secondField : nullptr, &secondError);
    if (length != secondLength || error != secondError || std::memcmp(&field, &secondField, sizeof(field))) NotImplemented_nid_no_patch(__func__);
    if (error > 0 && error != Icu::BufferOverflow && !EarlyError(operation, error)) NotImplemented_nid_no_patch(__func__);
    FieldAlias(position, field, status);
    for (std::size_t index = 0; index < count; ++index) {
        if (first[index] != second[index]) continue;
        const auto* address = Offset(output, index);
        if (Overlap(address, sizeof(char16_t), status, 4) ||
            (position && Overlap(address, sizeof(char16_t), position, 12))) NotImplemented_nid_no_patch(__func__);
    }
    WriteField(position, field);
    for (std::size_t start = 0; start < count;) {
        if (first[start] != second[start]) { ++start; continue; }
        auto end = start + 1;
        while (end < count && first[end] == second[end]) ++end;
        if (!GuestCopyOut_nid_no_patch(Offset(output, start), first.data() + start, (end - start) * sizeof(char16_t))) NotImplemented_nid_no_patch(__func__);
        start = end;
    }
    WriteStatus(status, before, error);
    return length;
}

} // namespace

extern "C" {

UNumberFormat* APS5_VABI unum_open_nid_postfix(std::int32_t style, const char16_t* pattern, std::int32_t patternLength,
                                              const char* locale, UParseError* parseError, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return nullptr;
    if (style < 0 || style >= 17) { WriteStatus(status, before, Icu::UnsupportedError); return nullptr; }
    if (!locale) NotImplemented_nid_no_patch(__func__);
    const auto name = ReadString(locale, -1);
    const bool patterned = style == 0 || style == 9;
    const auto text = patterned ? ReadString(pattern, patternLength) : std::u16string{};
    Backend::Parse parse;
    std::memset(&parse, 0xa5, sizeof(parse));
    const auto unchanged = parse;
    auto error = before;
    Native native(Backend::Open_nid_no_patch(style, patterned && pattern ? text.c_str() : nullptr,
        patterned && pattern ? static_cast<std::int32_t>(text.size()) : patternLength, name.c_str(),
        patterned && parseError ? &parse : nullptr, &error), Backend::Close_nid_no_patch);
    if (patterned && parseError && std::memcmp(&parse, &unchanged, sizeof(parse))) {
        if (Overlap(parseError, sizeof(parse), status, 4) || !GuestCopyOut_nid_no_patch(parseError, &parse, sizeof(parse))) NotImplemented_nid_no_patch(__func__);
    }
    WriteStatus(status, before, error);
    return Publish(std::move(native));
}

UNumberFormat* APS5_VABI unum_clone_nid_postfix(const UNumberFormat* format, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return nullptr;
    const auto owner = Find(format);
    const std::lock_guard lock(owner->mutex);
    auto error = before;
    Native native(Backend::Clone_nid_no_patch(owner->native.get(), &error), Backend::Close_nid_no_patch);
    WriteStatus(status, before, error);
    return Publish(std::move(native));
}

void APS5_VABI unum_close_nid_postfix(UNumberFormat* format) {
    if (!format) return;
    std::shared_ptr<Entry> closing;
    {
        const std::lock_guard lock(RegistryMutex);
        const auto found = Registry.find(format);
        if (found == Registry.end()) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second);
        Registry.erase(found);
    }
}

std::int32_t APS5_VABI unum_formatDouble_nid_postfix(const UNumberFormat* format, double number, char16_t* result,
                                                   std::int32_t capacity, UFieldPosition* position, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return -1;
    const auto owner = Find(format);
    const std::lock_guard lock(owner->mutex);
    return Output(Operation::Double, result, capacity, position, status, before,
        [&](char16_t* output, std::int32_t size, Backend::Field* field, std::int32_t* error) {
            return Backend::Double_nid_no_patch(owner->native.get(), number, output, size, field, error);
        });
}

std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const UNumberFormat* format, const char* number, std::int32_t length,
                                                    char16_t* result, std::int32_t capacity, UFieldPosition* position, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return -1;
    if ((!result && capacity != 0) || capacity < 0) { WriteStatus(status, before, Icu::IllegalArgument); return -1; }
    const auto owner = Find(format);
    if (!number && length != 0) NotImplemented_nid_no_patch(__func__);
    const auto text = ReadString(number, length);
    const std::lock_guard lock(owner->mutex);
    return Output(Operation::Decimal, result, capacity, position, status, before,
        [&](char16_t* output, std::int32_t size, Backend::Field* field, std::int32_t* error) {
            return Backend::Decimal_nid_no_patch(owner->native.get(), number ? text.c_str() : nullptr,
                static_cast<std::int32_t>(text.size()), output, size, field, error);
        });
}

std::int32_t APS5_VABI unum_getSymbol_nid_postfix(const UNumberFormat* format, std::int32_t symbol, char16_t* result,
                                               std::int32_t capacity, std::int32_t* status) {
    if (!status) return 0;
    const auto before = ReadStatus(status);
    if (before > 0) return 0;
    if (!format || symbol < 0 || symbol >= 28) { WriteStatus(status, before, Icu::IllegalArgument); return 0; }
    const auto owner = Find(format);
    const std::lock_guard lock(owner->mutex);
    return Output(Operation::Symbol, result, capacity, nullptr, status, before,
        [&](char16_t* output, std::int32_t size, Backend::Field*, std::int32_t* error) {
            return Backend::Symbol_nid_no_patch(owner->native.get(), symbol, output, size, error);
        });
}

std::int32_t APS5_VABI unum_getTextAttribute_nid_postfix(const UNumberFormat* format, std::int32_t attribute,
                                                      char16_t* result, std::int32_t capacity, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return -1;
    const auto owner = Find(format);
    const std::lock_guard lock(owner->mutex);
    return Output(Operation::Text, result, capacity, nullptr, status, before,
        [&](char16_t* output, std::int32_t size, Backend::Field*, std::int32_t* error) {
            return Backend::Text_nid_no_patch(owner->native.get(), attribute, output, size, error);
        });
}

void APS5_VABI unum_setAttribute_nid_postfix(UNumberFormat* format, std::int32_t attribute, std::int32_t value) {
    const auto owner = Find(format);
    const std::lock_guard lock(owner->mutex);
    Backend::SetAttribute_nid_no_patch(owner->native.get(), attribute, value);
}

void APS5_VABI unum_setTextAttribute_nid_postfix(UNumberFormat* format, std::int32_t attribute, const char16_t* value,
                                               std::int32_t length, std::int32_t* status) {
    const auto before = ReadStatus(status);
    if (before > 0) return;
    const auto owner = Find(format);
    const auto text = ReadString(value, length);
    const std::lock_guard lock(owner->mutex);
    auto error = before;
    Backend::SetText_nid_no_patch(owner->native.get(), attribute, value ? text.c_str() : nullptr,
        value ? static_cast<std::int32_t>(text.size()) : length, &error);
    WriteStatus(status, before, error);
}

}
