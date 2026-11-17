#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/Utf8.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

constexpr std::int32_t NoSubstitution = -1;

template<class T>
class OutputAccess {
public:
    OutputAccess(T* destination, std::int32_t capacity, std::int32_t* length,
                 std::int32_t* substitutions, std::int32_t* error)
        : errorAccess(error, sizeof(*error)),
          lengthAccess(length, length ? sizeof(*length) : 0),
          substitutionsAccess(substitutions, substitutions ? sizeof(*substitutions) : 0),
          destinationAccess(capacity > 0 ? destination : nullptr, static_cast<std::size_t>(capacity) * sizeof(T)) {
        if (!errorAccess.Open() || !lengthAccess.Open() || !substitutionsAccess.Open() || !destinationAccess.Open())
            NotImplemented_nid_no_patch(__func__);
    }

private:
    GuestArena::HostWrite errorAccess;
    GuestArena::HostWrite lengthAccess;
    GuestArena::HostWrite substitutionsAccess;
    GuestArena::HostWrite destinationAccess;
};

class OutputGuard {
public:
    OutputGuard(void* destination, std::int32_t capacity, std::size_t width, std::int32_t* destinationLength,
                std::int32_t* substitutions, std::int32_t* status, const std::string& caller)
        : destinationAccess(capacity > 0 ? destination : nullptr, static_cast<std::size_t>(capacity) * width),
          lengthAccess(destinationLength, destinationLength ? sizeof(*destinationLength) : 0),
          substitutionsAccess(substitutions, substitutions ? sizeof(*substitutions) : 0),
          statusAccess(status, sizeof(*status)) {
        if (!destinationAccess.Open() || !lengthAccess.Open() || !substitutionsAccess.Open() || !statusAccess.Open())
            throw std::invalid_argument(caller + ": unwritable output");
    }

private:
    GuestArena::HostWrite destinationAccess;
    GuestArena::HostWrite lengthAccess;
    GuestArena::HostWrite substitutionsAccess;
    GuestArena::HostWrite statusAccess;
};

bool Surrogate(std::int32_t value) { return value >= 0xd800 && value <= 0xdfff; }

template<class TSource, class TDestination>
bool Validate(TDestination* destination, std::int32_t capacity, const TSource* source,
              std::int32_t length, std::int32_t substitution, std::int32_t* error) {
    if (!error) NotImplemented_nid_no_patch(__func__);
    if (*error > 0) return false;
    if ((!source && length != 0) || length < -1 || capacity < 0 || (!destination && capacity > 0) ||
        substitution > 0x10ffff || Surrogate(substitution)) {
        const GuestArena::HostWrite access(error, sizeof(*error));
        if (!access.Open()) NotImplemented_nid_no_patch(__func__);
        *error = IllegalArgument;
        return false;
    }
    return true;
}

