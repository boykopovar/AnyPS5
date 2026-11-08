#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int HeaderTooLong = 123;
constexpr int TooLong = 155;
constexpr int Constructed = 0x20;
constexpr int Private = 0xc0;
constexpr int PrimitiveTag = 0x1f;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

bool GetLength(const unsigned char** cursor, int* indefinite, std::int64_t* length, std::int64_t max) {
    const unsigned char* pointer = *cursor;
    std::uint64_t result = 0;
    if (max-- < 1) return false;
    if (*pointer == 0x80) {
        *indefinite = 1;
        ++pointer;
    } else {
        *indefinite = 0;
        unsigned int count = *pointer & 0x7f;
        if (*(pointer++) & 0x80) {
            if (max < static_cast<std::int64_t>(count)) return false;
            while (count && *pointer == 0) {
                ++pointer;
                --count;
            }
            if (count > 8) return false;
            while (count-- > 0) {
                result <<= 8;
                result |= *(pointer++);
            }
        } else {
            result = count;
        }
    }
    if (result > static_cast<std::uint64_t>(INT64_MAX)) return false;
    *cursor = pointer;
    *length = static_cast<std::int64_t>(result);
    return true;
}

}

extern "C" {

int APS5_VABI ASN1_get_object_nid_postfix(const unsigned char** pp, std::int64_t* plength, int* ptag, int* pclass, std::int64_t omax) {
    if (!pp || !plength || !ptag || !pclass) throw std::invalid_argument(std::string(__func__) + ": null output");
    if (omax < 0) throw std::invalid_argument(std::string(__func__) + ": negative input length");
    const unsigned char* pointer = *pp;
    if (omax > 0 && !pointer) throw std::invalid_argument(std::string(__func__) + ": null input");
    std::int64_t max = omax;
    if (!max) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    int result = *pointer & Constructed;
    const int objectClass = *pointer & Private;
    int tag = *pointer & PrimitiveTag;
    if (tag == PrimitiveTag) {
        ++pointer;
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
        std::int64_t value = 0;
        while (*pointer & 0x80) {
            value <<= 7;
            value |= *(pointer++) & 0x7f;
            if (--max == 0) {
                PutError(HeaderTooLong);
                return 0x80;
            }
            if (value > (INT_MAX >> 7)) {
                PutError(HeaderTooLong);
                return 0x80;
            }
        }
        value <<= 7;
        value |= *(pointer++) & 0x7f;
        tag = static_cast<int>(value);
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
    } else {
        ++pointer;
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
    }
    *ptag = tag;
    *pclass = objectClass;
    int indefinite = 0;
    if (!GetLength(&pointer, &indefinite, plength, max)) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    if (indefinite && !(result & Constructed)) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    if (*plength > omax - static_cast<std::int64_t>(pointer - *pp)) {
        PutError(TooLong);
        result |= 0x80;
    }
    *pp = pointer;
    return result | indefinite;
}

}
