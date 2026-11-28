#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Cmac.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <mbedtls/aes.h>

struct LibreSslCmac {
    LibreSslCipherContext cipherContext;
    unsigned char firstSubkey[32];
    unsigned char secondSubkey[32];
    unsigned char previousBlock[32];
    unsigned char lastBlock[32];
    std::int32_t lastLength;
};

static_assert(sizeof(LibreSslCmac) == 304);

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void Cleanse(void* data, std::size_t length) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    while (length-- != 0) *bytes++ = 0;
}

template <typename T>
struct Cleansed {
    T value{};
    ~Cleansed() { Cleanse(&value, sizeof(value)); }
};

struct CleansedBytes {
    std::vector<unsigned char> data;
    explicit CleansedBytes(std::size_t size = 0) : data(size) {}
    ~CleansedBytes() {
        if (!data.empty()) Cleanse(data.data(), data.size());
    }
};

void RequireReadable(const void* source, std::size_t length, const char* message) {
    if (length == 0) return;
    ErrnoGuard guard;
    if (source == nullptr) throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, length);
    if (!GuestReadable_nid_no_patch(source, length)) throw std::invalid_argument(message);
}

void CopyIn(void* destination, const void* source, std::size_t length, const char* message) {
    if (length == 0) return;
    ErrnoGuard guard;
    if (source == nullptr) throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, length);
    if (!GuestReadable_nid_no_patch(source, length) || !GuestCopyIn_nid_no_patch(destination, source, length))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t length, const char* message)
        : guard(pointer ? pointer : nullptr, pointer ? length : 0) {
        if (length == 0) return;
        if (pointer == nullptr || !guard.Open() || !GuestWritable_nid_no_patch(pointer, length))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    ErrnoGuard saved;
    GuestArena::HostWrite guard;
};

void RequireWritable(void* destination, std::size_t length, const char* message) {
    GuestWriteScope scope(destination, length, message);
}

void CopyOut(void* destination, const void* source, std::size_t length, const char* message) {
    if (length == 0) return;
    GuestWriteScope writeScope(destination, length, message);
    if (!GuestCopyOut_nid_no_patch(destination, source, length))
        throw std::invalid_argument(message);
}

LibreSslCipher ReadCipher(const LibreSslCipher* cipher, const char* message) {
    if (cipher == nullptr) throw std::invalid_argument(message);
    LibreSslCipher copy{};
    CopyIn(&copy, cipher, sizeof(copy), message);
    return copy;
}

int Error(int reason) {
    LibreSslPutError_nid_no_patch(6, 0xfff, reason, __FILE__, __LINE__);
    return 0;
}

void RequireContext(const LibreSslCmac* context) {
    if (context == nullptr) throw std::invalid_argument("CMAC: null context");
}

bool ValidBlockSize(std::int32_t blockSize) {
    return blockSize == 1 || blockSize == 8 || blockSize == 16;
}

int Cleanup(LibreSslCipherContext& context) {
    if (context.engine != nullptr) NotImplemented_nid_no_patch(__func__);
    if (context.cipher != nullptr) {
        const auto method = ReadCipher(context.cipher, "CMAC: unreadable cipher");
        if (method.contextSize < 0) throw std::invalid_argument("CMAC: invalid cipher dimensions");
        if (method.cleanup != nullptr && !method.cleanup(&context)) return 0;
        if (context.cipherData != nullptr && method.contextSize != 0) {
            const auto size = static_cast<std::size_t>(method.contextSize);
            GuestWriteScope dataWrite(context.cipherData, size, "CMAC: unwritable cipher data");
            Cleanse(context.cipherData, size);
        }
    }
    free_nid_postfix(context.cipherData);
    Cleanse(&context, sizeof(context));
    return 1;
}

int Control(LibreSslCipherContext& context, int command, int argument) {
    if (context.cipher == nullptr) return Error(131);
    const auto method = ReadCipher(context.cipher, "CMAC: unreadable cipher");
    if (method.control == nullptr) return Error(132);
    const int result = method.control(&context, command, argument, nullptr);
    return result == -1 ? Error(133) : result;
}

