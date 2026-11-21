# libSceLibreSsl3

## add Functional

- [ECDSA_do_verify](../../../core/libs/prx/libSceLibreSsl3/src/EcdsaDoVerify.cpp) - follows LibreSSL 4.0.0 `ecdsa.c` and `ecs_lib.c`: it returns 1 for a valid signature, 0 for an invalid one and -1 for an internal error or a key without a group or public key. A null key throws, where LibreSSL dereferences it; a missing signature component throws, and a component outside [1, order-1] returns 0. Error-queue entries for internal failures are not modelled, the EC_KEY layout is the module's existing one, and the console is unverified. Closes when: compared with the console.
