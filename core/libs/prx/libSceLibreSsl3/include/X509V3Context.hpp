#pragma once

#include <cstddef>
#include <cstdint>

struct LibreSslX509;

struct LibreSsl3X509V3Context {
    std::int32_t flags;
    LibreSslX509* issuerCertificate;
    LibreSslX509* subjectCertificate;
    void* subjectRequest;
    void* crl;
    void* database;
};

static_assert(sizeof(LibreSsl3X509V3Context) == 48);
static_assert(offsetof(LibreSsl3X509V3Context, issuerCertificate) == 8);
static_assert(offsetof(LibreSsl3X509V3Context, subjectCertificate) == 16);
static_assert(offsetof(LibreSsl3X509V3Context, subjectRequest) == 24);
static_assert(offsetof(LibreSsl3X509V3Context, crl) == 32);
static_assert(offsetof(LibreSsl3X509V3Context, database) == 40);