int InitializeCipher(LibreSslCipherContext& context, const LibreSslCipher* cipher, const unsigned char* key, const unsigned char* iv, void* engine) {
    context.encrypt = 1;
    if (context.engine != nullptr || (cipher != nullptr && engine != nullptr)) NotImplemented_nid_no_patch(__func__);
    if (cipher != nullptr) {
        const auto incoming = ReadCipher(cipher, "CMAC: unreadable cipher");
        if (incoming.contextSize < 0 || incoming.ivLength < 0) throw std::invalid_argument("CMAC: invalid cipher dimensions");
        if (context.cipher != nullptr) {
            const auto flags = context.flags;
            if (!Cleanup(context)) return 0;
            context.encrypt = 1;
            context.flags = flags;
        }
        context.cipher = cipher;
        context.cipherData = nullptr;
        if (incoming.contextSize != 0) {
            context.cipherData = calloc_nid_postfix(1, incoming.contextSize);
            if (context.cipherData == nullptr) return Error(65);
        }
        context.keyLength = incoming.keyLength;
        context.flags &= 1;
        if ((incoming.flags & 0x40) != 0 && !Control(context, 0, 0)) return Error(134);
    }
    if (context.cipher == nullptr) return Error(131);
    const auto method = ReadCipher(context.cipher, "CMAC: unreadable cipher");
    if (!ValidBlockSize(method.blockSize)) return Error(136);
    const auto mode = method.flags & 0xf0007;
    if ((context.flags & 1) == 0 && mode == 0x10002) return Error(170);
    if ((method.flags & 0x10) == 0) {
        if (mode == 2 || mode == 3 || mode == 4 || mode == 5) {
            if (method.ivLength < 0 || method.ivLength > 16) return Error(102);
            if (mode != 2) context.position = 0;
            if (mode == 5) {
                if (iv != nullptr) std::memcpy(context.iv, iv, method.ivLength);
            } else {
                if (iv != nullptr) std::memcpy(context.originalIv, iv, method.ivLength);
                std::memcpy(context.iv, context.originalIv, method.ivLength);
            }
        } else if (mode != 0 && mode != 1) {
            return 0;
        }
    }
    if (key != nullptr || (method.flags & 0x20) != 0) {
        if (method.init == nullptr) NotImplemented_nid_no_patch(__func__);
        CleansedBytes keyCopy;
        const unsigned char* keyInput = key;
        if (key != nullptr && context.keyLength > 0) {
            keyCopy.data.resize(static_cast<std::size_t>(context.keyLength));
            CopyIn(keyCopy.data.data(), key, keyCopy.data.size(), "CMAC_Init: unreadable key");
            keyInput = keyCopy.data.data();
        }
        if (context.cipherData != nullptr && method.contextSize > 0) {
            const auto size = static_cast<std::size_t>(method.contextSize);
            RequireReadable(context.cipherData, size, "CMAC: unreadable cipher data");
            GuestWriteScope cipherWrite(context.cipherData, size, "CMAC: unwritable cipher data");
            if (!method.init(&context, keyInput, iv, 1)) return 0;
        } else if (!method.init(&context, keyInput, iv, 1)) {
            return 0;
        }
    }
    context.bufferLength = 0;
    context.finalUsed = 0;
    context.blockMask = method.blockSize - 1;
    return 1;
}

int SetKeyLength(LibreSslCipherContext& context, std::size_t length) {
    const auto method = ReadCipher(context.cipher, "CMAC: unreadable cipher");
    const int narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(length));
    if ((method.flags & 0x80) != 0) return Control(context, 1, narrowed);
    if (context.keyLength == narrowed) return 1;
    if (narrowed > 0 && (method.flags & 8) != 0) {
        context.keyLength = narrowed;
        return 1;
    }
    return Error(130);
}

