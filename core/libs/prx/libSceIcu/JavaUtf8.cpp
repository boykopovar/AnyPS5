#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool Continuation(std::uint8_t byte) {
    return byte >= 0x80 && byte <= 0xbf;
}

bool ThreeByteSecond(std::uint8_t lead, std::uint8_t byte) {
    if (lead == 0xe0) return byte >= 0xa0 && byte <= 0xbf;
    if (lead == 0xed) return byte >= 0x80 && byte <= 0x9f;
    return Continuation(byte);
}

bool FourByteSecond(std::uint8_t lead, std::uint8_t byte) {
    if (lead == 0xf0) return byte >= 0x90 && byte <= 0xbf;
    if (lead == 0xf4) return byte >= 0x80 && byte <= 0x8f;
    return lead >= 0xf1 && lead <= 0xf3 && Continuation(byte);
}

std::int64_t Skipped(const std::uint8_t* source, std::int64_t lead, std::int64_t length) {
    const auto first = source[lead];
    const auto at = [&](std::int64_t offset) { return lead + offset < length ? source[lead + offset] : 0; };
    if (first >= 0xf0 && first <= 0xf4) {
        if (lead + 1 >= length || !FourByteSecond(first, source[lead + 1])) return 1;
        if (!Continuation(at(2))) return 2;
        if (!Continuation(at(3))) return 3;
        return 4;
    }
    if (first >= 0xe0 && first <= 0xef) {
        if (lead + 1 >= length || !ThreeByteSecond(first, source[lead + 1])) return 1;
        return 2;
    }
    return 1;
}

int Encoded(char16_t unit) {
    return unit != 0 && unit <= 0x7f ? 1 : unit <= 0x7ff ? 2 : 3;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller, const char* what) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unwritable " + what);
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    if (!CopyIn(&value, status, sizeof(value))) throw std::invalid_argument(std::string(caller) + ": unreadable status");
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing != incoming) CopyOut(status, &outgoing, sizeof(outgoing), caller, "status");
}

template<class T>
std::vector<T> CopySource(const T* source, std::int32_t length, const char* caller) {
    constexpr std::uintptr_t Chunk = 4096;
    const bool counted = length >= 0;
    const std::size_t limit = counted ? static_cast<std::size_t>(length) * sizeof(T) : 0;
    std::vector<unsigned char> bytes;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    std::size_t scanned = 0;
    bool terminated = false;
    while (counted ? bytes.size() < limit : !terminated) {
        std::size_t count = Chunk - address % Chunk;
        if (counted) count = std::min(count, limit - bytes.size());
        const std::size_t offset = bytes.size();
        bytes.resize(offset + count);
        if (!CopyIn(bytes.data() + offset, reinterpret_cast<const void*>(address), count))
            throw std::invalid_argument(std::string(caller) + ": unreadable source");
        address += count;
        if (counted) continue;
        for (; scanned + sizeof(T) <= bytes.size(); scanned += sizeof(T)) {
            T unit {};
            std::memcpy(&unit, bytes.data() + scanned, sizeof(T));
            if (unit == 0) {
                bytes.resize(scanned);
                terminated = true;
                break;
            }
        }
    }
    std::vector<T> units(bytes.size() / sizeof(T));
    if (!bytes.empty()) std::memcpy(units.data(), bytes.data(), bytes.size());
    return units;
}

template<class T>
std::int32_t Terminate(std::vector<T>& output, std::int32_t capacity, std::int32_t length, std::int32_t status) {
    if (status > 0 || length < 0) return status;
    if (length < capacity) {
        output.push_back(0);
        return status == Icu::NotTerminated ? 0 : status;
    }
    return length == capacity ? Icu::NotTerminated : Icu::BufferOverflow;
}

template<class T>
void CopyOutput(T* destination, const std::vector<T>& output, const char* caller) {
    if (!output.empty()) CopyOut(destination, output.data(), output.size() * sizeof(T), caller, "destination");
}

}

