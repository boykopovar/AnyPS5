#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename T>
T CopyInPreservingErrno(const void* guest, const char* caller) {
    PreservedErrno guard;
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void CopyOutPreservingErrno(T* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

void AppendGuestBytes(std::vector<unsigned char>& output, const unsigned char* data, int length, const char* caller) {
    if (length <= 0) return;
    const auto offset = output.size();
    output.resize(offset + static_cast<std::size_t>(length));
    PreservedErrno guard;
    if (!GuestCopyIn_nid_no_patch(output.data() + offset, data, static_cast<std::size_t>(length)))
        NotImplemented_nid_no_patch(caller);
}

void AppendElement(std::vector<unsigned char>& output, int tag, const std::vector<unsigned char>& content) {
    output.push_back(static_cast<unsigned char>(tag));
    const std::size_t length = content.size();
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
    } else {
        int count = 0;
        for (std::size_t rest = length; rest != 0; rest >>= 8) ++count;
        output.push_back(static_cast<unsigned char>(0x80 | count));
        while (count > 0) output.push_back(static_cast<unsigned char>(length >> (8 * --count)));
    }
    output.insert(output.end(), content.begin(), content.end());
}

void AppendObject(std::vector<unsigned char>& output, const void* guestObject, const char* caller) {
    std::vector<unsigned char> content;
    if (guestObject) {
        const auto object = CopyInPreservingErrno<LibreSslAsn1Object>(guestObject, caller);
        AppendGuestBytes(content, object.data, object.length, caller);
    }
    AppendElement(output, 0x06, content);
}

void AppendParameter(std::vector<unsigned char>& output, const LibreSsl3Asn1Type* guestParameter, const char* caller) {
    if (!guestParameter) return;
    const auto parameter = CopyInPreservingErrno<LibreSsl3Asn1Type>(guestParameter, caller);
    if (parameter.type == 1) {
        const auto value = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(parameter.value));
        if (value != -1) AppendElement(output, 0x01, {static_cast<unsigned char>(value)});
        return;
    }
    if (parameter.type == 5) {
        AppendElement(output, 0x05, {});
        return;
    }
    if (parameter.type == 6) {
        AppendObject(output, parameter.value, caller);
        return;
    }
    const bool raw = parameter.type == 16 || parameter.type == 17 || parameter.type == -3;
    if (!raw && (parameter.type < 2 || parameter.type > 30))
        NotImplemented_nid_no_patch((std::string(caller) + ": algorithm parameter of ASN.1 type " + std::to_string(parameter.type)).c_str());
    if (!parameter.value) NotImplemented_nid_no_patch((std::string(caller) + ": null algorithm parameter").c_str());
    const auto string = CopyInPreservingErrno<LibreSsl3Asn1String>(parameter.value, caller);
    std::vector<unsigned char> content;
    if (parameter.type == 3) content.push_back(static_cast<unsigned char>((string.flags & 0x08) != 0 ? string.flags & 0x07 : 0));
    AppendGuestBytes(content, string.data, string.length, caller);
    if (raw) output.insert(output.end(), content.begin(), content.end());
    else AppendElement(output, parameter.type, content);
}

std::vector<unsigned char> EncodeSubjectPublicKeyInfo(const LibreSsl3X509Pubkey& key, const char* caller) {
    if (!key.algor) NotImplemented_nid_no_patch((std::string(caller) + ": null public key algorithm").c_str());
    const auto algor = CopyInPreservingErrno<LibreSsl3X509Algor>(key.algor, caller);
    std::vector<unsigned char> algorithm;
    AppendObject(algorithm, algor.algorithm, caller);
    AppendParameter(algorithm, algor.parameter, caller);
    const auto bits = CopyInPreservingErrno<LibreSsl3Asn1String>(key.publicKey, caller);
    std::vector<unsigned char> bitString{0};
    AppendGuestBytes(bitString, bits.data, bits.length, caller);
    std::vector<unsigned char> body;
    AppendElement(body, 0x30, algorithm);
    AppendElement(body, 0x03, bitString);
    std::vector<unsigned char> encoded;
    AppendElement(encoded, 0x30, body);
    return encoded;
}

LibreSsl3EvpPkey* DecodeEncodedKey(LibreSsl3X509Pubkey* guestKey, const LibreSsl3X509Pubkey& key, const char* caller) {
    const std::vector<unsigned char> bytes = EncodeSubjectPublicKeyInfo(key, caller);
    const unsigned char* cursor = bytes.data();
    auto* decoded = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(bytes.size()));
    if (!decoded) return nullptr;

    auto* slot = reinterpret_cast<LibreSsl3EvpPkey**>(
        reinterpret_cast<unsigned char*>(guestKey) + offsetof(LibreSsl3X509Pubkey, pkey));
    const auto previous = CopyInPreservingErrno<LibreSsl3EvpPkey*>(slot, caller);
    if (previous) {
        EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
        return previous;
    }
    {
        PreservedErrno guard;
        if (!GuestWritable_nid_no_patch(slot, sizeof(*slot))) {
            EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
            NotImplemented_nid_no_patch(caller);
        }
    }
    CopyOutPreservingErrno(slot, decoded, caller);
    return decoded;
}

}

extern "C" void* APS5_VABI X509_get0_pubkey_nid_postfix(const LibreSsl3Certificate* certificate) {
    if (!certificate) return nullptr;
    const auto* certInfo = CopyInPreservingErrno<LibreSsl3CertificateInfo*>(
        reinterpret_cast<const unsigned char*>(certificate) + offsetof(LibreSsl3Certificate, certInfo), "X509_get0_pubkey");
    if (!certInfo) return nullptr;
    auto* guestKey = CopyInPreservingErrno<LibreSsl3X509Pubkey*>(
        reinterpret_cast<const unsigned char*>(certInfo) + offsetof(LibreSsl3CertificateInfo, key), "X509_get0_pubkey");
    if (!guestKey) return nullptr;
    const auto key = CopyInPreservingErrno<LibreSsl3X509Pubkey>(guestKey, "X509_get0_pubkey");
    if (!key.pkey && key.publicKey) {
        PreservedErrno guard;
        return DecodeEncodedKey(guestKey, key, "X509_get0_pubkey");
    }
    return key.pkey;
}