int EncryptBlock(LibreSslCmac& context, const LibreSslCipher& method, unsigned char* output, const unsigned char* input) {
    if (method.cipher == nullptr) NotImplemented_nid_no_patch(__func__);
    if (context.cipherContext.cipherData != nullptr && method.contextSize > 0) {
        const auto size = static_cast<std::size_t>(method.contextSize);
        RequireReadable(context.cipherContext.cipherData, size, "CMAC: unreadable cipher data");
        GuestWriteScope cipherWrite(context.cipherContext.cipherData, size, "CMAC: unwritable cipher data");
        return method.cipher(&context.cipherContext, output, input, method.blockSize);
    }
    return method.cipher(&context.cipherContext, output, input, method.blockSize);
}

void DoubleBlock(unsigned char* output, const unsigned char* input, int length) {
    unsigned char carry = 0;
    for (int index = length; index-- > 0;) {
        const auto value = input[index];
        output[index] = static_cast<unsigned char>((value << 1) | carry);
        carry = value >> 7;
    }
    output[length - 1] ^= static_cast<unsigned char>((length == 16 ? 0x87 : 0x1b) & -static_cast<int>(carry));
}

int APS5_VABI AesInit(LibreSslCipherContext* context, const unsigned char* key, const unsigned char*, int encrypt) {
    if (key == nullptr || context->cipherData == nullptr) throw std::invalid_argument("CMAC AES: missing key storage");
    if (context->keyLength != 16 && context->keyLength != 24 && context->keyLength != 32) return Error(143);
    auto* aes = static_cast<mbedtls_aes_context*>(context->cipherData);
    mbedtls_aes_init(aes);
    const int result = encrypt ? mbedtls_aes_setkey_enc(aes, key, context->keyLength * 8) : mbedtls_aes_setkey_dec(aes, key, context->keyLength * 8);
    if (result != 0) throw std::runtime_error("CMAC AES: backend key setup failed");
    return 1;
}

int APS5_VABI AesCipher(LibreSslCipherContext* context, unsigned char* output, const unsigned char* input, std::size_t length) {
    if (context->cipherData == nullptr || (length != 0 && (output == nullptr || input == nullptr))) throw std::invalid_argument("CMAC AES: null data");
    if (length == 0) return 1;
    auto* aes = static_cast<mbedtls_aes_context*>(context->cipherData);
    const int mode = context->encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    const auto complete = length - length % 16;
    if (complete != 0 && mbedtls_aes_crypt_cbc(aes, mode, complete, context->iv, input, output) != 0)
        throw std::runtime_error("CMAC AES: backend encryption failed");
    const auto remaining = length - complete;
    if (remaining == 0) return 1;
    input += complete;
    output += complete;
    unsigned char block[16];
    unsigned char nextIv[16];
    if (context->encrypt) {
        std::memcpy(block, context->iv, 16);
        for (std::size_t index = 0; index < remaining; ++index) block[index] ^= input[index];
        if (mbedtls_aes_crypt_ecb(aes, mode, block, output) != 0) throw std::runtime_error("CMAC AES: backend encryption failed");
        std::memcpy(context->iv, output, 16);
    } else {
        std::memcpy(nextIv, input, 16);
        if (mbedtls_aes_crypt_ecb(aes, mode, input, block) != 0) throw std::runtime_error("CMAC AES: backend decryption failed");
        for (std::size_t index = 0; index < remaining; ++index) output[index] = block[index] ^ context->iv[index];
        std::memcpy(context->iv, nextIv, 16);
    }
    Cleanse(block, sizeof(block));
    Cleanse(nextIv, sizeof(nextIv));
    return 1;
}

const LibreSslCipher aes128{419, 16, 16, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};
const LibreSslCipher aes192{423, 16, 24, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};
const LibreSslCipher aes256{427, 16, 32, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};

}

