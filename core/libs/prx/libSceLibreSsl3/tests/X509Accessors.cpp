#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 accessor check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

struct LaterName {
    int nid;
    const char* oid;
};

constexpr LaterName LaterNames[] {
    {956, "1.3.6.1.4.1.311.60.2.1.1"},
    {957, "1.3.6.1.4.1.311.60.2.1.2"},
    {958, "1.3.6.1.4.1.311.60.2.1.3"},
    {959, ""},
    {960, ""},
    {961, ""},
    {962, ""},
    {963, ""},
    {964, ""},
    {965, ""},
    {966, ""},
    {967, ""},
    {968, "1.2.156.10197.1.401"},
    {969, "1.2.156.10197.1.504"},
    {970, "1.2.156"},
    {971, "1.2.156.10197"},
    {972, "1.2.156.10197.1"},
    {973, "1.2.156.10197.1.104.1"},
    {974, "1.2.156.10197.1.104.2"},
    {975, "1.2.156.10197.1.104.3"},
    {976, "1.2.156.10197.1.104.4"},
    {977, "1.2.156.10197.1.104.5"},
    {978, "1.2.156.10197.1.104.6"},
    {979, "1.2.156.10197.1.104.7"},
    {980, "1.3.133.16.840.63.0.2"},
    {981, "1.3.132.1.11.0"},
    {982, "1.3.132.1.11.1"},
    {983, "1.3.132.1.11.2"},
    {984, "1.3.132.1.11.3"},
    {985, "1.3.133.16.840.63.0.3"},
    {986, "1.3.132.1.14.0"},
    {987, "1.3.132.1.14.1"},
    {988, "1.3.132.1.14.2"},
    {989, "1.3.132.1.14.3"},
    {990, ""},
    {991, ""},
    {992, "1.2.840.113549.1.1.9"},
    {993, "1.2.643.7.1.2.1.1.1"},
    {994, "1.2.643.7.1.2.1.1.2"},
    {995, "1.2.643.7.1.2.1.1.3"},
    {996, "1.2.643.7.1.2.1.1.4"},
    {997, "1.2.643.7.1.2.1.2.0"},
    {998, "1.2.643.7.1.2.1.2.3"},
    {999, "1.2.643.7.1.1.4.1"},
    {1000, "1.2.643.7.1.1.4.2"},
    {1001, "1.2.840.113549.1.9.16.1.24"},
    {1002, "1.2.840.113549.1.9.16.1.26"},
    {1003, "1.2.840.113549.1.9.16.1.35"},
    {1004, "1.2.840.113549.1.9.16.1.36"},
    {1005, "1.3.6.1.5.5.7.14"},
    {1006, "1.3.6.1.5.5.7.1.28"},
    {1007, "1.3.6.1.5.5.7.1.29"},
    {1008, "1.3.6.1.5.5.7.14.2"},
    {1009, "1.3.6.1.5.5.7.14.3"},
    {1010, "1.3.6.1.5.5.7.48.10"},
    {1011, "1.3.6.1.5.5.7.48.11"},
    {1012, "1.3.6.1.5.5.7.48.13"},
    {1013, "1.2.840.113549.1.9.16.1.47"},
    {1014, "1.2.840.113549.1.9.16.1.48"},
    {1015, "1.3.6.1.5.5.7.3.30"},
    {1016, "1.3.6.1.5.5.7.1.24"},
    {1017, "1.2.840.113549.1.9.16.1.49"},
    {1018, "1.3.6.1.4.1.11129.2.4.2"},
    {1019, "1.3.6.1.4.1.11129.2.4.3"},
    {1020, "1.3.6.1.4.1.11129.2.4.4"},
    {1021, "1.3.6.1.4.1.11129.2.4.5"},
    {1022, ""},
    {1023, "1.2.840.113549.1.9.16.2.47"},
    {1024, "1.2.840.113549.1.9.16.1.50"},
    {1025, "1.2.840.113549.1.1.15"},
    {1026, "1.2.840.113549.1.1.16"},
    {1027, "1.2.840.113549.2.12"},
    {1028, "1.2.840.113549.2.13"},
    {1029, "2.16.840.1.101.3.4.2.5"},
    {1030, "2.16.840.1.101.3.4.2.6"},
    {1031, "2.16.840.1.101.3.4.2.7"},
    {1032, "2.16.840.1.101.3.4.2.8"},
    {1033, "2.16.840.1.101.3.4.2.9"},
    {1034, "2.16.840.1.101.3.4.2.10"},
    {1035, "2.16.840.1.101.3.4.2.13"},
    {1036, "2.16.840.1.101.3.4.2.14"},
    {1037, "2.16.840.1.101.3.4.2.15"},
    {1038, "2.16.840.1.101.3.4.2.16"},
    {1039, "2.16.840.1.101.3.4.3.3"},
    {1040, "2.16.840.1.101.3.4.3.4"},
    {1041, "2.16.840.1.101.3.4.3.5"},
    {1042, "2.16.840.1.101.3.4.3.6"},
    {1043, "2.16.840.1.101.3.4.3.7"},
    {1044, "2.16.840.1.101.3.4.3.8"},
    {1045, "2.16.840.1.101.3.4.3.9"},
    {1046, "2.16.840.1.101.3.4.3.10"},
    {1047, "2.16.840.1.101.3.4.3.11"},
    {1048, "2.16.840.1.101.3.4.3.12"},
    {1049, "2.16.840.1.101.3.4.3.13"},
    {1050, "2.16.840.1.101.3.4.3.14"},
    {1051, "2.16.840.1.101.3.4.3.15"},
    {1052, "2.16.840.1.101.3.4.3.16"},
    {1053, "1.3.6.1.5.5.7.1.31"},
    {1054, "1.2.840.113549.1.9.16.1.51"},
    {1055, ""},
};

