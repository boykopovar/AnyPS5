# libSceLibreSsl

## change [PEM_read_bio_PrivateKey and PEM_read_RSAPrivateKey]

- [PEM_read_bio_PrivateKey and PEM_read_RSAPrivateKey](../../../core/libs/prx/libSceLibreSsl/src/PemPrivateKey.cpp) - console behavior is unverified. RSA keys in plain and encrypted PKCS#8 work with PBES2/PBKDF2 PRFs HMAC-SHA1/224/256/384/512 and AES-128/192/256-CBC, DES-CBC or 3DES-CBC; PBES1 MD5-DES-CBC and SHA1-DES-CBC; and PKCS#12 SHA1-3DES-CBC. LibreSSL 2.5.4 error queues are reproduced for tested unsupported PBE, KDF, cipher and PRF identifiers, malformed parameters, bad padding and PKCS#8 decode failure. Other malformed ASN.1 forms remain unverified. Traditional encrypted PEM, non-RSA keys and unsupported BER forms still throw.
