/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/x509/x509_d2.c (v 1.10) L87-107;
 * src/lib/libcrypto/x509/x509_lu.c (v 1.23) L123-132; src/lib/libcrypto/x509/by_file.c (v 1.21) L92-118 and L237-266;
 * src/lib/libcrypto/pem/pem_info.c (v 1.22) L95-305; src/lib/libcrypto/asn1/x_info.c (v 1.17) L66-107;
 * src/lib/libcrypto/bio/bss_file.c (v 1.32) L113-137 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000-2004 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
#include "StoreLoad.hpp"
#include "PemReadBio.hpp"
#include "X509Aux.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int SystemLibrary = 2;
constexpr int PemLibrary = 9;
constexpr int X509Library = 11;
constexpr int Asn1Library = 13;
constexpr int BioLibrary = 32;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int SystemLibraryFailure = 2;
constexpr int PemLibraryFailure = 9;
constexpr int Asn1LibraryFailure = 13;
constexpr int MissingFile = 128;
constexpr int NotFoundError = 2;
constexpr int FileLoadCommand = 1;
constexpr int PemFileType = 1;
constexpr int DefaultFileType = 3;
constexpr int NoStartLine = 108;
constexpr int CloseOnFree = 1;
constexpr int MinimumNodes = 4;

constexpr const char* CertificateLabel = "CERTIFICATE";
constexpr const char* OldCertificateLabel = "X509 CERTIFICATE";
constexpr const char* TrustedCertificateLabel = "TRUSTED CERTIFICATE";
constexpr const char* CertificateListLabel = "X509 CRL";
constexpr const char* RsaPrivateKeyLabel = "RSA PRIVATE KEY";
constexpr const char* DsaPrivateKeyLabel = "DSA PRIVATE KEY";
constexpr const char* EcPrivateKeyLabel = "EC PRIVATE KEY";

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    int (APS5_VABI* compare)(const void*, const void*);
};

struct X509Info {
    LibreSslX509* certificate;
    void* crl;
    void* privateKey;
    const void* cipher;
    unsigned char iv[16];
    std::int32_t encryptedLength;
    char* encryptedData;
    std::int32_t references;
};

static_assert(sizeof(X509Info) == 72 && offsetof(X509Info, crl) == 8 && offsetof(X509Info, privateKey) == 16);
static_assert(offsetof(X509Info, iv) == 32 && offsetof(X509Info, encryptedLength) == 48);
static_assert(offsetof(X509Info, encryptedData) == 56 && offsetof(X509Info, references) == 64);
static_assert(sizeof(Stack) == 32 && offsetof(Stack, data) == 8 && offsetof(Stack, numAlloc) == 20);

using LookupControl = int (APS5_VABI*)(LibreSslX509Lookup*, int, const char*, std::int64_t, char**);

int Reason(std::uint64_t code) {
    return static_cast<int>(code & 0xfff);
}

void PutX509Error(int reason) {
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, reason, __FILE__, __LINE__);
}

