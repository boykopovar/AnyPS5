#pragma once

#include <cstddef>
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"

namespace LibreSsl {

struct PemBuffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

static_assert(sizeof(PemBuffer) == 24);

void FreePemBuffer(PemBuffer* buffer);

struct PemBuffers {
    PemBuffer* name = nullptr;
    PemBuffer* header = nullptr;
    PemBuffer* data = nullptr;

    void Clear() {
        FreePemBuffer(name);
        FreePemBuffer(header);
        FreePemBuffer(data);
        name = header = data = nullptr;
    }
    ~PemBuffers() { Clear(); }
};

struct Pkcs8Key {
    int algorithm;
    int keyType;
    const unsigned char* key;
    long keyLength;
};

void PutPemError(int reason);
int PemCheckSuffix(const char* name, const char* suffix);
int OldPrivateKeyType(const char* name, int length);
bool ReadPemBytes(Bio* bio, const char* name, PemBuffers& record, long& length, const char* function);
LibreSslX509* ReadPemRecord(Bio* bio, LibreSslX509** certificate, LibreSslPemPasswordCallback callback,
                            void* userData, const char* name, const char* function);
LibreSslRsa* DecodeRsaPrivateKey(const unsigned char** input, long length, const char* function);
bool DecodePkcs8(const unsigned char** input, long length, Pkcs8Key& key, const char* function);

}
