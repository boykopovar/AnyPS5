# libSceLibreSsl3

## add Functional

- [ECDSA_do_sign](../../../core/libs/prx/libSceLibreSsl3/src/EcdsaDoSign.cpp) (libSceLibreSsl3) - nothing is checked on the console. Follows LibreSSL 4.0.0 (`ce488b78e0`): a missing private key queues `ERR_R_PASSED_NULL_PARAMETER` through the EC library and a negative digest length queues `ERR_R_BN_LIB` through the BN library, a missing group returns NULL without queuing, and the `EC_KEY_METHOD` dispatch is not modelled. An mbedTLS signing failure, including an entropy failure reported by the random callback, returns NULL without queuing an error quad. Closes when: compared with the console.

## remove `ba4o0Ih6kSs`