extern "C" {

LibreSslCmac* APS5_VABI CMAC_CTX_new_nid_postfix() {
    auto* context = static_cast<LibreSslCmac*>(malloc_nid_postfix(sizeof(LibreSslCmac)));
    if (context != nullptr) {
        LibreSslCmac initial{};
        initial.lastLength = -1;
        CopyOut(context, &initial, sizeof(initial), "CMAC_CTX_new: unwritable context");
    }
    return context;
}

void APS5_VABI CMAC_CTX_free_nid_postfix(LibreSslCmac* context) {
    if (context == nullptr) return;
    Cleansed<LibreSslCmac> local;
    CopyIn(&local.value, context, sizeof(local.value), "CMAC_CTX_free: unreadable context");
    RequireWritable(context, sizeof(local.value), "CMAC_CTX_free: unwritable context");
    Cleanup(local.value.cipherContext);
    Cleanse(&local.value, sizeof(local.value));
    CopyOut(context, &local.value, sizeof(local.value), "CMAC_CTX_free: unwritable context");
    free_nid_postfix(context);
}

int APS5_VABI CMAC_Init_nid_postfix(LibreSslCmac* context, const void* key, std::size_t keyLength, const LibreSslCipher* cipher, void* engine) {
    RequireContext(context);
    Cleansed<LibreSslCmac> local;
    CopyIn(&local.value, context, sizeof(local.value), "CMAC_Init: unreadable context");
    RequireWritable(context, sizeof(local.value), "CMAC_Init: unwritable context");
    const unsigned char zero[32]{};
    if (key == nullptr && keyLength == 0 && cipher == nullptr && engine == nullptr) {
        if (local.value.lastLength == -1) return 0;
        if (!InitializeCipher(local.value.cipherContext, nullptr, nullptr, zero, nullptr)) return 0;
        const auto method = ReadCipher(local.value.cipherContext.cipher, "CMAC_Init: unreadable cipher");
        if (!ValidBlockSize(method.blockSize)) return Error(136);
        std::memset(local.value.previousBlock, 0, method.blockSize);
        local.value.lastLength = 0;
        CopyOut(context, &local.value, sizeof(local.value), "CMAC_Init: unwritable context");
        return 1;
    }
    bool stagedCustomKey = false;
    if (cipher != nullptr) {
        const auto incoming = ReadCipher(cipher, "CMAC_Init: unreadable cipher");
        if (key != nullptr) {
            if ((incoming.flags & 0x80) == 0) {
                const int narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(keyLength));
                if (narrowed > 0 && (incoming.keyLength == narrowed || (incoming.flags & 8) != 0))
                    RequireReadable(key, static_cast<std::size_t>(narrowed), "CMAC_Init: unreadable key");
            } else {
                stagedCustomKey = true;
            }
        }
    } else if (key != nullptr && local.value.cipherContext.cipher != nullptr) {
        const auto existing = ReadCipher(local.value.cipherContext.cipher, "CMAC_Init: unreadable cipher");
        if ((existing.flags & 0x80) == 0) {
            const int narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(keyLength));
            if (narrowed > 0 && (local.value.cipherContext.keyLength == narrowed || (existing.flags & 8) != 0))
                RequireReadable(key, static_cast<std::size_t>(narrowed), "CMAC_Init: unreadable key");
        }
    }
    struct SyncGuard {
        LibreSslCmac* target;
        LibreSslCmac* source;
        bool dirty = false;
        void Flush() {
            if (!dirty) return;
            dirty = false;
            CopyOut(target, source, sizeof(*source), "CMAC_Init: unwritable context");
        }
        ~SyncGuard() noexcept {
            if (!dirty) return;
            try {
                CopyOut(target, source, sizeof(*source), "CMAC_Init: unwritable context");
            } catch (...) {
            }
        }
    } sync{context, &local.value};
    if (stagedCustomKey) {
        if (local.value.cipherContext.engine != nullptr || engine != nullptr) NotImplemented_nid_no_patch(__func__);
        Cleansed<LibreSslCipherContext> staged;
        staged.value.flags = local.value.cipherContext.flags & 1;
        struct StagedGuard {
            LibreSslCipherContext* ctx;
            bool armed = true;
            ~StagedGuard() noexcept {
                if (!armed) return;
                try {
                    if (!Cleanup(*ctx) && ctx->cipherData != nullptr) {
                        if (ctx->cipher != nullptr) {
                            const auto method = ReadCipher(ctx->cipher, "CMAC: unreadable cipher");
                            if (method.contextSize > 0) {
                                const auto size = static_cast<std::size_t>(method.contextSize);
                                GuestWriteScope dataWrite(ctx->cipherData, size, "CMAC: unwritable cipher data");
                                Cleanse(ctx->cipherData, size);
                            }
                        }
                        free_nid_postfix(ctx->cipherData);
                        ctx->cipherData = nullptr;
                    }
                } catch (...) {
                }
            }
        } stagedGuard{&staged.value};
        if (!InitializeCipher(staged.value, cipher, nullptr, nullptr, engine)) return 0;
        if (!SetKeyLength(staged.value, keyLength)) return 0;
        if (staged.value.keyLength > 0)
            RequireReadable(key, static_cast<std::size_t>(staged.value.keyLength), "CMAC_Init: unreadable key");
        if (local.value.cipherContext.cipher != nullptr && !Cleanup(local.value.cipherContext)) return 0;
        sync.dirty = true;
        local.value.cipherContext = staged.value;
        stagedGuard.armed = false;
        staged.value = {};
    } else {
        if (cipher != nullptr) {
            sync.dirty = true;
            if (!InitializeCipher(local.value.cipherContext, cipher, nullptr, nullptr, engine)) {
                sync.Flush();
                return 0;
            }
        }
        if (key == nullptr) {
            sync.Flush();
            return 1;
        }
        if (local.value.cipherContext.cipher == nullptr) {
            sync.Flush();
            return 0;
        }
        const int previousKeyLength = local.value.cipherContext.keyLength;
        if (!SetKeyLength(local.value.cipherContext, keyLength)) {
            sync.Flush();
            return 0;
        }
        if (local.value.cipherContext.keyLength > 0) {
            try {
                RequireReadable(key, static_cast<std::size_t>(local.value.cipherContext.keyLength), "CMAC_Init: unreadable key");
            } catch (...) {
                local.value.cipherContext.keyLength = previousKeyLength;
                throw;
            }
        }
        sync.dirty = true;
    }
    if (!InitializeCipher(local.value.cipherContext, nullptr, static_cast<const unsigned char*>(key), zero, nullptr)) {
        sync.Flush();
        return 0;
    }
    const auto method = ReadCipher(local.value.cipherContext.cipher, "CMAC_Init: unreadable cipher");
    if (!ValidBlockSize(method.blockSize)) {
        sync.Flush();
        return Error(136);
    }
    if (!EncryptBlock(local.value, method, local.value.previousBlock, zero)) {
        sync.Flush();
        return 0;
    }
    const auto blockSize = method.blockSize;
    DoubleBlock(local.value.firstSubkey, local.value.previousBlock, blockSize);
    DoubleBlock(local.value.secondSubkey, local.value.firstSubkey, blockSize);
    Cleanse(local.value.previousBlock, blockSize);
    if (!InitializeCipher(local.value.cipherContext, nullptr, nullptr, zero, nullptr)) {
        sync.Flush();
        return 0;
    }
    local.value.lastLength = 0;
    sync.Flush();
    return 1;
}

