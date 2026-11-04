#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr std::int64_t NoFreeFlag = 0x10;
constexpr std::int32_t ObjectType = 6;
constexpr std::int32_t BooleanType = 1;
constexpr std::int32_t NullType = 5;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void ClearAndFree(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
    free_nid_postfix(data);
}

void FreeString(Asn1String* value) {
    if (!value) return;
    if (value->data && !(value->flags & NoFreeFlag)) {
        if (value->length < 0) NotImplemented_nid_no_patch(__func__);
        ClearAndFree(value->data, static_cast<std::size_t>(value->length));
    }
    value->data = nullptr;
    value->length = 0;
    value->flags &= ~NoFreeFlag;
    free_nid_postfix(value);
}

void FreeObject(LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & 0x04) {
        free_nid_postfix(const_cast<char*>(object->shortName));
        free_nid_postfix(const_cast<char*>(object->longName));
        object->shortName = nullptr;
        object->longName = nullptr;
    }
    if (object->flags & 0x08) {
        if (object->length < 0) NotImplemented_nid_no_patch(__func__);
        ClearAndFree(const_cast<unsigned char*>(object->data), static_cast<std::size_t>(object->length));
        object->data = nullptr;
        object->length = 0;
    }
    if (object->flags & 0x01) free_nid_postfix(object);
}

}

extern "C" {

void APS5_VABI ASN1_TYPE_free_nid_postfix(Asn1Type* value) {
    if (!value) return;
    void* element = value->value.pointer;
    if (element) {
        if (value->type == ObjectType) {
            FreeObject(static_cast<LibreSslAsn1Object*>(element));
        } else if (value->type != BooleanType && value->type != NullType) {
            FreeString(static_cast<Asn1String*>(element));
        }
    }
    free_nid_postfix(value);
}

EcdsaSig* APS5_VABI ECDSA_SIG_new_nid_postfix(void) {
    auto* signature = static_cast<EcdsaSig*>(malloc_nid_postfix(sizeof(EcdsaSig)));
    if (!signature) {
        PutError(MallocFailure);
        return nullptr;
    }
    signature->r = nullptr;
    signature->s = nullptr;
    return signature;
}

int APS5_VABI ECDSA_SIG_set0_nid_postfix(EcdsaSig* signature, Bignum* r, Bignum* s) {
    if (!signature) throw std::invalid_argument(std::string(__func__) + ": null signature");
    if (!r || !s) return 0;
    BN_free_nid_postfix(signature->r);
    BN_free_nid_postfix(signature->s);
    signature->r = r;
    signature->s = s;
    return 1;
}

void APS5_VABI ECDSA_SIG_free_nid_postfix(EcdsaSig* signature) {
    if (!signature) return;
    BN_free_nid_postfix(signature->r);
    BN_free_nid_postfix(signature->s);
    free_nid_postfix(signature);
}

}
