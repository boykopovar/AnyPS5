#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int BadObjectHeader = 102;
constexpr int MallocFailure = 65;

constexpr char DecodeCaller[] = "d2i_SSL_SESSION";

constexpr std::uint64_t SessionAsn1Version = 1;
constexpr std::uint64_t DefaultSessionTimeout = 3;

constexpr unsigned char IntegerTag = 0x02;
constexpr unsigned char OctetStringTag = 0x04;
constexpr unsigned char SequenceTag = 0x30;
constexpr unsigned char TimeTag = 0xa1;
constexpr unsigned char TimeoutTag = 0xa2;
constexpr unsigned char PeerCertificateTag = 0xa3;
constexpr unsigned char SessionIdContextTag = 0xa4;
constexpr unsigned char VerifyResultTag = 0xa5;
constexpr unsigned char HostnameTag = 0xa6;
constexpr unsigned char LifetimeTag = 0xa9;
constexpr unsigned char TicketTag = 0xaa;

constexpr std::size_t MasterKeyCapacity = sizeof(LibreSsl3SslSession::masterKey);
constexpr std::size_t SessionIdCapacity = sizeof(LibreSsl3SslSession::sessionId);
constexpr std::size_t SessionIdContextCapacity = sizeof(LibreSsl3SslSession::sidCtx);

struct Tlv {
    unsigned char tag;
    std::span<const unsigned char> content;
};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> bytes{};
    std::size_t count = 0;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8)
        bytes[count++] = static_cast<unsigned char>(remaining);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) output.push_back(bytes[--count]);
}

void AppendTlv(std::vector<unsigned char>& output, unsigned char tag, std::span<const unsigned char> content) {
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

void AppendInteger(std::vector<unsigned char>& output, std::uint64_t value) {
    std::array<unsigned char, 8> bytes{};
    std::size_t count = 0;
    for (std::uint64_t remaining = value; remaining != 0; remaining >>= 8)
        bytes[count++] = static_cast<unsigned char>(remaining);
    std::vector<unsigned char> content;
    if (count == 0) {
        content.push_back(0);
    } else {
        if ((bytes[count - 1] & 0x80) != 0) content.push_back(0);
        for (std::size_t index = count; index != 0; --index) content.push_back(bytes[index - 1]);
    }
    AppendTlv(output, IntegerTag, content);
}

void AppendIntegerElement(std::vector<unsigned char>& output, unsigned char tag, std::uint64_t value) {
    std::vector<unsigned char> content;
    AppendInteger(content, value);
    AppendTlv(output, tag, content);
}

void AppendTaggedOctetString(std::vector<unsigned char>& output, unsigned char tag,
    std::span<const unsigned char> value) {
    std::vector<unsigned char> content;
    AppendTlv(content, OctetStringTag, value);
    AppendTlv(output, tag, content);
}

bool EncodePeerCertificate(const void* peerCertificate, std::vector<unsigned char>& der) {
    unsigned char* encoded = nullptr;
    const int length = i2d_X509_nid_postfix(static_cast<const LibreSsl3Certificate*>(peerCertificate), &encoded);
    if (length <= 0 || !encoded) {
        if (encoded) free_nid_postfix(encoded);
        return false;
    }
    der.assign(encoded, encoded + length);
    free_nid_postfix(encoded);
    return true;
}

bool EncodeSession(const LibreSsl3SslSession& session, std::vector<unsigned char>& encoded) {
    if (session.sslVersion < 0) return false;
    if (session.masterKeyLength > MasterKeyCapacity) return false;
    if (session.sessionIdLength > SessionIdCapacity) return false;
    if (session.sidCtxLength > SessionIdContextCapacity) return false;
    if (session.time < 0 || session.timeout < 0 || session.verifyResult < 0) return false;

    std::vector<unsigned char> content;
    AppendInteger(content, SessionAsn1Version);
    AppendInteger(content, static_cast<std::uint64_t>(session.sslVersion));

    const std::array<unsigned char, 2> cipherSuite{static_cast<unsigned char>(session.cipherValue >> 8),
        static_cast<unsigned char>(session.cipherValue)};
    AppendTlv(content, OctetStringTag, cipherSuite);

    AppendTlv(content, OctetStringTag,
        std::span<const unsigned char>(session.sessionId, static_cast<std::size_t>(session.sessionIdLength)));
    AppendTlv(content, OctetStringTag,
        std::span<const unsigned char>(session.masterKey, static_cast<std::size_t>(session.masterKeyLength)));

    if (session.time != 0) AppendIntegerElement(content, TimeTag, static_cast<std::uint64_t>(session.time));
    if (session.timeout != 0) AppendIntegerElement(content, TimeoutTag, static_cast<std::uint64_t>(session.timeout));

    if (session.peerCert) {
        std::vector<unsigned char> peerCertificate;
        if (!EncodePeerCertificate(session.peerCert, peerCertificate)) return false;
        AppendTlv(content, PeerCertificateTag, peerCertificate);
    }

    AppendTaggedOctetString(content, SessionIdContextTag,
        std::span<const unsigned char>(session.sidCtx, static_cast<std::size_t>(session.sidCtxLength)));

    if (session.verifyResult != 0)
        AppendIntegerElement(content, VerifyResultTag, static_cast<std::uint64_t>(session.verifyResult));

    encoded.clear();
    AppendTlv(encoded, SequenceTag, content);
    return true;
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) return false;
    const unsigned char tag = input[0];
    const unsigned char lengthByte = input[1];
    if ((tag & 0x1f) == 0x1f) return false;
    std::size_t header = 2;
    std::size_t length = lengthByte;
    if ((lengthByte & 0x80) != 0) {
        const std::size_t count = lengthByte & 0x7f;
        if (count == 0 || count == 0x7f || count > 4) return false;
        if (input.size() < 2 + count) return false;
        std::size_t longLength = 0;
        for (std::size_t index = 0; index < count; ++index)
            longLength = (longLength << 8) | input[2 + index];
        if (longLength < 0x80 || input[2] == 0) return false;
        length = longLength;
        header += count;
    }
    if (length > input.size() - header) return false;
    value.tag = tag;
    value.content = input.subspan(header, length);
    input = input.subspan(header + length);
    return true;
}