template<class T>
std::int32_t Length(const T* source, std::int32_t length) {
    if (length >= 0) return length;
    const auto measured = std::char_traits<T>::length(source);
    if (measured > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(measured);
}

template<class T>
void Finish(T* destination, std::int32_t capacity, std::int64_t count, std::int32_t* length, std::int32_t* error) {
    if (count > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    if (length) *length = static_cast<std::int32_t>(count);
    if (*error > 0) return;
    if (count < capacity) {
        destination[count] = 0;
        if (*error == NotTerminated) *error = 0;
    } else {
        *error = count == capacity ? NotTerminated : BufferOverflow;
    }
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const std::string& caller, const char* what) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    if (!copied) throw std::invalid_argument(caller + ": unwritable " + what);
}

std::int32_t ReadStatus(const std::int32_t* status, const std::string& caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    if (!CopyIn(&value, status, sizeof(value))) throw std::invalid_argument(caller + ": unreadable status");
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const std::string& caller) {
    if (outgoing != incoming) CopyOut(status, &outgoing, sizeof(outgoing), caller, "status");
}

template<class T>
std::vector<T> ReadSource(const T* source, std::int32_t length, const std::string& caller) {
    constexpr std::uintptr_t Chunk = 0x1000;
    std::vector<unsigned char> bytes;
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    if (length >= 0) {
        const auto total = static_cast<std::size_t>(length) * sizeof(T);
        while (bytes.size() < total) {
            const std::size_t offset = bytes.size();
            const std::size_t count = (std::min)(Chunk - (address + offset) % Chunk, total - offset);
            bytes.resize(offset + count);
            if (!CopyIn(bytes.data() + offset, reinterpret_cast<const void*>(address + offset), count))
                throw std::invalid_argument(caller + ": unreadable source");
        }
    } else {
        bool terminated = false;
        for (std::size_t offset = 0, scanned = 0; !terminated;) {
            const std::size_t start = bytes.size();
            const std::size_t count = Chunk - (address + offset) % Chunk;
            bytes.resize(start + count);
            if (!CopyIn(bytes.data() + start, reinterpret_cast<const void*>(address + offset), count))
                throw std::invalid_argument(caller + ": unreadable source");
            offset += count;
            for (; scanned + sizeof(T) <= bytes.size(); scanned += sizeof(T)) {
                T unit{};
                std::memcpy(&unit, bytes.data() + scanned, sizeof(T));
                if (unit != 0) continue;
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
void CopyOutput(T* destination, const std::vector<T>& output, const std::string& caller) {
    if (!output.empty()) CopyOut(destination, output.data(), output.size() * sizeof(T), caller, "destination");
}

template<class T>
void Complete(std::vector<T>& output, T* destination, std::int32_t capacity, std::int32_t required, std::int32_t replaced,
              std::int32_t* destinationLength, std::int32_t* substitutions, std::int32_t* status, std::int32_t incoming,
              const std::string& caller) {
    if (substitutions) CopyOut(substitutions, &replaced, sizeof(replaced), caller, "substitution count");
    if (destinationLength) CopyOut(destinationLength, &required, sizeof(required), caller, "destination length");
    auto current = incoming;
    if (substitutions == status) current = replaced;
    if (destinationLength == status) current = required;
    auto outgoing = current;
    if (current <= 0 && required < capacity) output.push_back(0);
    Icu::Terminate(output.data(), capacity, required, &outgoing);
    CopyOutput(destination, output, caller);
    if (outgoing != current) CopyOut(status, &outgoing, sizeof(outgoing), caller, "status");
}

std::int32_t Decode(const std::uint8_t* text, std::int32_t length, std::int32_t& index) {
    const std::int32_t lead = text[index++];
    if (lead <= 0x7f) return lead;
    return utf8_nextCharSafeBody_nid_postfix(text, &index, length, lead, -1);
}

char16_t* FromUtf8(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                   const char* source, std::int32_t sourceLength, std::int32_t substitution,
                   std::int32_t* substitutions, std::int32_t* status, const std::string& caller) {
    const auto incoming = ReadStatus(status, caller);
    if (incoming > 0) return nullptr;
    if ((!source && sourceLength != 0) || sourceLength < -1 || (!destination && capacity > 0) || capacity < 0 ||
        substitution > 0x10ffff || Icu::Surrogate(substitution)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, caller);
        return nullptr;
    }
    const OutputGuard guard(destination, capacity, sizeof(char16_t), destinationLength, substitutions, status, caller);
    constexpr std::int32_t none = 0;
    if (substitutions) CopyOut(substitutions, &none, sizeof(none), caller, "substitution count");
    const auto bytes = ReadSource(reinterpret_cast<const std::uint8_t*>(source), sourceLength, caller);
    const auto length = static_cast<std::int32_t>(bytes.size());
    std::vector<char16_t> output;
    output.reserve(static_cast<std::size_t>(length));
    bool writing = true;
    std::int64_t extra = 0;
    std::int32_t replaced = 0;
    for (std::int32_t index = 0; index < length;) {
        auto value = Decode(bytes.data(), length, index);
        if (value < 0) {
            if (substitution < 0) {
                CopyOutput(destination, output, caller);
                WriteStatus(status, incoming, Icu::InvalidCharacter, caller);
                return nullptr;
            }
            value = substitution;
            ++replaced;
        }
        const auto emit = [&](char16_t unit) {
            if (writing && static_cast<std::int64_t>(output.size()) < capacity) output.push_back(unit);
            else {
                writing = false;
                ++extra;
            }
        };
        if (value <= 0xffff) {
            emit(static_cast<char16_t>(value));
        } else {
            emit(static_cast<char16_t>(0xd7c0 + (value >> 10)));
            emit(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
        }
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + extra);
    Complete(output, destination, capacity, required, replaced, destinationLength, substitutions, status, incoming, caller);
    return destination;
}

char* ToUtf8(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
             const char16_t* source, std::int32_t sourceLength, std::int32_t substitution,
             std::int32_t* substitutions, std::int32_t* status, const std::string& caller) {
    const auto incoming = ReadStatus(status, caller);
    if (incoming > 0) return nullptr;
    if ((!source && sourceLength != 0) || sourceLength < -1 || (!destination && capacity > 0) || capacity < 0 ||
        substitution > 0x10ffff || Icu::Surrogate(substitution)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, caller);
        return nullptr;
    }
    const OutputGuard guard(destination, capacity, sizeof(char), destinationLength, substitutions, status, caller);
    constexpr std::int32_t none = 0;
    if (substitutions) CopyOut(substitutions, &none, sizeof(none), caller, "substitution count");
    const auto units = ReadSource(source, sourceLength, caller);
    const auto length = static_cast<std::int64_t>(units.size());
    std::vector<char> output;
    output.reserve(static_cast<std::size_t>(length));
    bool writing = true;
    std::int64_t extra = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length; ++index) {
        std::int32_t value = units[index];
        if (Icu::Surrogate(value)) {
            if (value < 0xdc00 && index + 1 < length && units[index + 1] >= 0xdc00 && units[index + 1] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + units[index + 1] - 0xdc00;
                ++index;
            } else if (substitution >= 0) {
                value = substitution;
                ++replaced;
            } else {
                CopyOutput(destination, output, caller);
                WriteStatus(status, incoming, Icu::InvalidCharacter, caller);
                return nullptr;
            }
        }
        std::array<char, 4> encoded{};
        unsigned width;
        if (value < 0x80) {
            width = 1;
            encoded[0] = static_cast<char>(value);
        } else if (value < 0x800) {
            width = 2;
            encoded[0] = static_cast<char>(0xc0 | (value >> 6));
        } else if (value < 0x10000) {
            width = 3;
            encoded[0] = static_cast<char>(0xe0 | (value >> 12));
        } else {
            width = 4;
            encoded[0] = static_cast<char>(0xf0 | (value >> 18));
        }
        for (unsigned trail = 1; trail < width; ++trail)
            encoded[trail] = static_cast<char>(0x80 | ((value >> (6 * (width - 1 - trail))) & 0x3f));
        if (writing && static_cast<std::int64_t>(output.size()) + width <= capacity) {
            output.insert(output.end(), encoded.begin(), encoded.begin() + width);
        } else {
            writing = false;
            extra += width;
        }
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + extra);
    Complete(output, destination, capacity, required, replaced, destinationLength, substitutions, status, incoming, caller);
    return destination;
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_strFromUTF8_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t* error) {
    return FromUtf8(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, error, __func__);
}

char* APS5_VABI u_strToUTF8_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char16_t* source, std::int32_t sourceLength, std::int32_t* error) {
    return ToUtf8(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, error, __func__);
}

char16_t* APS5_VABI u_strFromUTF8WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return FromUtf8(destination, capacity, destinationLength, source, sourceLength, substitution, substitutions, error, __func__);
}

char* APS5_VABI u_strToUTF8WithSub_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char16_t* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return ToUtf8(destination, capacity, destinationLength, source, sourceLength, substitution, substitutions, error, __func__);
}