constexpr std::uint64_t UnknownNidError = (8ULL << 24) | (0xfffULL << 12) | 101;

std::vector<unsigned char> Encode(const std::string& dotted) {
    std::vector<std::uint64_t> arcs;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = dotted.find('.', start);
        arcs.push_back(std::stoull(dotted.substr(start, end == std::string::npos ? std::string::npos : end - start)));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    Require(arcs.size() >= 2);
    std::vector<unsigned char> out {static_cast<unsigned char>(arcs[0] * 40 + arcs[1])};
    for (std::size_t index = 2; index < arcs.size(); ++index) {
        std::vector<unsigned char> groups;
        std::uint64_t value = arcs[index];
        do {
            groups.push_back(static_cast<unsigned char>(value & 0x7f));
            value >>= 7;
        } while (value);
        for (std::size_t group = groups.size(); group-- > 1;) out.push_back(groups[group] | 0x80);
        out.push_back(groups[0]);
    }
    return out;
}

struct Object {
    std::vector<unsigned char> data;
    LibreSslAsn1Object object {};

    explicit Object(std::vector<unsigned char> bytes, int nid = 0) : data(std::move(bytes)) {
        object = {nullptr, nullptr, nid, static_cast<std::int32_t>(data.size()), data.empty() ? nullptr : data.data(), 0};
    }

    Object(Object&& other) noexcept : data(std::move(other.data)), object(other.object) {
        object.data = data.empty() ? nullptr : data.data();
    }

    Object& operator=(Object&& other) noexcept {
        data = std::move(other.data);
        object = other.object;
        object.data = data.empty() ? nullptr : data.data();
        return *this;
    }
};

int SignatureNid(const LibreSslAsn1Object* algorithm) {
    LibreSsl3X509Algor algor {const_cast<LibreSslAsn1Object*>(algorithm), nullptr};
    LibreSsl3Certificate certificate {};
    certificate.sigAlg = &algor;
    return X509_get_signature_nid_nid_postfix(&certificate);
}

int SignatureNid(const std::vector<unsigned char>& data) {
    const Object object(data);
    return SignatureNid(&object.object);
}