bool ReadTlv(std::span<const unsigned char>& input, unsigned char tag, Tlv& value) {
    return ReadTlv(input, value) && value.tag == tag;
}

bool ReadOptional(std::span<const unsigned char>& input, unsigned char tag, Tlv& value, bool& present) {
    present = !input.empty() && input[0] == tag;
    if (!present) return true;
    return ReadTlv(input, tag, value);
}

bool ReadIntegerContent(std::span<const unsigned char> content, std::uint64_t& value) {
    if (content.empty()) return false;
    if ((content[0] & 0x80) != 0) return false;
    if (content[0] == 0 && content.size() > 1 && (content[1] & 0x80) == 0) return false;
    value = 0;
    for (const unsigned char byte : content) {
        if ((value >> 56) != 0) return false;
        value = (value << 8) | byte;
    }
    return true;
}

bool ReadRequiredInteger(std::span<const unsigned char>& input, std::uint64_t& value) {
    Tlv integer;
    if (!ReadTlv(input, IntegerTag, integer)) return false;
    return ReadIntegerContent(integer.content, value);
}

bool ReadOptionalInteger(std::span<const unsigned char>& input, unsigned char tag, std::uint64_t& value,
    std::uint64_t defaultValue) {
    Tlv element;
    bool present = false;
    if (!ReadOptional(input, tag, element, present)) return false;
    if (!present) {
        value = defaultValue;
        return true;
    }
    std::span<const unsigned char> remaining = element.content;
    Tlv integer;
    if (!ReadTlv(remaining, IntegerTag, integer)) return false;
    if (!remaining.empty()) return false;
    return ReadIntegerContent(integer.content, value);
}