int APS5_VABI CMAC_Update_nid_postfix(LibreSslCmac* context, const void* data, std::size_t length) {
    RequireContext(context);
    Cleansed<LibreSslCmac> local;
    CopyIn(&local.value, context, sizeof(local.value), "CMAC_Update: unreadable context");
    RequireWritable(context, sizeof(local.value), "CMAC_Update: unwritable context");
    if (local.value.lastLength == -1) return 0;
    if (length == 0) return 1;
    if (data == nullptr) throw std::invalid_argument("CMAC_Update: null input");
    const auto method = ReadCipher(local.value.cipherContext.cipher, "CMAC_Update: unreadable cipher");
    const auto blockSize = method.blockSize;
    if (!ValidBlockSize(blockSize) || local.value.lastLength < 0 || local.value.lastLength > blockSize)
        throw std::invalid_argument("CMAC_Update: invalid context state");
    RequireReadable(data, length, "CMAC: unreadable guest buffer");
    const auto* bytes = static_cast<const unsigned char*>(data);
    if (local.value.lastLength != 0) {
        const auto take = std::min(length, static_cast<std::size_t>(blockSize - local.value.lastLength));
        CopyIn(local.value.lastBlock + local.value.lastLength, bytes, take, "CMAC: unreadable guest buffer");
        local.value.lastLength += static_cast<std::int32_t>(take);
        bytes += take;
        length -= take;
        if (length == 0) {
            CopyOut(context, &local.value, sizeof(local.value), "CMAC_Update: unwritable context");
            return 1;
        }
        if (!EncryptBlock(local.value, method, local.value.previousBlock, local.value.lastBlock)) {
            CopyOut(context, &local.value, sizeof(local.value), "CMAC_Update: unwritable context");
            return 0;
        }
    }
    while (length > static_cast<std::size_t>(blockSize)) {
        unsigned char block[16];
        CopyIn(block, bytes, static_cast<std::size_t>(blockSize), "CMAC: unreadable guest buffer");
        const bool encrypted = EncryptBlock(local.value, method, local.value.previousBlock, block);
        Cleanse(block, sizeof(block));
        if (!encrypted) {
            CopyOut(context, &local.value, sizeof(local.value), "CMAC_Update: unwritable context");
            return 0;
        }
        bytes += blockSize;
        length -= blockSize;
    }
    CopyIn(local.value.lastBlock, bytes, length, "CMAC: unreadable guest buffer");
    local.value.lastLength = static_cast<std::int32_t>(length);
    CopyOut(context, &local.value, sizeof(local.value), "CMAC_Update: unwritable context");
    return 1;
}