Stack* NewStack() {
    auto* const stack = static_cast<Stack*>(malloc_nid_postfix(sizeof(Stack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    std::fill_n(stack->data, MinimumNodes, nullptr);
    stack->num = 0;
    stack->sorted = 0;
    stack->numAlloc = MinimumNodes;
    stack->compare = nullptr;
    return stack;
}

bool PushElement(Stack* stack, void* element) {
    if (stack->numAlloc <= stack->num + 1) {
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

void FreeStack(Stack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

int ElementCount(const Stack* stack) {
    return stack->num;
}

X509Info* InfoAt(const Stack* stack, int index) {
    return reinterpret_cast<X509Info*>(stack->data[index]);
}

X509Info* InfoNew() {
    auto* const info = static_cast<X509Info*>(malloc_nid_postfix(sizeof(X509Info)));
    if (!info) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure, __FILE__, __LINE__);
        return nullptr;
    }
    info->certificate = nullptr;
    info->crl = nullptr;
    info->privateKey = nullptr;
    info->cipher = nullptr;
    info->encryptedLength = 0;
    info->encryptedData = nullptr;
    info->references = 1;
    return info;
}

void InfoFree(X509Info* info) {
    if (!info) return;
    if (--info->references > 0) return;
    if (info->crl) NotImplemented_nid_no_patch("X509_INFO_free of an X509 CRL");
    if (info->privateKey) NotImplemented_nid_no_patch("X509_INFO_free of a private key");
    X509_free_nid_postfix(info->certificate);
    free_nid_postfix(info->encryptedData);
    free_nid_postfix(info);
}

LibreSsl::Bio* NewFileBio(const char* file) {
    auto* const stream = fopen_nid_postfix(file, "r");
    if (!stream) {
        const int openError = errno;
        LibreSslPutError_nid_no_patch(SystemLibrary, AnyFunction, openError, __FILE__, __LINE__);
        LibreSslPutError_nid_no_patch(BioLibrary, AnyFunction, openError == NotFoundError ? MissingFile : SystemLibraryFailure,
                                      __FILE__, __LINE__);
        return nullptr;
    }
    auto* const bio = BIO_new_fp_nid_postfix(stream, CloseOnFree);
    if (!bio) {
        fclose_nid_postfix(stream);
        return nullptr;
    }
    return bio;
}

void ReleaseInfos(Stack* infos) {
    for (int index = 0; index < ElementCount(infos); ++index)
        InfoFree(InfoAt(infos, index));
    FreeStack(infos);
}

Stack* ReadInfoBio(LibreSsl::Bio* bio, Stack* list) {
    Stack* result = list;
    if (!result) {
        result = NewStack();
        if (!result) {
            LibreSsl::PutPemError(MallocFailure);
            return nullptr;
        }
    }
    const auto release = [&] {
        for (int index = 0; index < ElementCount(result); ++index)
            InfoFree(InfoAt(result, index));
        if (result != list) FreeStack(result);
        return nullptr;
    };
    X509Info* info = InfoNew();
    if (!info) return release();
    try {
        for (;;) {
            LibreSsl::PemBuffers buffers;
            long length = 0;
            if (!LibreSsl::ReadPem(bio, buffers, length)) {
                if (Reason(LibreSslPeekLastError_nid_no_patch()) == NoStartLine) {
                    ERR_clear_error_nid_postfix();
                    break;
                }
                InfoFree(info);
                info = nullptr;
                return release();
            }
            for (;;) {
                const char* name = buffers.name->data;
                const bool trusted = std::strcmp(name, TrustedCertificateLabel) == 0;
                if (trusted || std::strcmp(name, CertificateLabel) == 0 || std::strcmp(name, OldCertificateLabel) == 0) {
                    if (info->certificate) {
                        if (!PushElement(result, info)) {
                            InfoFree(info);
                            info = nullptr;
                            return release();
                        }
                        info = InfoNew();
                        if (!info) return release();
                        continue;
                    }
                    if (!LibreSsl::ReadCipherInfo(buffers.header->data, "PEM_X509_INFO_read_bio")) {
                        InfoFree(info);
                        info = nullptr;
                        return release();
                    }
                    const unsigned char* input = reinterpret_cast<const unsigned char*>(buffers.data->data);
                    LibreSslX509* read =
                        trusted ? LibreSsl::X509AuxRead_nid_no_patch(&info->certificate, &input, static_cast<std::int64_t>(length))
                                : d2i_X509_nid_postfix(&info->certificate, &input, static_cast<std::int64_t>(length));
                    if (!read) {
                        LibreSsl::PutPemError(Asn1LibraryFailure);
                        InfoFree(info);
                        info = nullptr;
                        return release();
                    }
                    info->certificate = read;
                    break;
                }
                if (std::strcmp(name, CertificateListLabel) == 0) NotImplemented_nid_no_patch("PEM_X509_INFO_read_bio of an X509 CRL");
                if (std::strcmp(name, RsaPrivateKeyLabel) == 0 || std::strcmp(name, DsaPrivateKeyLabel) == 0 ||
                    std::strcmp(name, EcPrivateKeyLabel) == 0) NotImplemented_nid_no_patch("PEM_X509_INFO_read_bio of a private key");
                break;
            }
        }
    } catch (...) {
        if (info) try { InfoFree(info); } catch (...) {}
        for (int index = 0; index < ElementCount(result); ++index)
            InfoFree(InfoAt(result, index));
        if (result != list) FreeStack(result);
        throw;
    }
    if (info->certificate || info->crl || info->privateKey || info->encryptedData) {
        if (!PushElement(result, info)) {
            InfoFree(info);
            return release();
        }
        info = nullptr;
    }
    InfoFree(info);
    return result;
}

int LoadCertCrlFile(LibreSslX509Lookup* lookup, const char* file) {
    auto* const input = NewFileBio(file);
    if (!input) {
        PutX509Error(SystemLibraryFailure);
        return 0;
    }
    Stack* infos = nullptr;
    try {
        infos = ReadInfoBio(input, nullptr);
    } catch (...) {
        BIO_free_nid_postfix(input);
        throw;
    }
    BIO_free_nid_postfix(input);
    if (!infos) {
        PutX509Error(PemLibraryFailure);
        return 0;
    }
    int count = 0;
    try {
        for (int index = 0; index < ElementCount(infos); ++index) {
            X509Info* const item = InfoAt(infos, index);
            if (item->certificate) {
                X509_STORE_add_cert_nid_postfix(lookup->storeContext, item->certificate);
                ++count;
            }
            if (item->crl) NotImplemented_nid_no_patch("X509_load_cert_crl_file: a certificate revocation list in the file");
        }
    } catch (...) {
        try { ReleaseInfos(infos); } catch (...) {}
        throw;
    }
    ReleaseInfos(infos);
    return count;
}

int LookupCtrl(LibreSslX509Lookup* lookup, int command, const char* argument, std::int64_t type, char** result) {
    if (!lookup->method) return -1;
    if (lookup->method->ctrl)
        return reinterpret_cast<LookupControl>(lookup->method->ctrl)(lookup, command, argument, type, result);
    return 1;
}

}  // namespace

namespace LibreSsl {

int ByFileCtrl_nid_no_patch(LibreSslX509Lookup* lookup, int command, const char* argument, std::int64_t type, char** result) {
    int ok = 0;
    switch (command) {
    case FileLoadCommand:
        if (type == DefaultFileType) {
            NotImplemented_nid_no_patch("X509_LOOKUP_file: the default certificate file");
        } else if (type == PemFileType) {
            if (!lookup) throw std::invalid_argument(std::string(__func__) + ": null lookup");
            ok = LoadCertCrlFile(lookup, argument) != 0;
        } else {
            NotImplemented_nid_no_patch("X509_LOOKUP_file: a certificate file that is not PEM");
        }
        break;
    default:
        break;
    }
    return ok;
}

int StoreLoadLocations_nid_no_patch(LibreSslX509Store* store, const char* file, const char* path) {
    if (file) {
        LibreSslX509Lookup* const lookup = X509_STORE_add_lookup_nid_postfix(store, X509_LOOKUP_file_nid_postfix());
        if (!lookup) return 0;
        if (LookupCtrl(lookup, FileLoadCommand, file, PemFileType, nullptr) != 1) return 0;
    }
    if (path) NotImplemented_nid_no_patch("X509_STORE_load_locations: a directory of certificates");
    if (!file && !path) return 0;
    return 1;
}

}