void CheckPubkey() {
    LibreSsl3X509Pubkey key {};
    LibreSsl3CertificateInfo info {};
    std::memset(&info, 0xa5, sizeof(info));
    info.key = &key;
    LibreSsl3Certificate certificate {};
    std::memset(&certificate, 0x5a, sizeof(certificate));
    certificate.certInfo = &info;
    Require(X509_get_X509_PUBKEY_nid_postfix(&certificate) == &key);
    info.key = nullptr;
    Require(X509_get_X509_PUBKEY_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = nullptr;
    RequireThrows([&] { X509_get_X509_PUBKEY_nid_postfix(&certificate); });
    RequireThrows([] { X509_get_X509_PUBKEY_nid_postfix(nullptr); });
}

void CheckSignatureNidInputs() {
    RequireThrows([] { X509_get_signature_nid_nid_postfix(nullptr); });
    LibreSsl3Certificate certificate {};
    RequireThrows([&] { X509_get_signature_nid_nid_postfix(&certificate); });
    Require(SignatureNid(static_cast<const LibreSslAsn1Object*>(nullptr)) == 0);
    const LibreSslAsn1Object empty {nullptr, nullptr, 668, 0, nullptr, 0};
    Require(SignatureNid(&empty) == 0);
    const unsigned char garbage[] {0xff, 0xff, 0xff};
    const LibreSslAsn1Object named {nullptr, nullptr, 668, 3, garbage, 0};
    Require(SignatureNid(&named) == 668);
    const LibreSslAsn1Object negative {nullptr, nullptr, 0, -1, nullptr, 0};
    Require(SignatureNid(&negative) == 0);
    const LibreSslAsn1Object longNull {nullptr, nullptr, 0, 200, nullptr, 0};
    Require(SignatureNid(&longNull) == 0);
    const LibreSslAsn1Object shortNull {nullptr, nullptr, 0, 9, nullptr, 0};
    RequireThrows([&] { SignatureNid(&shortNull); });
    const LibreSslAsn1Object hugeNegative {nullptr, nullptr, 0, INT_MIN, nullptr, 0};
    RequireThrows([&] { SignatureNid(&hugeNegative); });
}

void CheckEveryBuiltInObject() {
    int found = 0;
    for (int nid = 1; nid < LibreSslObjectCount; ++nid) {
        const LibreSslAsn1Object& entry = LibreSslObjects_nid_no_patch[nid];
        if (entry.length == 0) continue;
        const int expected = nid == 379 ? 676 : nid;
        Require(SignatureNid(std::vector<unsigned char>(entry.data, entry.data + entry.length)) == expected);
        ++found;
    }
    Require(found == 884);
    int later = 0;
    for (const auto& name : LaterNames) {
        if (!*name.oid) continue;
        Require(SignatureNid(Encode(name.oid)) == name.nid);
        ++later;
    }
    Require(later == 87);
    Require(SignatureNid(Encode("1.2.840.113549.1.1.11")) == 668);
    Require(SignatureNid(Encode("1.3.101.112")) == 952);
    Require(SignatureNid(Encode("1.3.6.1.5.5.7.1.31")) == 1053);
    Require(SignatureNid(Encode("1.2.840.113549.1.1")) == 186);
    Require(SignatureNid(Encode("1.3")) == 676);
}

void CheckUnknownObjects() {
    Require(SignatureNid(Encode("1.2.3.4.5.6.7.8.9")) == 0);
    Require(SignatureNid(Encode("1.2.840.113549.1.1.11.1")) == 0);
    Require(SignatureNid(Encode("1.2.840.113549.1.1.99")) == 0);
    Require(SignatureNid(std::vector<unsigned char> {0xff, 0xff, 0xff}) == 0);
    Require(SignatureNid(std::vector<unsigned char> {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0xff}) == 0);
}

void CheckAddedObjects() {
    const int first = OBJ_create_nid_postfix("1.3.6.1.4.1.99999.1", "aps5-first", "AnyPS5 first");
    Require(first > 0);
    Require(SignatureNid(Encode("1.3.6.1.4.1.99999.1")) == first);
    const int shadow = OBJ_create_nid_postfix("1.2.840.113549.1.1.11", "aps5-shadow", "AnyPS5 shadow");
    Require(shadow > 0 && shadow != 668);
    Require(SignatureNid(Encode("1.2.840.113549.1.1.11")) == shadow);
    Require(SignatureNid(Encode("1.3.101.112")) == 952);
    Require(SignatureNid(Encode("1.3.6.1.5.5.7.1.31")) == 1053);
    Require(SignatureNid(Encode("1.2.3.4.5.6.7.8.9")) == 0);
    const LibreSslAsn1Object longNull {nullptr, nullptr, 0, 200, nullptr, 0};
    RequireThrows([&] { SignatureNid(&longNull); });
    const LibreSslAsn1Object negative {nullptr, nullptr, 0, -1, nullptr, 0};
    RequireThrows([&] { SignatureNid(&negative); });
    const std::vector<unsigned char> largest(2047, 0x2a);
    Require(SignatureNid(largest) == 0);
    const std::vector<unsigned char> tooLarge(2048, 0x2a);
    RequireThrows([&] { SignatureNid(tooLarge); });
    OBJ_cleanup_nid_postfix();
    Require(SignatureNid(tooLarge) == 0);
    Require(SignatureNid(&negative) == 0);
    Require(SignatureNid(Encode("1.2.840.113549.1.1.11")) == 668);
    Require(SignatureNid(Encode("1.3.6.1.4.1.99999.1")) == 0);
    Require(SignatureNid(&longNull) == 0);
}

struct Extensions {
    std::vector<Object> objects;
    std::vector<LibreSsl3X509Extension> extensions;
    std::vector<char*> slots;
    LibreSsl3Stack stack {};
    LibreSsl3CertificateInfo info {};
    LibreSsl3Certificate certificate {};

    explicit Extensions(std::vector<Object> list) : objects(std::move(list)) {
        extensions.reserve(objects.size());
        for (auto& object : objects) extensions.push_back({&object.object, 0, nullptr});
        for (auto& extension : extensions) slots.push_back(reinterpret_cast<char*>(&extension));
        stack = {static_cast<int>(slots.size()), slots.data(), 0, static_cast<int>(slots.size()), nullptr};
        std::memset(&info, 0xa5, sizeof(info));
        info.extensions = &stack;
        certificate.certInfo = &info;
    }

    int Find(int nid, int lastpos) const {
        return X509_get_ext_by_NID_nid_postfix(&certificate, nid, lastpos);
    }
};

std::vector<Object> List(std::initializer_list<const char*> oids) {
    std::vector<Object> list;
    for (const char* oid : oids) list.emplace_back(*oid ? Encode(oid) : std::vector<unsigned char> {});
    return list;
}

void CheckExtensionSearch() {
    std::vector<Object> list = List({"2.5.29.19", "2.5.29.15", "2.5.29.19", "1.2.3.4.5", "", "1.3.6.1.5.5.7.1.31"});
    list.emplace_back(Encode("2.5.29.17"), 83);
    Extensions certificate(std::move(list));
    Require(certificate.Find(87, -1) == 0);
    Require(certificate.Find(87, 0) == 2);
    Require(certificate.Find(87, 2) == -1);
    Require(certificate.Find(87, -5) == 0);
    Require(certificate.Find(87, INT_MIN) == 0);
    Require(certificate.Find(87, 100) == -1);
    Require(certificate.Find(83, -1) == 1);
    Require(certificate.Find(83, 1) == -1);
    Require(certificate.Find(85, -1) == 6);
    Require(certificate.Find(1053, -1) == 5);
    Require(certificate.Find(0, -1) == 4);
    Require(certificate.Find(1022, -1) == 4);
    Require(certificate.Find(1022, 4) == -1);
    Require(certificate.Find(1, -1) == -1);
    RequireThrows([&] { certificate.Find(87, INT_MAX); });
    certificate.stack.num = 2;
    Require(certificate.Find(87, 0) == -1);
    certificate.stack.num = -1;
    Require(certificate.Find(87, -1) == -1);
    certificate.stack.num = 0;
    Require(certificate.Find(87, -1) == -1);
}

void CheckUnknownNids() {
    const Extensions certificate({});
    ERR_clear_error_nid_postfix();
    for (const int nid : {-1, INT_MIN, INT_MAX, 102, 118, 350, 511, 772, 774, 775, 1056}) {
        Require(certificate.Find(nid, -1) == -2);
        Require(certificate.Find(nid, INT_MAX) == -2);
        Require(ERR_get_error_nid_postfix() == UnknownNidError);
        Require(ERR_get_error_nid_postfix() == UnknownNidError);
        Require(ERR_get_error_nid_postfix() == 0);
    }
    Require(certificate.Find(1055, -1) == -1);
    Require(ERR_get_error_nid_postfix() == 0);
}

void CheckExtensionInputs() {
    RequireThrows([] { X509_get_ext_by_NID_nid_postfix(nullptr, 87, -1); });
    RequireThrows([] { X509_get_ext_by_NID_nid_postfix(nullptr, -1, -1); });
    LibreSsl3Certificate empty {};
    RequireThrows([&] { X509_get_ext_by_NID_nid_postfix(&empty, 87, -1); });
    LibreSsl3CertificateInfo info {};
    empty.certInfo = &info;
    Require(X509_get_ext_by_NID_nid_postfix(&empty, 87, -1) == -1);
    Extensions certificate(List({"2.5.29.19"}));
    certificate.slots[0] = nullptr;
    RequireThrows([&] { certificate.Find(87, -1); });
    certificate.slots[0] = reinterpret_cast<char*>(&certificate.extensions[0]);
    certificate.extensions[0].object = nullptr;
    RequireThrows([&] { certificate.Find(87, -1); });
    certificate.extensions[0].object = &certificate.objects[0].object;
    certificate.stack.data = nullptr;
    RequireThrows([&] { certificate.Find(87, -1); });
    certificate.stack.data = certificate.slots.data();
    certificate.objects[0].object.data = nullptr;
    RequireThrows([&] { certificate.Find(87, -1); });
    certificate.objects[0].object.length = -3;
    Require(certificate.Find(87, -1) == -1);
}

void CheckAddedNids() {
    const int first = OBJ_create_nid_postfix("1.3.6.1.4.1.99999.2", "aps5-added", "AnyPS5 added");
    Require(first >= LibreSslObjectCount && first < 1056);
    const LaterName& builtIn = LaterNames[first - LibreSslObjectCount];
    Require(builtIn.nid == first && *builtIn.oid);
    Extensions certificate(List({"1.3.6.1.4.1.99999.2", builtIn.oid}));
    Require(certificate.Find(first, -1) == 1);
    std::string last;
    int nid = first;
    while (nid < 1056) {
        last = "1.3.6.1.4.1.99999.3." + std::to_string(nid);
        nid = OBJ_create_nid_postfix(last.c_str(), nullptr, nullptr);
    }
    Require(nid == 1056);
    certificate.objects[0] = Object(Encode(last));
    certificate.extensions[0].object = &certificate.objects[0].object;
    Require(certificate.Find(1056, -1) == 0);
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(certificate.Find(1056, -1) == -2);
    Require(ERR_get_error_nid_postfix() == UnknownNidError);
}

void CheckRepeatable() {
    for (int round = 0; round < 3; ++round) {
        Require(SignatureNid(Encode("1.2.840.113549.1.1.11")) == 668);
        Require(SignatureNid(Encode("1.3")) == 676);
    }
}

} // namespace

int main() {
    CheckPubkey();
    CheckSignatureNidInputs();
    CheckEveryBuiltInObject();
    CheckUnknownObjects();
    CheckAddedObjects();
    CheckExtensionSearch();
    CheckUnknownNids();
    CheckExtensionInputs();
    CheckAddedNids();
    CheckRepeatable();
    return 0;
}