extern "C" {

char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                                 const char* text, std::int32_t sourceLength, std::int32_t substitution,
                                                                 std::int32_t* substitutions, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if ((!text && sourceLength != 0) || sourceLength < -1 || (!destination && capacity != 0) || capacity < 0 ||
        substitution > 0x10ffff || Icu::Surrogate(substitution)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    const std::int32_t noSubstitutions = 0;
    if (substitutions) CopyOut(substitutions, &noSubstitutions, sizeof(noSubstitutions), __func__, "substitution count");
    const auto input = CopySource(reinterpret_cast<const std::uint8_t*>(text), sourceLength, __func__);
    const auto* source = input.data();
    const auto length = static_cast<std::int64_t>(input.size());
    std::vector<char16_t> output;
    output.reserve(static_cast<std::size_t>(std::min<std::int64_t>(capacity, 2 * length + 1)));
    std::int64_t extra = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length;) {
        const auto written = static_cast<std::int64_t>(output.size());
        const auto lead = source[index];
        const bool writing = written < capacity;
        char16_t unit = lead;
        std::int64_t used = 1;
        bool valid = true;
        if (lead >= 0xc0 && lead <= 0xdf && index + 1 < length && Continuation(source[index + 1])) {
            unit = static_cast<char16_t>(((lead & 0x1f) << 6) | (source[index + 1] & 0x3f));
            used = 2;
        } else if (lead >= 0xe0 && lead <= 0xef && index + 2 < length && Continuation(source[index + 1]) && Continuation(source[index + 2])) {
            unit = static_cast<char16_t>(((lead & 0x0f) << 12) | ((source[index + 1] & 0x3f) << 6) | (source[index + 2] & 0x3f));
            used = 3;
        } else if (lead >= 0x80) {
            valid = false;
        }
        if (valid) {
            index += used;
            if (writing) output.push_back(unit);
            else ++extra;
            continue;
        }
        if (substitution < 0) {
            CopyOutput(destination, output, __func__);
            WriteStatus(status, incoming, Icu::InvalidCharacter, __func__);
            return nullptr;
        }
        index += Skipped(source, index, length);
        ++replaced;
        if (!writing) {
            ++extra;
        } else if (substitution <= 0xffff) {
            output.push_back(static_cast<char16_t>(substitution));
        } else {
            output.push_back(static_cast<char16_t>(0xd7c0 + (substitution >> 10)));
            if (written + 1 < capacity) output.push_back(static_cast<char16_t>(0xdc00 + (substitution & 0x3ff)));
            else ++extra;
        }
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + extra);
    const auto outgoing = Terminate(output, capacity, required, incoming);
    CopyOutput(destination, output, __func__);
    if (substitutions) CopyOut(substitutions, &replaced, sizeof(replaced), __func__, "substitution count");
    if (destinationLength) CopyOut(destinationLength, &required, sizeof(required), __func__, "destination length");
    WriteStatus(status, incoming, outgoing, __func__);
    return destination;
}


char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                    const char16_t* source, std::int32_t sourceLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if ((!source && sourceLength != 0) || sourceLength < -1 || (!destination && capacity != 0) || capacity < 0) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    const auto input = CopySource(source, sourceLength, __func__);
    std::vector<char> output;
    output.reserve(static_cast<std::size_t>(std::min<std::int64_t>(capacity, 3 * static_cast<std::int64_t>(input.size()) + 1)));
    std::int64_t extra = 0;
    bool full = false;
    for (const auto unit : input) {
        const auto size = Encoded(unit);
        if (!full && capacity - static_cast<std::int64_t>(output.size()) >= size) {
            if (size == 1) {
                output.push_back(static_cast<char>(unit));
            } else if (size == 2) {
                output.push_back(static_cast<char>(0xc0 | (unit >> 6)));
                output.push_back(static_cast<char>(0x80 | (unit & 0x3f)));
            } else {
                output.push_back(static_cast<char>(0xe0 | (unit >> 12)));
                output.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (unit & 0x3f)));
            }
        } else {
            full = true;
            extra += size;
        }
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + extra);
    const auto outgoing = Terminate(output, capacity, required, incoming);
    CopyOutput(destination, output, __func__);
    if (destinationLength) CopyOut(destinationLength, &required, sizeof(required), __func__, "destination length");
    WriteStatus(status, incoming, outgoing, __func__);
    return destination;
}

}