int APS5_VABI CMAC_Final_nid_postfix(LibreSslCmac* context, unsigned char* output, std::size_t* length) {
    RequireContext(context);
    Cleansed<LibreSslCmac> local;
    CopyIn(&local.value, context, sizeof(local.value), "CMAC_Final: unreadable context");
    RequireWritable(context, sizeof(local.value), "CMAC_Final: unwritable context");
    if (local.value.lastLength == -1) return 0;
    if (length == nullptr) throw std::invalid_argument("CMAC_Final: null output length");
    const auto method = ReadCipher(local.value.cipherContext.cipher, "CMAC_Final: unreadable cipher");
    const auto blockSize = method.blockSize;
    if (!ValidBlockSize(blockSize) || local.value.lastLength < 0 || local.value.lastLength > blockSize)
        throw std::invalid_argument("CMAC_Final: invalid context state");
    const std::size_t reported = static_cast<std::size_t>(blockSize);
    RequireWritable(length, sizeof(reported), "CMAC_Final: unwritable output length");
    if (output != nullptr) RequireWritable(output, reported, "CMAC_Final: unwritable output buffer");
    CopyOut(length, &reported, sizeof(reported), "CMAC_Final: unwritable output length");
    if (output == nullptr) return 1;
    const bool full = local.value.lastLength == blockSize;
    if (!full) {
        std::memset(local.value.lastBlock + local.value.lastLength, 0, blockSize - local.value.lastLength);
        local.value.lastBlock[local.value.lastLength] = 0x80;
    }
    const auto* subkey = full ? local.value.firstSubkey : local.value.secondSubkey;
    unsigned char tag[16]{};
    for (int index = 0; index < blockSize; ++index) tag[index] = local.value.lastBlock[index] ^ subkey[index];
    const bool encrypted = EncryptBlock(local.value, method, tag, tag);
    CopyOut(context, &local.value, sizeof(local.value), "CMAC_Final: unwritable context");
    if (encrypted) {
        CopyOut(output, tag, reported, "CMAC_Final: unwritable output buffer");
        Cleanse(tag, sizeof(tag));
        return 1;
    }
    Cleanse(tag, sizeof(tag));
    const unsigned char cleared[16]{};
    CopyOut(output, cleared, reported, "CMAC_Final: unwritable output buffer");
    return 0;
}

const LibreSslCipher* APS5_VABI EVP_aes_128_cbc_nid_postfix() { return &aes128; }
const LibreSslCipher* APS5_VABI EVP_aes_192_cbc_nid_postfix() { return &aes192; }
const LibreSslCipher* APS5_VABI EVP_aes_256_cbc_nid_postfix() { return &aes256; }

}
