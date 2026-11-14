#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ASN1DER_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ASN1DER_HPP

#include <cstdint>
#include <vector>
#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSslDer {

struct Header {
    std::int64_t length;
    bool indefinite;
    bool constructed;
};

bool Fail(int reason);
int ReadIdentifier(const unsigned char*& in, std::int64_t& length, int& tag, int& objectClass, std::int64_t available);
bool CheckHeader(Header& header, const unsigned char*& in, std::int64_t available, int expectedTag, int expectedClass);
bool CheckEoc(const unsigned char*& in, std::int64_t length);
bool Collect(std::vector<unsigned char>& buffer, const unsigned char*& in, std::int64_t length, bool indefinite, int depth);
Asn1String* NewString(int type);
bool DecodeInteger(Asn1String*& slot, const unsigned char* content, std::int64_t length, int type);
bool DecodeObject(LibreSslAsn1Object*& slot, const unsigned char* content, std::int64_t length);
bool DecodeOctetString(Asn1String*& slot, const unsigned char* content, std::int64_t length, unsigned char* collected, int type);

}

#endif