char16_t* APS5_VABI u_strFromUTF8Lenient_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t* error) {
    std::int32_t status = 0;
    if (!GuestCopyIn_nid_no_patch(&status, error, sizeof(status))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (status > 0) return nullptr;
    const auto writeStatus = [&](std::int32_t value) {
        if (!GuestCopyOut_nid_no_patch(error, &value, sizeof(value))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    };
    const auto writeLength = [&](std::int32_t value) {
        if (destinationLength && !GuestCopyOut_nid_no_patch(destinationLength, &value, sizeof(value)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    };
    if ((!source && sourceLength != 0) || sourceLength < -1 || capacity < 0 || (!destination && capacity > 0)) {
        writeStatus(IllegalArgument);
        return nullptr;
    }
    if (!GuestWritable_nid_no_patch(error, sizeof(*error)) ||
        (destinationLength && !GuestWritable_nid_no_patch(destinationLength, sizeof(*destinationLength))))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (sourceLength >= 0 && capacity < sourceLength) {
        writeLength(sourceLength);
        writeStatus(BufferOverflow);
        return nullptr;
    }
    std::array<std::uint8_t, 0x1000> input;
    std::int64_t inputBase = 0;
    std::int64_t inputCount = 0;
    const auto byte = [&](std::int64_t position) {
        if (position < inputBase || position >= inputBase + inputCount) {
            const auto offset = reinterpret_cast<std::uintptr_t>(source + position) % input.size();
            const auto count = input.size() - offset;
            if (!GuestCopyIn_nid_no_patch(input.data(), source + position, count)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
            inputBase = position;
            inputCount = static_cast<std::int64_t>(count);
        }
        return input[static_cast<std::size_t>(position - inputBase)];
    };
    std::int64_t length = sourceLength;
    if (length < 0) {
        length = 0;
        while (byte(length) != 0) {
            if (++length > std::numeric_limits<std::int32_t>::max()) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
    }
    std::array<char16_t, 0x800> output;
    std::size_t pending = 0;
    std::int64_t written = 0;
    const auto flush = [&] {
        if (!GuestCopyOut_nid_no_patch(destination + written, output.data(), pending * sizeof(char16_t)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        written += static_cast<std::int64_t>(pending);
        pending = 0;
    };
    std::int64_t count = 0;
    const auto emit = [&](char16_t unit) {
        if (count < capacity) {
            output[pending++] = unit;
            if (pending == output.size()) flush();
        }
        ++count;
    };
    for (std::int64_t position = 0; position < length;) {
        const std::uint32_t lead = byte(position);
        const unsigned units = lead < 0xc0 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (length - position < static_cast<std::int64_t>(units)) { emit(0xfffd); break; }
        std::uint32_t value = lead;
        for (unsigned index = 1; index < units; ++index) value = (value << 6) + byte(position + index);
        value -= units == 2 ? 0x3080 : units == 3 ? 0x2080 : units == 4 ? 0x3c82080 : 0;
        if (units < 4) emit(static_cast<char16_t>(value));
        else {
            emit(static_cast<char16_t>(0xd7c0 + (value >> 10)));
            emit(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
        }
        position += units;
    }
    flush();
    writeLength(static_cast<std::int32_t>(count));
    if (!GuestCopyIn_nid_no_patch(&status, error, sizeof(status))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (status > 0) return destination;
    if (count < capacity) {
        const char16_t terminator = 0;
        if (!GuestCopyOut_nid_no_patch(destination + count, &terminator, sizeof(terminator)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        if (status == NotTerminated) writeStatus(0);
    } else {
        writeStatus(count == capacity ? NotTerminated : BufferOverflow);
    }
    return destination;
}

}