bool ReadRequiredOctets(std::span<const unsigned char>& input, std::span<const unsigned char>& value) {
    Tlv octets;
    if (!ReadTlv(input, OctetStringTag, octets)) return false;
    value = octets.content;
    return true;
}

bool ReadOptionalOctets(std::span<const unsigned char>& input, unsigned char tag,
    std::span<const unsigned char>& value, bool& present) {
    Tlv element;
    if (!ReadOptional(input, tag, element, present)) return false;
    if (!present) return true;
    std::span<const unsigned char> remaining = element.content;
    Tlv octets;
    if (!ReadTlv(remaining, OctetStringTag, octets)) return false;
    if (!remaining.empty()) return false;
    value = octets.content;
    return true;
}

bool CopyIntoArray(std::span<const unsigned char> source, unsigned char* destination, std::size_t capacity) {
    if (source.size() > capacity) return false;
    if (!source.empty()) std::memcpy(destination, source.data(), source.size());
    return true;
}

bool DecodeSession(std::span<const unsigned char> input, std::span<const unsigned char>& rest,
    LibreSsl3SslSession& session) {
    Tlv sequence;
    if (!ReadTlv(input, SequenceTag, sequence)) return false;
    std::span<const unsigned char> content = sequence.content;

    std::uint64_t version = 0;
    if (!ReadRequiredInteger(content, version)) return false;
    if (version != SessionAsn1Version) return false;

    std::uint64_t tlsVersion = 0;
    if (!ReadRequiredInteger(content, tlsVersion)) return false;
    if (tlsVersion > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) return false;
    session.sslVersion = static_cast<std::int32_t>(tlsVersion);

    std::span<const unsigned char> cipherSuite;
    if (!ReadRequiredOctets(content, cipherSuite)) return false;
    if (cipherSuite.size() != 2) return false;
    session.cipherValue = static_cast<std::uint16_t>(cipherSuite[0] << 8 | cipherSuite[1]);

    std::span<const unsigned char> sessionId;
    if (!ReadRequiredOctets(content, sessionId)) return false;
    if (!CopyIntoArray(sessionId, session.sessionId, SessionIdCapacity)) return false;
    session.sessionIdLength = sessionId.size();

    std::span<const unsigned char> masterKey;
    if (!ReadRequiredOctets(content, masterKey)) return false;
    if (!CopyIntoArray(masterKey, session.masterKey, MasterKeyCapacity)) return false;
    session.masterKeyLength = masterKey.size();

    std::uint64_t sessionTime = 0;
    if (!ReadOptionalInteger(content, TimeTag, sessionTime, 0)) return false;
    if (sessionTime > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
    session.time = static_cast<std::int64_t>(std::time(nullptr));
    if (sessionTime != 0) session.time = static_cast<std::int64_t>(sessionTime);

    std::uint64_t timeout = 0;
    if (!ReadOptionalInteger(content, TimeoutTag, timeout, DefaultSessionTimeout)) return false;
    if (timeout > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
    session.timeout = static_cast<std::int64_t>(DefaultSessionTimeout);
    if (timeout != 0) session.timeout = static_cast<std::int64_t>(timeout);

    session.peerCert = nullptr;
    Tlv peerCertificate;
    bool present = false;
    if (!ReadOptional(content, PeerCertificateTag, peerCertificate, present)) return false;
    if (present) NotImplemented_nid_no_patch(DecodeCaller);

    session.sidCtxLength = 0;
    std::span<const unsigned char> sessionIdContext;
    if (!ReadOptionalOctets(content, SessionIdContextTag, sessionIdContext, present)) return false;
    if (present) {
        if (!CopyIntoArray(sessionIdContext, session.sidCtx, SessionIdContextCapacity)) return false;
        session.sidCtxLength = sessionIdContext.size();
    }

    std::uint64_t verifyResult = 0;
    if (!ReadOptionalInteger(content, VerifyResultTag, verifyResult, 0)) return false;
    if (verifyResult > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
    session.verifyResult = static_cast<std::int64_t>(verifyResult);

    std::span<const unsigned char> hostname;
    if (!ReadOptionalOctets(content, HostnameTag, hostname, present)) return false;
    if (present) NotImplemented_nid_no_patch(DecodeCaller);

    std::uint64_t lifetime = 0;
    if (!ReadOptionalInteger(content, LifetimeTag, lifetime, 0)) return false;
    if (lifetime != 0) NotImplemented_nid_no_patch(DecodeCaller);

    std::span<const unsigned char> ticket;
    if (!ReadOptionalOctets(content, TicketTag, ticket, present)) return false;
    if (present) NotImplemented_nid_no_patch(DecodeCaller);

    rest = input;
    return true;
}

}

extern "C" {

int APS5_VABI i2d_SSL_SESSION_nid_postfix(LibreSsl3SslSession* session, unsigned char** output) {
    if (!session) return 0;
    LibreSsl3SslSession fields{};
    if (!CopyIn(&fields, session, sizeof(fields))) NotImplemented_nid_no_patch(__func__);
    if (fields.cipherValue == 0) return 0;

    std::vector<unsigned char> encoded;
    if (!EncodeSession(fields, encoded)) return -1;
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return -1;
    const int length = static_cast<int>(encoded.size());

    if (!output) return length;

    unsigned char* target = nullptr;
    if (!CopyIn(&target, output, sizeof(target))) NotImplemented_nid_no_patch(__func__);

    unsigned char* next = target;
    if (target) {
        if (!CopyOut(target, encoded.data(), encoded.size())) NotImplemented_nid_no_patch(__func__);
        next = target + encoded.size();
    } else {
        auto* allocated = static_cast<unsigned char*>(calloc_nid_postfix(1, encoded.size()));
        if (!allocated) return -1;
        std::memcpy(allocated, encoded.data(), encoded.size());
        next = allocated;
    }
    if (!CopyOut(output, &next, sizeof(next))) {
        if (!target) free_nid_postfix(next);
        NotImplemented_nid_no_patch(__func__);
    }
    return length;
}

LibreSsl3SslSession* APS5_VABI d2i_SSL_SESSION_nid_postfix(
    LibreSsl3SslSession** output, const unsigned char** input, std::int64_t length) {
    if (!input || length < 0) {
        PutError(BadObjectHeader);
        return nullptr;
    }
    const unsigned char* start = nullptr;
    if (!CopyIn(&start, input, sizeof(start)) || !start) {
        PutError(BadObjectHeader);
        return nullptr;
    }

    LibreSsl3SslSession* existing = nullptr;
    if (output && !CopyIn(&existing, output, sizeof(existing))) {
        PutError(BadObjectHeader);
        return nullptr;
    }

    LibreSsl3SslSession session{};
    if (existing) {
        if (!CopyIn(&session, existing, sizeof(session))) NotImplemented_nid_no_patch(__func__);
    } else {
        session.references = 1;
    }

    std::span<const unsigned char> remaining(start, static_cast<std::size_t>(length));
    std::span<const unsigned char> rest;
    if (!DecodeSession(remaining, rest, session)) return nullptr;

    LibreSsl3SslSession* decoded = existing;
    if (decoded) {
        if (!CopyOut(decoded, &session, sizeof(session))) NotImplemented_nid_no_patch(__func__);
    } else {
        decoded = static_cast<LibreSsl3SslSession*>(calloc_nid_postfix(1, sizeof(LibreSsl3SslSession)));
        if (!decoded) {
            PutError(MallocFailure);
            return nullptr;
        }
        std::memcpy(decoded, &session, sizeof(session));
    }

    const auto* end = start + (static_cast<std::size_t>(length) - rest.size());
    if (!CopyOut(input, &end, sizeof(end))) {
        if (!existing) free_nid_postfix(decoded);
        NotImplemented_nid_no_patch(__func__);
    }
    if (output && !CopyOut(output, &decoded, sizeof(decoded))) {
        if (!existing) free_nid_postfix(decoded);
        NotImplemented_nid_no_patch(__func__);
    }
    return decoded;
}

}
