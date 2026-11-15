#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"

namespace LibreSsl {

struct PemBuffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

struct PemBuffers {
    PemBuffer* name = nullptr;
    PemBuffer* header = nullptr;
    PemBuffer* data = nullptr;

    bool Allocate();
    void Clear();
    ~PemBuffers();
};

bool ReadRecord(LibreSsl::Bio* bio, PemBuffers& buffers, std::size_t& decodedLength);
bool CheckPem(const char* name, const char* expected);
bool CipherInfo(const char* header);
void PutPemError(int reason);

LibreSslX509* ReadPemRecord(LibreSsl::Bio* bio, LibreSslX509** certificate,
                            LibreSslPemPasswordCallback callback, void* userData, const char* function,
                            const char* expectedLabel);

}
