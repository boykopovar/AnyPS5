#include <pthread.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include "cryptlib.h"

static void libressl_once(void) {
    OPENSSL_cpuid_setup();
    ERR_load_crypto_strings();
}

int OPENSSL_init_crypto(uint64_t opts, const void *settings) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    (void)opts; (void)settings;
    return pthread_once(&once, libressl_once) == 0;
}
