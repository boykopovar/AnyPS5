# libSceLibreSsl

## add Functional

- [EC_KEY_get0_private_key and EC_KEY_set_private_key](../../../core/libs/prx/libSceLibreSsl/src/Ec.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `ec_key.c`: the getter returns a borrowed BIGNUM that stays valid until the key is freed or its private key is replaced, cached on the key and released with it, and the setter replaces the stored key with a copy of the supplied BIGNUM and clears it for a null argument. The modelled key needs its group before a private key can be set, where LibreSSL accepts the two in either order, and the returned number is a guest-heap duplicate of the stored scalar rather than the key's own field. Closes when: compared with the console.
