#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int RsaType = 6;
constexpr int EcType = 408;
constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr int PointConversionUncompressed = 4;
constexpr int BioCommandMemPointer = 3;

constexpr int PemLibrary = 9;
constexpr int PemBadBase64Decode = 100;
constexpr int PemBadEndLine = 102;
constexpr int PemNoStartLine = 108;
constexpr int ErrorReasonAsn1Library = 13;
constexpr int PemNotDekInfo = 105;
constexpr int PemNotEncrypted = 106;
constexpr int PemNotProcType = 107;
constexpr int Asn1TooLong = 155;
constexpr int EcLibrary = 16;
constexpr int ErrorReasonEcLibrary = 16;
constexpr int RsaLibrary = 4;
constexpr int ErrorReasonRsaLibrary = 4;
constexpr int Asn1NestedAsn1Error = 58;
constexpr int Asn1LengthMismatch = 62;
constexpr int Asn1FieldMissing = 121;
constexpr int Asn1IntegerTooLargeForLong = 128;
constexpr int Asn1TooLarge = 223;
constexpr int Asn1WrongTag = 168;
constexpr int Asn1Errors = 13;
constexpr int Asn1DataIsWrong = 109;
constexpr int Asn1NullIsWrongLength = 144;
constexpr int Asn1SequenceLengthMismatch = 148;
constexpr int Asn1SequenceNotConstructed = 149;
constexpr int Asn1UnexpectedEoc = 159;
constexpr int Asn1InvalidObjectEncoding = 216;
constexpr int Asn1BooleanIsWrongLength = 106;
constexpr int Asn1StringTooShort = 152;
constexpr int Asn1TypeNotConstructed = 156;
constexpr int Asn1BmpStringIsWrongLength = 214;
constexpr int Asn1UniversalStringIsWrongLength = 215;
constexpr int Asn1InvalidBitStringBitsLeft = 220;
constexpr int Asn1TypeNotPrimitive = 231;
constexpr int ErrorReasonBufLibrary = 7;
constexpr int PemBadDecrypt = 101;
constexpr int PemBadIvChars = 103;
constexpr int PemBadPasswordRead = 104;
constexpr int PemReadKey = 111;
constexpr int PemUnsupportedEncryption = 114;
constexpr int PemPasswordBufferSize = 1024;
constexpr int EvpErrors = 6;
constexpr int EvpBadDecrypt = 100;
constexpr int EvpUnsupportedCipher = 107;
constexpr int EvpWrongFinalBlockLength = 109;
constexpr int EvpDecodeError = 114;
constexpr int EvpKeygenFailure = 120;
constexpr int EvpUnknownPbeAlgorithm = 121;
constexpr int EvpCipherParameterError = 122;
constexpr int EvpUnsupportedKeylength = 123;
constexpr int EvpUnsupportedKeyDerivationFunction = 124;
constexpr int EvpUnsupportedPrf = 125;
constexpr int EvpUnsupportedSaltType = 126;
constexpr int EvpNoCipherSet = 131;
constexpr int EvpUnsupportedNumberOfRounds = 135;
constexpr int EvpUnknownDigest = 161;
constexpr int Pkcs12Errors = 35;
constexpr int Pkcs12DecodeError = 101;
constexpr int Pkcs12EncryptError = 103;
constexpr int Pkcs12AlgorCipherInitError = 115;
constexpr int Pkcs12CipherFinalError = 116;
constexpr int Pkcs12PbeCryptError = 117;

constexpr char RsaTraditionalPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpQIBAAKCAQEAnSLmvsiTtm1KhSwaFmn9kbS7r8t2OakfpDVlVsA3/7Jxae/2\n"
    "tHdP3RBXtZstk/mguYOBEVEqZC4W0UZmDOurhu3JeqtVCMjhRMbB/e8VqwoVPVac\n"
    "GCWSYou5Kxg2cguwt83bE+AiDXk9dUyQlqUnAHgZiaKC7vp5DdxnpkucjERjppeL\n"
    "L2/ca694m3D2zsusS10ODTy+d1t6bj9zBH8Ap0bny38wskhER6GGR4LDX81iKHrM\n"
    "Dg7fzcLJB2g0pu0McxIq5VqHsPwpmqMgIEQRHIveWBU24fwKpMEI8fTzjDD0XcA2\n"
    "DFz9c5SfWd77MogCStgl6h9uj78BnFavGLZObQIDAQABAoIBAATZufOzr1OXRhCc\n"
    "TuXYObqQsKZ5ilm3TcG84DbwUgozkOqrPJtPf4icY4i4lIyXzQ1M8OP+9GcJQS0O\n"
    "lkHZ2H6ewnRl64cM5F9CH2TeR3XzJrqw/LqGFmGyXHHnGm8kyzyxPwz7AzIVRv9i\n"
    "8Kely3FpQ6iK+q2zz57M6eopNVjjCyQhYHLKeTUmU/sr2FHku6qEhjYAyL4ZKrtT\n"
    "cy50CTDsrCpYgpgAISSQuO6jlqO1qWFqmcyTWH0RUji353tGmj62CI0Yh1DLfWv+\n"
    "Ip3tDoQl1YFVNItur3XBvVE2wtq33Q4XdHWjBquowiO1dXrxMniBtorsRNs45Wk9\n"
    "all+LGUCgYEA1bvnBN8HddXDcW9XOQmyylpx9PYFR2c4qg79KuffLsPcv0b+eIhb\n"
    "tNDoLUiiyaXYhkBfQUvxuJxclmZaL/T2HyW0jIsbYeZR+IcU/anQouX0LTsv7b6z\n"
    "qnqo+5le8wL6dxLYubcxub8M6ILoRw4DZVEK9/+KxchEQvcUQ3gatQsCgYEAvDXK\n"
    "pWxB7mpzCOF2DoneGFLXAwFTNehJLbMilM9njW7B+Axz8i7k8xfVH+DRYzobsmPe\n"
    "5lq1YNhV9qRUObHTzuRMYBN/gb+uFlB6sjWziGHTl2c2y8chCKqC+RrSe1mq8HQ4\n"
    "RywbaFEC/ogM854Kssdp1eMjbgATh9WfGmWyxWcCgYEArKYDQBkUwqexH5Fr6KQf\n"
    "63tR8ClHLF2CFUTUVnNFAlaum0gyyJSFV7MkyWeSL8cq949KIsnA8XpFBCybj3+V\n"
    "zKU9igXYUaWl81bSqvEgmmWcb2kPF0HjicNZextx6mUugz8TGDmWDk/4kl0kWjlE\n"
    "nvPvI1Ml+wwBsAI8OR/smu8CgYEAoQFjWiBCRrLJtc8qL0XBuQ0k4m0/wd30vCOC\n"
    "ccr7ywH07T258nQrZ8n7iEnK1hffmbvV8zml8DMhVt09MB3wFA05SPdlHJndUzmF\n"
    "vO4zp3gIinzBY5H1sHQtluxbIyqSESFbA4e36oVjCltYuQ96F3aAE3k71d461vjo\n"
    "IqzdQgECgYEArHreTGO4wKEl4AfYmOOJsybL8xW2x557EEL8SOk/mopGYP3VNn5B\n"
    "8BfAK+eqgbjop6fE4PZDp2n37a402b4Upc7JQrInGpZwIlE/9bDTXFQWkS6Zny0A\n"
    "ncQjOJpY2aYydxnb/ujlEuAcSTyGQQIGamn93QrMtvB351dBVJaUJIM=\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char RsaPkcs8Pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQCdIua+yJO2bUqF\n"
    "LBoWaf2RtLuvy3Y5qR+kNWVWwDf/snFp7/a0d0/dEFe1my2T+aC5g4ERUSpkLhbR\n"
    "RmYM66uG7cl6q1UIyOFExsH97xWrChU9VpwYJZJii7krGDZyC7C3zdsT4CINeT11\n"
    "TJCWpScAeBmJooLu+nkN3GemS5yMRGOml4svb9xrr3ibcPbOy6xLXQ4NPL53W3pu\n"
    "P3MEfwCnRufLfzCySERHoYZHgsNfzWIoeswODt/NwskHaDSm7QxzEirlWoew/Cma\n"
    "oyAgRBEci95YFTbh/AqkwQjx9POMMPRdwDYMXP1zlJ9Z3vsyiAJK2CXqH26PvwGc\n"
    "Vq8Ytk5tAgMBAAECggEABNm587OvU5dGEJxO5dg5upCwpnmKWbdNwbzgNvBSCjOQ\n"
    "6qs8m09/iJxjiLiUjJfNDUzw4/70ZwlBLQ6WQdnYfp7CdGXrhwzkX0IfZN5HdfMm\n"
    "urD8uoYWYbJccecabyTLPLE/DPsDMhVG/2Lwp6XLcWlDqIr6rbPPnszp6ik1WOML\n"
    "JCFgcsp5NSZT+yvYUeS7qoSGNgDIvhkqu1NzLnQJMOysKliCmAAhJJC47qOWo7Wp\n"
    "YWqZzJNYfRFSOLfne0aaPrYIjRiHUMt9a/4ine0OhCXVgVU0i26vdcG9UTbC2rfd\n"
    "Dhd0daMGq6jCI7V1evEyeIG2iuxE2zjlaT1qWX4sZQKBgQDVu+cE3wd11cNxb1c5\n"
    "CbLKWnH09gVHZziqDv0q598uw9y/Rv54iFu00OgtSKLJpdiGQF9BS/G4nFyWZlov\n"
    "9PYfJbSMixth5lH4hxT9qdCi5fQtOy/tvrOqeqj7mV7zAvp3Eti5tzG5vwzoguhH\n"
    "DgNlUQr3/4rFyERC9xRDeBq1CwKBgQC8NcqlbEHuanMI4XYOid4YUtcDAVM16Ekt\n"
    "syKUz2eNbsH4DHPyLuTzF9Uf4NFjOhuyY97mWrVg2FX2pFQ5sdPO5ExgE3+Bv64W\n"
    "UHqyNbOIYdOXZzbLxyEIqoL5GtJ7WarwdDhHLBtoUQL+iAzzngqyx2nV4yNuABOH\n"
    "1Z8aZbLFZwKBgQCspgNAGRTCp7EfkWvopB/re1HwKUcsXYIVRNRWc0UCVq6bSDLI\n"
    "lIVXsyTJZ5Ivxyr3j0oiycDxekUELJuPf5XMpT2KBdhRpaXzVtKq8SCaZZxvaQ8X\n"
    "QeOJw1l7G3HqZS6DPxMYOZYOT/iSXSRaOUSe8+8jUyX7DAGwAjw5H+ya7wKBgQCh\n"
    "AWNaIEJGssm1zyovRcG5DSTibT/B3fS8I4JxyvvLAfTtPbnydCtnyfuIScrWF9+Z\n"
    "u9XzOaXwMyFW3T0wHfAUDTlI92Ucmd1TOYW87jOneAiKfMFjkfWwdC2W7FsjKpIR\n"
    "IVsDh7fqhWMKW1i5D3oXdoATeTvV3jrW+OgirN1CAQKBgQCset5MY7jAoSXgB9iY\n"
    "44mzJsvzFbbHnnsQQvxI6T+aikZg/dU2fkHwF8Ar56qBuOinp8Tg9kOnafftrjTZ\n"
    "vhSlzslCsicalnAiUT/1sNNcVBaRLpmfLQCdxCM4mljZpjJ3Gdv+6OUS4BxJPIZB\n"
    "AgZqaf3dCsy28HfnV0FUlpQkgw==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char RsaPublicPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnSLmvsiTtm1KhSwaFmn9\n"
    "kbS7r8t2OakfpDVlVsA3/7Jxae/2tHdP3RBXtZstk/mguYOBEVEqZC4W0UZmDOur\n"
    "hu3JeqtVCMjhRMbB/e8VqwoVPVacGCWSYou5Kxg2cguwt83bE+AiDXk9dUyQlqUn\n"
    "AHgZiaKC7vp5DdxnpkucjERjppeLL2/ca694m3D2zsusS10ODTy+d1t6bj9zBH8A\n"
    "p0bny38wskhER6GGR4LDX81iKHrMDg7fzcLJB2g0pu0McxIq5VqHsPwpmqMgIEQR\n"
    "HIveWBU24fwKpMEI8fTzjDD0XcA2DFz9c5SfWd77MogCStgl6h9uj78BnFavGLZO\n"
    "bQIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

constexpr char RsaEncryptedPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQp51CO2MWeBY6ue67\n"
    "IZb0ogICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEELZxp7IrkRO0AjPl\n"
    "k0zgylMEggTQwgNlrMH8UnklVVc4ZRoWdDWs7dn26Ol3fAxAX8iyMoiZjBX91DJi\n"
    "B+CrhQpCsLZr3V3uYtCrl6DzyqWaBKeTzMuC5XLVeFF0fuFC55IlGYA5QRD24ghp\n"
    "/DqXGssYDS0a5Sv5Ag6P94IWjX0hcuJkK6QOaavhdn4bmHnin5x1eig0u3ICLyMj\n"
    "bdfWziHfnQT1MTPDfaK/EjkPqFCdJx32V3gYF+eAbabLLz/FBtYTWCjVTlbuPMgz\n"
    "KhQFYokIIihaeeuYxhODLxy93iDFlbxO10tgHdl74kA/ZjEBE1+iHyiZCvHi3KCw\n"
    "Xq2Ykcq7xGLzZxE7HE2VTFJhrAiVEydk8fUTkkS0x7ezuYU0uz6P+L11eXJGPBCz\n"
    "0CvVqSyaAa/rkBpFfWS+lxnybtPJZKJx80q48T9viRqLZvitSoDsqpVARztNMC4X\n"
    "Sejt4sGGOx8Vf9EeMBTD7F8YCXekQWF9PPHVdyAmxrc7Hogkx7BdgL53flVdSKyB\n"
    "BJZI8oG2vdBnYArL1x7akl6CkCNXXvCwoeFHJXvXOO44H801V2443RKhxtsG+f5K\n"
    "syjcve/aHtpFsCii5oiViMqi7ankImmfhqDPBEP/6yE2WZvO6AsOlMZK6GGtaE5A\n"
    "lOQUYiDL/Zbih3dVl9GR4APP9FxWqTbKEF9jrU4nr1a0xaiqV6cb/uubW1+yLoK3\n"
    "tKmJoygT8l0nt7M4mu1Mia0I4GXut4UqygcUTQLfJHXp5uOFw8jLGWrPMADUf+zP\n"
    "is92hCbOElfXI6amxZVcNlOIb9/cj4IVvheu8EKESw5bUQY4FP8ctI5e4U2CZ0pZ\n"
    "mqFUanDbclga8Ap3lPRfspZk4FAevGJ7nILe9qHLstAgIBtEICZDhDPz7lTc/aUi\n"
    "HRg9SEMOS8QFJfkDezwrqx5G3rRoXSu1TsB0KhqALeCQT+EJKzTtETxsRuiLq4da\n"
    "apYHJ0NPZH9US5zz0FG0h2O6vPyk/Cen40DMCaUeM6os3X5g4Fr7EAQ0EyBc8YSd\n"
    "4tulIuZCXQOVSTEVSmujKUsXiDYuHdzgrIs21Kc9A3/rvky2kzpXvaLL8dMGe9oe\n"
    "Dch/kE/HeITR4W1/xu1p+pBPdYrh2aCeiIE110uZysvLZmKOnu/Ze4Jx4q0APffU\n"
    "ByzPMJEv5179ZRpCfJgo8xJIuoEX3EIssfnVtXFiB3wrdRR1GyLtn7z3Qr+v63kX\n"
    "LyuL0Vdy/Q/pGbcW1oytWY41LWHrofpPZMokfU8xeBlWgjqX53edetuDzKmjNCJD\n"
    "0xJFHIuokNwiA7ItXSbIbDj0aXIz19iDJocSA4dtROQiGIaubJC10w9Z2mRh6G9H\n"
    "TlyDd7X4R/0q+KoYGa81yfndIZD5PKE585oVJeOykZGJ00yQ95LkgFqjy8ZWCRFL\n"
    "apTBuIg1UiaeeB2CGBAOg0g2rQHjnz9xlSyMwA8vffGn/8ZrlVikiJOHRMOs61Fa\n"
    "ZADiEDFzDAsp6NErUCCv/GBXQySb/fgUpCLEugytblj17C/XVQSxMCeEaqrIxjZz\n"
    "hyE0Bhj/8PWwbrqnbfF7S5t4Cs30sttsFxuFdhQm2qxSsUpklr0t6ZoicJxVKgrS\n"
    "5hT14VmDF/8tZqsq2mHUfmQ1z7K2cFo0+mmPIECjk/aXjTkJ6h4mou0=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EncryptedHeaderPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-256-CBC,5D92A8FE0AB34C041BEA60A4C16162FC\n"
    "\n"
    "gdF0NqYlgIOCD9E5EYDdckZ7x6fNI9rUzDNpNUGEtKOIRsM6zEJ5WeAgDHykETce\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char TestPassword[] = "anyps5-pem-test";

constexpr char EcTraditionalAes128Pem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,0F30DE72C44D2D625DE44863C11CBA88\n"
    "\n"
    "VPROzD/J1zDpb5F5VLpOkZSSU0Ht+NSOON5z1KO2ILCnS2hYrCfeP+ByUKJAxX24\n"
    "h4QrC67UIyPHVggHAPKuIQB8uf/KvKcNHCNDOxuU6mHPA+GpWTl//tM5zzOouTaw\n"
    "y+9vcVn9NimiD6o/kkSflMwraGGVTVd0eUr0Njyxosk=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcTraditionalAes192Pem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-192-CBC,C26A1104E06BAE5517A52D8E3643F687\n"
    "\n"
    "6n0s4fLkyeCzQSzoYKFRRrmWB9R/p8hg3L+jFSY8YFp3M/1bRR0pFNj1gAI6E8vA\n"
    "yOdE93vn3KUtpqTUW5GhZR1MY7CqJkPH7bGU4ET8Nd9GNjOyA1bElCAaYnyKElXP\n"
    "9d6BGS9RNDXPeEr+0xg0nNlG2MluBzhb4iq/4U70+tQ=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcTraditionalAes256Pem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-256-CBC,01F59F9F7108AB477D1CFFB938766907\n"
    "\n"
    "ThA5ze1KSTqVrYBxdgD4762G4hVwS0t9PN3adt6GZWuo5CD7+JhcFxeORxnDuTbN\n"
    "Zu1iLuzE3a9w4WIudxKDuxh5pImjR/hCEe2/ZHv32JcUSH7+q02IYdt7tx+wlY6y\n"
    "Rwx4aw3jlRfnTHuNcxBHvls7HUYqqZ/DWNeq5+m08P8=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcTraditionalDes3Pem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: DES-EDE3-CBC,8B3B291B1CE9F357\n"
    "\n"
    "1zSjjmi96kgY+tnYKEpgRpCA/rJnmJLO0TRC8XXcFcO0O0sanld/GUvx6vPG1veQ\n"
    "N15YXsKKYCS5AjnTaqj/tWBVGggvec3gdYRT9s/2ApQzsqU2rWOoLgHi3AMJYKb0\n"
    "Cn7JI3I70eS2iX2ZiuSzG1QA2qO8SGAwBNyOn0H6Feo=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char RsaTraditionalAes256Pem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-256-CBC,3914FF53FACD163B4BE4F90CCDDDC77E\n"
    "\n"
    "FjdInz7wgLn8tUb/Ku3KHFsjAcs/bn+rf6SabeGqM4uFGiqBKLC2aaPnFR13EQiz\n"
    "ykYHxeOOHHcTlgKYnzWIQYaBH1E+T83lBB8Sxke27/p+ulMf9K8X/PlWoZwHnGYG\n"
    "ZLLf+Mpq/OvCyYM0ST2NkZxiHZ6JBgOH1xJtjDsTXqhUw2jQMOmpl6cjTPN6431t\n"
    "F4+AaWufulCuZQukk7kN8NZQhOSVKL9DQ6JBlfOiwp2v0NppObrEmS9dohI3NxR/\n"
    "GhlCQjLJqyd7xtyUd75qOOmck589WEMOl3w3nHNb/1CpaC9OehXr+sVhZ8a0uX9S\n"
    "4OqVdA32bwhTUa54FvYuELL3j2k5PG9z6BaRJ859ukWhQeWSqI1fvSfmMVxHgmc1\n"
    "2Uqu9c6KUXYJ3D3IM/AcDKA4BvZqPlJxl+c4/OcUDhLizzeGDZuHJlJE9OHgK3O5\n"
    "CUB/kOWw+dBbF6Vj8SGMRETrgKNn4EVmGH0qpKdX+kwJlyYzy0HmhHkVhck33lxP\n"
    "lzLifMTXtzns+KVE+BpG2ezg7HBXi/A8qVqdaspDKrwf7ukHpmpWnP2m92hZEzxg\n"
    "R0JsI5hlgGHhRX5142KOsonhvagycvDI9AEMLeUADUNZqg4GZCdNufALXyBjx52k\n"
    "9+oap4DWafWiDielpmHgZCxGX7VzH2/KLM1d6AjmL4DSvPHt/fcxOum7+bdaAcrv\n"
    "p7EhdAJ4mfNPWpZ6Kjz03tRiSoklwNzRNGTf65tVTOuRAMRnVxrMTouBlYXUrHAq\n"
    "4NXDiDM9sLcQexHF6IXTdMM333imQU8/y3FOPPGBiPL88df26ly/fSq6525GXQ+4\n"
    "LKqKGjMrJfd3hXA4ZG6iaQAC78Kls0UoY435ZT6TodL0kDLMaCZsjReMGT+hx5xA\n"
    "7Nf/SK7bBoTIeFwV+2SQMGmwMNXvko0emNf6uTDa5QvAJg+hossAfY/IInDpXfN8\n"
    "WF8B+PiRYy9zk3E4/MEJXHAEV0lwdP/RTj6ABamVl6eQjyYVS4nddUDtO5MWnLVt\n"
    "oT6Od+UXYDL48t7QLjSn0a+ISpRkNiSn5MYnpeKd8YP9LeH8+2KE5+7JPXJhykGI\n"
    "grmAckq75lu5p9DPZdsT0h4dwQbwdM24sklrZeh1NCo7ahomJJOfdpcJHYkJn096\n"
    "lamauDCWaMuOVQuyws4/TOL641mS5gj8Ay5rQ6LA/Bp9HBEyBTdjf9ywHKT1w5Qy\n"
    "nsDWoZk7K+fUNEdcTh3WB9hvSUMP4+gusbc2pjXIlMK5F7I8JVfWnlupAhJr5Zil\n"
    "lqxez0WTLgDgOD4zWwtUSkJZFm3v29cHOQN/4m7spjoCdEO9yx7lrZORGfdLQxJE\n"
    "TSF59N2wHQbfzT3B5MiPnoBQCOvyZtAwKk8eFpkBcBZ9cn4dOI+sojgMGfDy8ltz\n"
    "fm6DvkiBKnKEbzuOh12kLUKqfPddzOG/hWVwCkV1XXqMGeixUH1KCwvtkAJsr4js\n"
    "qgORtrgjw1vY8Rz1smeRgQxXxX84AMjLiQGfLlPqhZ6djAf4J9oeTHpycv60NBP9\n"
    "5kADVGEcqUmP4+1lX/JZDcmYcgHPyLLQbjlUkslQ/QJgoGJMhavBSwZWIrUijgKc\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes256Sha256Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBAyL3afHfsE/IjQiayr\n"
    "j2e8AgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQyAIpYZm21dRlhhMZ\n"
    "5AO8BASBkNPslCzj+QLviW4tjmJH5+C07ef4tmhnQF+R2isJr/jomE0zHC/QcD03\n"
    "IJ5uTf0cVMHYHQ3AHUAddCaH7dS+lcTRREz34vs2JN5RmMPjstUEsvkGUiQbBJk5\n"
    "hWkeJJbniFx2cPhOjXy7uGUQMGFO9wPG2gZSUijyLcDUQXUqWekCezJNUdVhnRlW\n"
    "MMKj98YTBg==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes128Sha1Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHmMFEGCSqGSIb3DQEFDTBEMCMGCSqGSIb3DQEFDDAWBBA6ia4oaggPx1cBcpEq\n"
    "sYxTAgID6DAdBglghkgBZQMEAQIEEIXNe1RSxgkQgquWCQdPuz4EgZCDPSa6P/h8\n"
    "fyXbrFlaY0ItZamEDKLZ/mpIFtRGH9PXV+542ucge/mDznz1xow+63xdqyxpuWhw\n"
    "SDmU6b7vxXzHHIB02X1YtY2fLe/7G8Nidm2W4ochhmJAt99CYB/4X/B9kVhDOcrm\n"
    "c0NUYI9rlYVt4ZO5Sc6qyFwleapP/LAk5oqOweOR9OWb+AO1iVwMgm4=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes192Sha512Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHzMF4GCSqGSIb3DQEFDTBRMDAGCSqGSIb3DQEFDDAjBBCNkUoKsFBO/oxx6UEh\n"
    "6EChAgEBMAwGCCqGSIb3DQILBQAwHQYJYIZIAWUDBAEWBBBt7nvlIySKqPCqwBRK\n"
    "OTl2BIGQ9RGsi2UU0ApNamMC6jjMm0y3qoSn3LbEkOknL7ZFGWPv4ZTVApCeIQKM\n"
    "BU/dRZleJHFrQkEr9kllOOp/Q43MIz7IvCG24OnyGnLdvtPLJIwDq2aJV4gVhQQf\n"
    "+PbIx4+gGTktYqcViQDOrdaI8JD3wuHKUirOaB7Ho6TzGY14q2bM1bLGIuQwqcom\n"
    "jr6pfRS8\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes256Sha384Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHzMF4GCSqGSIb3DQEFDTBRMDAGCSqGSIb3DQEFDDAjBBByuRUNTt98Tud+msei\n"
    "YdobAgEDMAwGCCqGSIb3DQIKBQAwHQYJYIZIAWUDBAEqBBDsuDyr3hzn/rRAOg+I\n"
    "9KNCBIGQFfW5cSU82N1nYAAu7+Cd//INHv0NHed6yNq6WhpHqJtsQEz5bQFFqQke\n"
    "yQ1pTzH6xpSqd16kA8YELkp1d2pJYnoqpoziaf2XmmUPr+TcM3qi1gPF3Ls5YSV+\n"
    "XUOV/DZQ1Kym4r/doODcnmagg4acZgjkKl1XPtylhbc+5OzvpS13eCqi1vLh/z+o\n"
    "up/rgKtJ\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes128Md5Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHzMF4GCSqGSIb3DQEFDTBRMDAGCSqGSIb3DQEFDDAjBBAZYflFLGOuac4aYLHX\n"
    "3Bv3AgEFMAwGCCqGSIb3DQIGBQAwHQYJYIZIAWUDBAECBBDPQxs0lzZzGRMZ5e5Q\n"
    "ElZuBIGQS3IK0vJj7vkB/JPnbu4LfRoSYMutDWEbJuISu2QfYU2HoVjaGFB/Rgwd\n"
    "XoQvhwO/0edc04PV/hpudK1saHeu5TlcHeup5R02zWEntlz4yJBCOyPTEKD87yX4\n"
    "lLaSvh4te8R5RtUBSc6K7hKlMehRZlzmp7WhRfC6dCq7CnTASMdFMqeKxqXJeUvW\n"
    "N2P7vpHH\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes128Sha224Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHzMF4GCSqGSIb3DQEFDTBRMDAGCSqGSIb3DQEFDDAjBBB6lNHGJu0y4V+fB2iG\n"
    "N8I8AgEFMAwGCCqGSIb3DQIIBQAwHQYJYIZIAWUDBAECBBCnuht/I+hgzUr1xHT6\n"
    "IwpwBIGQL20vdqxo4Eo3GLBYZe4C3qnbLUk0jo8x3CDfuGGg/JrER3UicaTMwWxq\n"
    "DJM3ZuPc/qfApD/zdlRQ/0sxSo6CKtnBMw5cWekyUkTrAULUGNd3ja4IVWt3y7Fb\n"
    "CV8C64xx87G7wtImQ22d4HSlXaS0nJ+eZforvaNeV7V2F+3e8JblXwry5xEAroQm\n"
    "yth9kPBS\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Aes128Sha512_256Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHzMF4GCSqGSIb3DQEFDTBRMDAGCSqGSIb3DQEFDDAjBBASig5i22ujz+RS3kx0\n"
    "MOd5AgEFMAwGCCqGSIb3DQINBQAwHQYJYIZIAWUDBAECBBD6vg5aJO699rUPQhJG\n"
    "AMaSBIGQuZPWYLb2oBrg6lWC24vow0HlWlwYf8kheH7Flq6bs/Hjj4ied/NVCJrh\n"
    "ScurvVWeihzgkZ5rjQiFqI000qvqgXrBpJc9yjaeKTc5N+2B/0nZSih5uScWxX9k\n"
    "aEgUVLZj8q50doSRnkgYj98s2lO7Li28QUUytRUH9xmOAntRDGMZtUAVTiyrle/w\n"
    "CQFOO9nO\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Des3Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHrMFYGCSqGSIb3DQEFDTBJMDEGCSqGSIb3DQEFDDAkBBD/rekLFhD5JoIqZBgN\n"
    "IOfiAgIIADAMBggqhkiG9w0CCQUAMBQGCCqGSIb3DQMHBAhuKr8QtUo7HQSBkIDI\n"
    "9g6DiQvARZhxmwrw8XP4+P5ns8uQyuhvyjK3IPuKSvg7ow4kkJV3fGrJJKyWeHhY\n"
    "/ZyH2YhvpozqlI32sJSyPPZNsnSc75ErBWrfBYdtzhj2kZ/EWoySb+zw4qjFj6Ir\n"
    "tRSPPvnB+W+itLgyXoC2wQlyOHhPdEXSdC/EjELR8+TCMjcNotkUGi4RkmHcbA==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Pbes1Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIGxMBwGCiqGSIb3DQEMAQMwDgQIq5h3SktflhkCAggABIGQ2F0udCaW/kHkY2ls\n"
    "0UdbMwBYJCzNpTlPyZm8EcOXO7vFZ2Tyj+/YfFTmJXy2+e4VF2JJj7mRrJBHQahA\n"
    "1oiQckUiFXg4eMr+w2qmVgPYiPAo+xqYOa1SgThBv9xJi4V0pprvMuAjZyocAJVu\n"
    "AqFRJ5tUAETg546DXBSBZEWt9v4F4xGeOTVjcdld5otuZS+2\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcTraditionalDes2Pem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: DES-EDE-CBC,1E0CD05B1444D7E6\n"
    "\n"
    "zumB2wLaqTm4iVvG8Z/Q5dMuOMcVFdF8r083oVPwSP1W+eb0xF92sjlpS3m1Y+O1\n"
    "eAodC9G8mj210j8jQvmRFNEBo8bsPPJebDOjls2QSPfg9pt/pHx1G6HL1mn0RQJV\n"
    "YTtrh9Mmuydcf4uZPMJCk73WS9BMIKQ2eF76GmIkoA8=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcTraditionalDesPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: DES-CBC,B12CEA20A6E4AC3D\n"
    "\n"
    "ecyLn7OEATKswXbjntanaTUTf3QCW7QpdEmBXAO7zBWEh3TqpQy9S+4I2Qa4iW3l\n"
    "cix0Otkgi4GzsGrQz7IxfpDhWknUOcZqUwC2dpEK/XwEI/0XZft3hHH/oYIbbPIV\n"
    "muu4tFEefwmbOsWlTNfpTqLktnIkCfU0ViSAQbZ1PYg=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcTraditionalCamelliaPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: CAMELLIA-128-CBC,431AC761BC10E1CC019F7ACC85806207\n"
    "\n"
    "gYBwiE2KAW/PCTZUEuURT1CyLABszHs3kMdUVrspBG+dlVxp91+hHD2M6DVjOLIz\n"
    "rkZmnIn5uA/StunCq/lBwVoGC5wMy0Pw6WAgdn2XQYRmjtpBeEf/ekiWlRtlZt0H\n"
    "KLcgC0ZON3FTNMw+aRgkP3OybRF4YJcY1jtInJ2wj0Y=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char RsaTraditionalDes3Pem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: DES-EDE3-CBC,5072208DAB758B36\n"
    "\n"
    "uG9fnqXWoL/ONjs8OBDqDZ7+f+3PiO9gdwA6mCohojJkssaF6bfRS5mKMnPgP7j1\n"
    "0ubs/oPk6pdBDlcxaBiB0tud91eysWVTIf6XR+MleaiszUsO+jt3P26F1UcPlZCv\n"
    "QfyF4J630VyiKF0CwN70F9Ci2G0bF9D+/vZc0QjI5PrhY1UxJW/MF1H66Aw+d6Kv\n"
    "k5QhJmMz6pBRLt4DRmCfzFb1xJxv/glGaHwD2BGWr9yBex1IRk20L8kJOTTbJN+o\n"
    "pcoCKeMX8LxiKxlpfKmXBiIKBa4PsxMgCpwQciQNXgUIR6GL3MtpA2BNGqRivlHn\n"
    "grsljzqvvhaWXZsJFnGTcOeM2onuPBbLbU8rKC8SMk43wQavbQ34hbHjiTv4uplV\n"
    "7f6LarCSlK+Yvjd3UEjIgLBdU+bZcOuWQ3YsmO38031RL5Jl5H4A5ICAzwu2JOpg\n"
    "BftGS5kdY0Gxkdhxy0MBppYxqglEj/8oH8cHezE99CBTeFVWAFHKP2mjAnDtAnL4\n"
    "CmLTPZ68yW6WVv/W66hgcBc2Gqjk02fZhD4GfTRW5i1qwOH6tyYBB9GGsPhwVdK0\n"
    "dN7LlRjgDrnJFH814nHmevdKrjh8Gv96LWcT9rH2G0PYpNjiCdlbMW49nBlHssn6\n"
    "Jnh5v6EKIR9cjYrqD3XViQZBe/hiEDGYNslwwEM7Ltt7BhgDr5nX4x8B8RZ8uIJ8\n"
    "LekIaTxY3qFNmlozPrOqB+F4ONoUBqmuu8qUBy/u50R9LxBJaPdd10DnWB8sH10R\n"
    "rRVn3EGb9f3TJLWrVe9AhK/N2s+DJ+9CkhffzMDJW6BjvXedXE6NVEuCNj4xQdWV\n"
    "WWD826TG7PDcxFeE6Uq1w3OaMqB8CMZ0+iuIoDypatPR7YWuJtYVs43JQSODwdss\n"
    "Wfy2buJeI7E1HwNd0i+8MRIGupXg/TFvKgxS56/4PH4xJiWSqrNhUaEd2KqeifMp\n"
    "xcrp1csULne4E0+R/nQA/3TDdO/iYUpzEIwAdoyjZHmKVlklBlZ2E11o44L4ezlb\n"
    "3DbQM78xeun6fP1LoPyrgM96TxfLULCKaC1SjYaVeoUV1EOT5+SLYM13B5x+3XNo\n"
    "ww/7Dd4J8yiaLM1igs5PZsPWf34gHhY7BzIMXOeYSeM8KRePbbZIcodcoHN6Xund\n"
    "SuqINSjAJK/FB0cvqtnSOvbtTPIi9Uv50IEFeMyYgPVkupFg1OiAVSQ+98AT9tJx\n"
    "S9QZzAkHWFqghqpyHmfwpEg2MM0bSxzHRs6U7oUZLhFgl3B1GBsxcWjiGEV9Q9MI\n"
    "BQ4A3Ximjw2ITUz8sDOsyeHQiTzfbURYPa6kiAoK8qXm/sA2JNVnNQaCJi8uhFHF\n"
    "Es7H5dsW/1mgl74NqwiXXFRxKHL5tJfOT6R8/r1SU8u0Csfv95c3K0Q/JUG4+Sqx\n"
    "ekWeYylKJuLB72DOL9hBMdpqsdPeadBlSidRrCeriaHTaCGGd/TOkEp3EpheU9mY\n"
    "WamnjijlaG4XFjffG8K4nq35PDecmGA7obegJ8NbzdObg1YWdBQpwqpS3guSqp+J\n"
    "n5QPKI5SQ2G1/vWXPnuSKu5f9yp5Mk9bSY9Rdso4W13QRYW/LDRxj1ra/MIMnayG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char EcPkcs8DesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHnMFIGCSqGSIb3DQEFDTBFMDAGCSqGSIb3DQEFDDAjBBCucmsC155YWH0D4umx\n"
    "yJA3AgEHMAwGCCqGSIb3DQIJBQAwEQYFKw4DAgcECFoPAfgf2mihBIGQ63HMKu0Z\n"
    "jm3MnZaJBk1nLwePzAjh4vL7wvetYuRlK+lrUJ+h71NxUWDDa97/ZymBZuIg6n1c\n"
    "7ujmcqapZ/kcUSGCYjbmNH56VmDTtabYak0li9mQ6ir+wM9onN5e5WDsc6Nvm9oT\n"
    "zMiHyG7n3OEPgBXKOr3JcCPD8YC4IPvi7KSzJzlHtU7DIY6B3RPAeQS4\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Md5DesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIGwMBsGCSqGSIb3DQEFAzAOBAg+S/xlLLgsSwICCAAEgZAA8HKRVu5kp3QK/UZs\n"
    "vnvvCnmC9DwEDVmZ7mFT8sIwYv1Q6emb/VCCWbG2ARoKShCP3FjHoUpwny/qsclJ\n"
    "sdtMQMp5QlDOg0bQlfsJBpqmgVbLs3sg0fS6a0PqnlmVnRKL7J+jT5BQ8p1k7VUU\n"
    "Vs5a/hJKeG/FZeYBqwFniiWSqIfPo07qfB3EpT4uyN2rORc=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Sha1DesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIGvMBoGCSqGSIb3DQEFCjANBAjPhOYPp1+53gIBBQSBkPTtI7OhuMctl0SJ58uq\n"
    "PNQaIdy/CQni4K8Is53rRRoqtCDBQnbSr3/Nq6yxcjaSl75jl20tEcrTvsgjeBVP\n"
    "gXyXO0D1dAuiuUl0gGt/9mlGq6l7Yn/RhOkvGs5jBPBlYheTsKyn9DwUUSYA0gzw\n"
    "x10wdo4gGxY4DNTOpiTccOh0aI9s1FRRAvZ1/lJfQVi1xA==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Sha1TwoKeyDesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIGwMBsGCiqGSIb3DQEMAQQwDQQIitiHKhMufo4CAQMEgZBWDYvS+JbAH/IXNnky\n"
    "m8imNUNRc9FFVpagsdQ8wCkbVwk3xbTawakNRPshYUYzAuGwRDeyCgjmC4P0GbVC\n"
    "5915xw978sqRRuzSnYO1K+jRdkfV8BKDJrGrOUpWeUDaZKvC5SPvj7CTOVD7p3f9\n"
    "ojb7NAzv58qgPz34O0nahOt0U3LQkln7LIEdKu56fCF9hlQ=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8Sha1Rc4Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIGqMBsGCiqGSIb3DQEMAQEwDQQIqmQrK5TelsICAQMEgYoch3L1GGBjIX0MMJb4\n"
    "2iNA0b0bDe7BPCJmaq+Tg6EszIElShOC9jcjE2UyxR6N53I8KE4TOzoXIk22+7IP\n"
    "UTwPkN9JCncrWKMUk9w5KBmPd7pDqjbVylvJkrglMOWMWMSzKnpS2Zrn/fKVXAQs\n"
    "7B9bJHFteNOdO60hh5DT5fUWZXKOw9B08NwA9cM=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcPkcs8CamelliaPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIH1MGAGCSqGSIb3DQEFDTBTMDAGCSqGSIb3DQEFDDAjBBAS18zhH2CrOFRwXWpO\n"
    "44fBAgEDMAwGCCqGSIb3DQIJBQAwHwYLKoMIjJpLPQEBAQIEEBLPDPShOmwEgmIu\n"
    "LR/BVIAEgZByfY6vTcwLLTdkOx0hU8EtZq9l+JnATOJc8lWSKD2MQ6cXlqlWGZxn\n"
    "q1905JwhDz+Q7farZRqgU2nuPJ0QbkKLMXjHbt99fcbBHcUUymkbLXBVZa5NWZM+\n"
    "F9/RJU2XSLTpF5Dr/AM3kuLAEGdfVPCT8565p78zI7lZysSd1XaUVe76O0kXBnTh\n"
    "DMbJUO5VHhA=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaPkcs8Sha1TripleDesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIE6jAcBgoqhkiG9w0BDAEDMA4ECFQofOW0eGtvAgIIAASCBMgJVjprvKozr6PZ\n"
    "LbHZkJNPJi9sl/CAHTsrj7sP/bt69CEWrfPFUoFPayK9ILGEEBWsLd32MWjP1hnI\n"
    "rT3uOuwv4QW6cIEPy7WIDGWxgIEByI1tg6/bojG4aDnftJjLpmkNj7Xw1TOeM8BA\n"
    "hpOYK1G4YhwDTV/1+ShWnrleu+2uiUIwNufCcImRNxdIRubMIFdsTOrXR6oDpAP9\n"
    "IrViaOyjSQ15+ZQMQ2baYpm1TeFca/n9VqKrsO3JvdtfZxl/7BHVPF+9JuiToPg9\n"
    "eKNkvKDiOk3lIQ08Ic7PO+aDh1/qDRp0Q6CbQhgi+5yla4AYAhl/edmb4K0wWrAD\n"
    "hlpV6jnqagF4gHEnHxY36fQIyHcHhReqNTR33MyzDOawz3MhGfr8B1iloOjfT049\n"
    "qJ95nVNZxCRmOWmG+cbwkO+u7unogbJs1Obe06CgIVUlv+4nWghfjQ0S7g6y2gsD\n"
    "dVWLhbQCTAGjXAcQP6trd/SXXZ6/NPjqXc6xlkEtQjixKVqWw3Jet5DqZulREKcH\n"
    "LqooPFgLbQCrB8NIXEwPdwtEetjOWP7B7CDe7TOYRKPmsaw4sj6+rvyXUQ5vg1J3\n"
    "mWQCFVkhVbFEMoHBK7VAIz0t9kI2KNn6BTCxbLe7Dpz9grGPF/yXpOFf2F/sQs7o\n"
    "f9eNW6uKMRIgCr4PIdnwLiJTm5PkJxbKWkOpcw5LMIMauNi65B1bZbI6TkM99Pqe\n"
    "/euecqv/ZyaBEdKFpgVfI+Vcpilh/HjKdBpl/4YfzYkjxdz3BmPdS1PotY1ThMQ/\n"
    "77J8WaYgoifrq4Bisi8MXokHkBM37z5NvuaNuQx15C2zYtqK5hO9/LL2iqbp7gbD\n"
    "wJQjb+qYCyswMk/6FdeScf4CBAwxtbll7yGG6uMrWTCAt0Rg4HNCwWTksM9E5neX\n"
    "jQO0EWHmYq9E3HDW1ihfJqW/GMmlKNXUyQjh+eRwwaILlgXZtn9znxPq+DShv0zt\n"
    "b6ceyQv0bi1zpkqPM8ccDTO/KLbHHR7JoQlqj2vm8ML9MwqXbGcNNO0DGT15WjSP\n"
    "kig6xGMINMWGaRJZvCSmS1d38t74qbCeQNj0NV4HXHpTA7QUhOgGBpDUWvEQYGd3\n"
    "PTy+lphH8AnPgFmZ/292+m72HB5oQEJT2dMRniZT6uMbVVGeHAwFoLXmGIOzCjUg\n"
    "UxQnd4hLRdfxzg4mSgQXdWm2M4remN5AAgCW7hRiXwxsjsBqhq78GG4vDfyrKskw\n"
    "NSPKWOvlpJD59GhtH5KksntaJ2rdgiANwpBdURSgXjSoSdkUep3tCnT5yD8NrNUr\n"
    "7+m1q5DQewkq/XCeqOwoYggeul+M2hMtTG3gprpRX4LATv19xP2GRcIcSMGKOHbB\n"
    "OY7coKYCrdIGx92V0TEQrGvlQvRli997zdImnR5SJ25q9n3ayj4RslFHUJamYEVe\n"
    "zWvmpqPYg3mA4BIFj04NTDjaQ5BGHKjTK0U8gYujEM+WodyMRbEGNEhQt5ZqHjcL\n"
    "73A9hdAk/vO0VVYJQOTsNxSZ9lOhkNxku4FaoCY4toYS1dVEGZ35P6ZxlS7T5E5E\n"
    "ijudtdVWFCiX5z2MW7/hjAwl9+wQvQ8HxTuFdMT7VMJWF4ca9ih9ZFu/PJy7hLCe\n"
    "o2SC9KIZD3vEE4lPFdk=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaPkcs8Aes256Sha256Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQy6h8dzuTommLJUNw\n"
    "bmosygICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEL8nSTHEXTzJXRSN\n"
    "CVQht88EggTQj17T2ar4isghif/o5hBINtjktt/0UDUai4aiUU3Rq9RNbADzv+v/\n"
    "AlZv2xmLvOy4EVXZ6Wf9DDQ8VaqxJ8ehPmPpWeqd3Ng/b0rZaZFqtB/zriCmQo45\n"
    "Ow/P1xRCY74N3w8P3c9NEdjxroUqChodEjq0iVfgzkdXju/9f7Xz/UIM1wyDJQzj\n"
    "iw+Pkkn36lZL7ESTqgYHk2jXPxFPrpi8EFdPqjBOZMNZWRc6N24dp8Y56vlMXm5y\n"
    "b6bVPXUXKSdGGvtLb3KGXh+maJFWukoWpb1/BhLqi76vOnalxMtoVvW2dWp0UYrf\n"
    "5XD3ShuXm7FM2hy5hflbRy9oyiLcrdcEIoZ99feUWHkD9E6lRfPX8M/Q0Dx989Wb\n"
    "t1H8J/fiUEhZ+oYF/vn9zWZWwCVUQijmdjifQJXQgZeOMQmKA04kxPk3MImF3bfu\n"
    "o9+B6T37xnOAoV5MBF6sATQjbf/QC6BFFGN/exl1FySQdspVyGIJco1CpEkk8SfB\n"
    "yqpTGpeb0XVBtih6dKMAZjhP1FB45uYMccX48PwF1HdZRubc3V2WXRdmefq2KePo\n"
    "HQVD67T8PJKFG3UGq1XZIUh+TSOZWU80W/CzWlbHBZem4/Cazwy6B6SvkI3TmX6f\n"
    "jSZxS+Qfst5aWmGHkO7d2Z6EcjSw44Q0K/fJh4/qAr0mdvwJ1DEAJZOk8zIPfb6w\n"
    "OrYLgsLzoTmswbnHwmSsRYm0qZWzETvj1mUETXrIWo7hm8/EtGaOxMJf348cfkEJ\n"
    "Bp7iLEy0GL5IUXArqFAK3mxYHyd0N4r9UtQI28HbDUWdm/r8y5uBC0USiO3PCjjI\n"
    "7Nl5FGK/hC4T4ItKp07BGC4zcC43XotXaUvNjpiyOsqoEgXEXeayq+4LFGGVYNaX\n"
    "4r+WDgx6m8pPnX1LhcNKDLaYr3VkdCbOf5IsUvcbatxvS9w4/gGu1e78i+AaWrii\n"
    "YR3M9OjSTKkLxvuLWVkq4fi8mVt07Se63AB45Yi3zK2Mi8vdPBNlKkXOqS7/JNgG\n"
    "cPLUZPuVbO6Hd3sLi59e1bRuAvNmJ+MrjxFmeHa1c1I1Dseiocb/zXnCSkT7Wxv1\n"
    "rndZh6V0HdYwJvqBCo06MelS1Kri0O4DjMIciEQ8wK0qGeEX1nrJXAkLXMN4CPon\n"
    "3Z9Hvt+nAyPct4awPKuvTecuNyogBUUGYSOvywFiobQF3yzbFCJRlAV+IICNLDtJ\n"
    "CL48vxSvv6yRMNRRfdWqGjcd6V1UNVGeO7VBmLhWxxuOFzd1/N5bKaZkt/ltS7Rs\n"
    "5M5bVK+7vFImTt+PpbWqeRXh6zs0eg0ly7ai+w6AhyfAlQXWIVKzVxT6uCP3k2oj\n"
    "Y/VE/lhfnxVBU6WaOjR7ZuPv0EZpNhYooEuB0+l4w/8/d9dJnOdnPmN010eagTgu\n"
    "e3j8bwfWjaKVNdVMLa669fm1tows1/NcIzJOEwckJyFj+aq/7P089+jAjBB7C96v\n"
    "mG2EIsxWShM+De4r7t5HJN5/Rh0cnijDihAeJ1dV9P+F6ciMkl5t3WBrZb3FOGvc\n"
    "A3nU0doF7NOVlDd1gHMTyz3kFeqVifkzIVvY+r6QOt/e0rE5Af0M7oKQY8AlnCEN\n"
    "4yLghb4lJZm34or5Ytv2/ISULM5bwDFwC4iEMlH6fcEUvyOsNyWAkK0=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char Ec256TraditionalPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIOZ0M+PsasxBSfk/jJ+dOU1Bdgox1HuCiVjtl263ICRioAoGCCqGSM49\n"
    "AwEHoUQDQgAEUpDDG/3eJ9WJVuhEq5TaIe2McFF3dNTmAPJ2qztFmM11iKzCDcvm\n"
    "a07gSpW8Nd08VMf3npszrIKJU0Ar/ucVLw==\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char Ec256Pkcs8Pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5nQz4+xqzEFJ+T+M\n"
    "n505TUF2CjHUe4KJWO2XbrcgJGKhRANCAARSkMMb/d4n1YlW6ESrlNoh7YxwUXd0\n"
    "1OYA8narO0WYzXWIrMINy+ZrTuBKlbw13TxUx/eemzOsgolTQCv+5xUv\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Ec256PublicPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEUpDDG/3eJ9WJVuhEq5TaIe2McFF3\n"
    "dNTmAPJ2qztFmM11iKzCDcvma07gSpW8Nd08VMf3npszrIKJU0Ar/ucVLw==\n"
    "-----END PUBLIC KEY-----\n";

constexpr char Ec384TraditionalPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MIGkAgEBBDB+L65HRJai1swlPMbKL/9g0g5AUI5O0xR9p3RdmWe5uSAxlKhcJzgB\n"
    "tZaKrsD/kIOgBwYFK4EEACKhZANiAASxbuwhNc9XvYhcTA+B4wzhuMEYWt0QiQvL\n"
    "EGG6BSkMj7v4EYCxJMfyNsp8oG+wVdA21ZhTl0HFImjTw27GZbd+ILM9Ll7BRmfI\n"
    "zGsBjgjQehRAfaYCz7mg+RSzoYzxkG0=\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char Ec384Pkcs8Pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIG2AgEAMBAGByqGSM49AgEGBSuBBAAiBIGeMIGbAgEBBDB+L65HRJai1swlPMbK\n"
    "L/9g0g5AUI5O0xR9p3RdmWe5uSAxlKhcJzgBtZaKrsD/kIOhZANiAASxbuwhNc9X\n"
    "vYhcTA+B4wzhuMEYWt0QiQvLEGG6BSkMj7v4EYCxJMfyNsp8oG+wVdA21ZhTl0HF\n"
    "ImjTw27GZbd+ILM9Ll7BRmfIzGsBjgjQehRAfaYCz7mg+RSzoYzxkG0=\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Ec384PublicPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEsW7sITXPV72IXEwPgeMM4bjBGFrdEIkL\n"
    "yxBhugUpDI+7+BGAsSTH8jbKfKBvsFXQNtWYU5dBxSJo08NuxmW3fiCzPS5ewUZn\n"
    "yMxrAY4I0HoUQH2mAs+5oPkUs6GM8ZBt\n"
    "-----END PUBLIC KEY-----\n";

constexpr char Ec256Pkcs8InRsaLabelPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5nQz4+xqzEFJ+T+M\n"
    "n505TUF2CjHUe4KJWO2XbrcgJGKhRANCAARSkMMb/d4n1YlW6ESrlNoh7YxwUXd0\n"
    "1OYA8narO0WYzXWIrMINy+ZrTuBKlbw13TxUx/eemzOsgolTQCv+5xUv\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char RsaPkcs8InEcLabelPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQCdIua+yJO2bUqF\n"
    "LBoWaf2RtLuvy3Y5qR+kNWVWwDf/snFp7/a0d0/dEFe1my2T+aC5g4ERUSpkLhbR\n"
    "RmYM66uG7cl6q1UIyOFExsH97xWrChU9VpwYJZJii7krGDZyC7C3zdsT4CINeT11\n"
    "TJCWpScAeBmJooLu+nkN3GemS5yMRGOml4svb9xrr3ibcPbOy6xLXQ4NPL53W3pu\n"
    "P3MEfwCnRufLfzCySERHoYZHgsNfzWIoeswODt/NwskHaDSm7QxzEirlWoew/Cma\n"
    "oyAgRBEci95YFTbh/AqkwQjx9POMMPRdwDYMXP1zlJ9Z3vsyiAJK2CXqH26PvwGc\n"
    "Vq8Ytk5tAgMBAAECggEABNm587OvU5dGEJxO5dg5upCwpnmKWbdNwbzgNvBSCjOQ\n"
    "6qs8m09/iJxjiLiUjJfNDUzw4/70ZwlBLQ6WQdnYfp7CdGXrhwzkX0IfZN5HdfMm\n"
    "urD8uoYWYbJccecabyTLPLE/DPsDMhVG/2Lwp6XLcWlDqIr6rbPPnszp6ik1WOML\n"
    "JCFgcsp5NSZT+yvYUeS7qoSGNgDIvhkqu1NzLnQJMOysKliCmAAhJJC47qOWo7Wp\n"
    "YWqZzJNYfRFSOLfne0aaPrYIjRiHUMt9a/4ine0OhCXVgVU0i26vdcG9UTbC2rfd\n"
    "Dhd0daMGq6jCI7V1evEyeIG2iuxE2zjlaT1qWX4sZQKBgQDVu+cE3wd11cNxb1c5\n"
    "CbLKWnH09gVHZziqDv0q598uw9y/Rv54iFu00OgtSKLJpdiGQF9BS/G4nFyWZlov\n"
    "9PYfJbSMixth5lH4hxT9qdCi5fQtOy/tvrOqeqj7mV7zAvp3Eti5tzG5vwzoguhH\n"
    "DgNlUQr3/4rFyERC9xRDeBq1CwKBgQC8NcqlbEHuanMI4XYOid4YUtcDAVM16Ekt\n"
    "syKUz2eNbsH4DHPyLuTzF9Uf4NFjOhuyY97mWrVg2FX2pFQ5sdPO5ExgE3+Bv64W\n"
    "UHqyNbOIYdOXZzbLxyEIqoL5GtJ7WarwdDhHLBtoUQL+iAzzngqyx2nV4yNuABOH\n"
    "1Z8aZbLFZwKBgQCspgNAGRTCp7EfkWvopB/re1HwKUcsXYIVRNRWc0UCVq6bSDLI\n"
    "lIVXsyTJZ5Ivxyr3j0oiycDxekUELJuPf5XMpT2KBdhRpaXzVtKq8SCaZZxvaQ8X\n"
    "QeOJw1l7G3HqZS6DPxMYOZYOT/iSXSRaOUSe8+8jUyX7DAGwAjw5H+ya7wKBgQCh\n"
    "AWNaIEJGssm1zyovRcG5DSTibT/B3fS8I4JxyvvLAfTtPbnydCtnyfuIScrWF9+Z\n"
    "u9XzOaXwMyFW3T0wHfAUDTlI92Ucmd1TOYW87jOneAiKfMFjkfWwdC2W7FsjKpIR\n"
    "IVsDh7fqhWMKW1i5D3oXdoATeTvV3jrW+OgirN1CAQKBgQCset5MY7jAoSXgB9iY\n"
    "44mzJsvzFbbHnnsQQvxI6T+aikZg/dU2fkHwF8Ar56qBuOinp8Tg9kOnafftrjTZ\n"
    "vhSlzslCsicalnAiUT/1sNNcVBaRLpmfLQCdxCM4mljZpjJ3Gdv+6OUS4BxJPIZB\n"
    "AgZqaf3dCsy28HfnV0FUlpQkgw==\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char CertificatePem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "QUJDREVG\n"
    "-----END CERTIFICATE-----\n";

constexpr char RsaPublicKeyLabelPem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "QUJDREVG\n"
    "-----END RSA PUBLIC KEY-----\n";

constexpr char RsaPssPrivateKeyLabelPem[] =
    "-----BEGIN RSA-PSS PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END RSA-PSS PRIVATE KEY-----\n";

constexpr char UnknownPrivateKeyLabelPem[] =
    "-----BEGIN FOO PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END FOO PRIVATE KEY-----\n";

constexpr char Ed25519PrivateKeyLabelPem[] =
    "-----BEGIN ED25519 PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END ED25519 PRIVATE KEY-----\n";

constexpr char DsaPrivateKeyLabelPem[] =
    "-----BEGIN DSA PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END DSA PRIVATE KEY-----\n";

constexpr int DsaType = 116;
constexpr int DsaLibrary = 10;
constexpr int ErrorReasonDsaLibrary = 10;
constexpr int DsaBadQValue = 102;
constexpr int DsaModulusTooLarge = 103;
constexpr int DsaDecodeError = 104;
constexpr int DsaParameterEncodingError = 105;
constexpr int DsaInvalidParameters = 112;
constexpr int EvpExpectingAnRsaKey = 127;
constexpr int EvpPrivateKeyDecodeError = 145;

constexpr char Dsa1024TraditionalPem[] =
    "-----BEGIN DSA PRIVATE KEY-----\n"
    "MIIBuwIBAAKBgQDnLP8Yu1+Rebf/2ZRWwL7uFl+7kRfD5uxRBgW+Wu1CMcZm1KwT\n"
    "DJpx4C3+sltT55b0zoWQV53wdTaSPQ58Azk+R3qI/oxD+YL90KIPXgNnX3ZH3BK1\n"
    "cSH7Mh1Tcf3m/Z/1jpbN1pSzeaRPldRubhJv+zGPtPDXAXR+hXjzVwRbeQIVAP97\n"
    "oO7+/pk0fU0b4VGWQ44HPoexAoGBAJphVuqJe1flkwXJX4vSY8AH+iXku1h2kByu\n"
    "64I26QmiaEvbQ6ETcKqJj8KJlQwqGMeKIe7PKHXQA6k4p4ve/Gx0H6+u4vYbzTgr\n"
    "MCv4d62vZkGKWwBu56jAbaO071Uecl4JkF6eyf6pi5NZoLOTzpdURnbaSTYRHWlV\n"
    "Dl+8x/aaAoGADzHe1puObV00zBbFAUbqCXmBmLT+5EAgKsLtgpbfrfyKVt2+fnN7\n"
    "hoM8wwmrOJ4o0JsCktSkArh6vQCXkIuZq945K79/IXW930MsJvYAYqcADXks3vFG\n"
    "iEaCsjBrgAIWO4rZ4dFGXXf+dcGOpuYXA42AOSEYNjxfCaSDpCkXG6MCFBHaY0az\n"
    "IIcK7CBMBLMgSeTxGd/2\n"
    "-----END DSA PRIVATE KEY-----\n";

constexpr char Dsa1024Pkcs8Pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIBSwIBADCCASwGByqGSM44BAEwggEfAoGBAOcs/xi7X5F5t//ZlFbAvu4WX7uR\n"
    "F8Pm7FEGBb5a7UIxxmbUrBMMmnHgLf6yW1PnlvTOhZBXnfB1NpI9DnwDOT5Heoj+\n"
    "jEP5gv3Qog9eA2dfdkfcErVxIfsyHVNx/eb9n/WOls3WlLN5pE+V1G5uEm/7MY+0\n"
    "8NcBdH6FePNXBFt5AhUA/3ug7v7+mTR9TRvhUZZDjgc+h7ECgYEAmmFW6ol7V+WT\n"
    "Bclfi9JjwAf6JeS7WHaQHK7rgjbpCaJoS9tDoRNwqomPwomVDCoYx4oh7s8oddAD\n"
    "qTini978bHQfr67i9hvNOCswK/h3ra9mQYpbAG7nqMBto7TvVR5yXgmQXp7J/qmL\n"
    "k1mgs5POl1RGdtpJNhEdaVUOX7zH9poEFgIUEdpjRrMghwrsIEwEsyBJ5PEZ3/Y=\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Dsa2048TraditionalPem[] =
    "-----BEGIN DSA PRIVATE KEY-----\n"
    "MIIDVQIBAAKCAQEAx/4oM9wIPOUoxx0VwoGtR5Dr6SokUUfrlWzWFGa8bwO5CJLM\n"
    "Lq2aaNThCkIjdaR1BOJLyJ68GSRrheqM9bkZYTKiJLiq1njjhmMD/T2WQAs22Uck\n"
    "PRwpKeOpaHk+BfPPPdI9qW5jhQD+4Sa0mVqzjidovilNEDYDzJJ1QvG0LzK2hLIj\n"
    "h2aRNHdB0tCGNmjLIH+zn9/EgqMo5Bmkqn5vFWfCeZQaYlqqJvLXyNcV4r4fSJwh\n"
    "IPDmokd4FD1xsg26Gt+Ikv0eOzuHxZ4YxvIsU50TcR5Q/wR5TZ07zfrfgMWCVn3Z\n"
    "rvtc6vi/3gfFwcm7Tuyu4ei0/pocjXtIMVKrtQIhANl20GM0MxjqbK0n+5ZnzsmF\n"
    "6f2AdKGVGNjg59AjmHwhAoIBAFsijJi2YpALVhPl//y+yDc71k/IldryBazeJn/0\n"
    "XTpNndFNuINiiHsHaLHsw5Epl1/AQGbWUDDpvrz+FE+5hQyfRePBXe3HFcg09H1f\n"
    "KTDR1K9XWJhyJrTUim09oheLaFANNpWevftQ6ZbXoKbH+VWFSLQjzXg5ZSJfVidB\n"
    "huS6mmDcX1qNNW1vvSVQrKz5fcwv1ZDRE1rswnrN/QNc5B1lhTGZWg5kEWrSIWKC\n"
    "qN+cdzlbMmEuC1173JriIMQdVhWWX+CXlUE797YiDCViePf20syIU25rgINe5L4c\n"
    "E7FBfbbe5ycf9b+WvUa3eq7FPtbvLgz+cwOCP2HBZl7fvHwCggEAdGaa4o0zOR74\n"
    "3sdhG7WOe/odD8GECZpT+WibrN4aLAGdvmqCW759uHxgXJjNnsNa0lmdIgaNPOy4\n"
    "++5UO7jI5fHE+Eahrd6vCLYu05prrG43pHkZJ9VIhfI62FcDn0moVvKm5VqZ0izl\n"
    "1/ZB3Xl3XK7VetShwJltmZ1xmGj3yK+BYw0BGKDe48a/diZ5tZ99x6jWFT47NdV6\n"
    "OlUVkZe0ZjCcE301dZPPeilU9dv4eeQ9uxd+zzM9CgwIwreX57I6ShDebLRc6bZw\n"
    "Doy/hVxavvX2qhlAcnlrxI/Q2uzChHMiUFk8LRNXUTfVEFn/g/l2ogjGF7utWgTO\n"
    "lpRjY3r82gIgPgORWYgb6BlUFuQvZaRcOm1L23p0VAov0LmRRdWnnlg=\n"
    "-----END DSA PRIVATE KEY-----\n";

constexpr char Dsa2048Pkcs8Pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIICZAIBADCCAjkGByqGSM44BAEwggIsAoIBAQDH/igz3Ag85SjHHRXCga1HkOvp\n"
    "KiRRR+uVbNYUZrxvA7kIkswurZpo1OEKQiN1pHUE4kvInrwZJGuF6oz1uRlhMqIk\n"
    "uKrWeOOGYwP9PZZACzbZRyQ9HCkp46loeT4F88890j2pbmOFAP7hJrSZWrOOJ2i+\n"
    "KU0QNgPMknVC8bQvMraEsiOHZpE0d0HS0IY2aMsgf7Of38SCoyjkGaSqfm8VZ8J5\n"
    "lBpiWqom8tfI1xXivh9InCEg8OaiR3gUPXGyDboa34iS/R47O4fFnhjG8ixTnRNx\n"
    "HlD/BHlNnTvN+t+AxYJWfdmu+1zq+L/eB8XBybtO7K7h6LT+mhyNe0gxUqu1AiEA\n"
    "2XbQYzQzGOpsrSf7lmfOyYXp/YB0oZUY2ODn0COYfCECggEAWyKMmLZikAtWE+X/\n"
    "/L7INzvWT8iV2vIFrN4mf/RdOk2d0U24g2KIewdosezDkSmXX8BAZtZQMOm+vP4U\n"
    "T7mFDJ9F48Fd7ccVyDT0fV8pMNHUr1dYmHImtNSKbT2iF4toUA02lZ69+1Dplteg\n"
    "psf5VYVItCPNeDllIl9WJ0GG5LqaYNxfWo01bW+9JVCsrPl9zC/VkNETWuzCes39\n"
    "A1zkHWWFMZlaDmQRatIhYoKo35x3OVsyYS4LXXvcmuIgxB1WFZZf4JeVQTv3tiIM\n"
    "JWJ49/bSzIhTbmuAg17kvhwTsUF9tt7nJx/1v5a9Rrd6rsU+1u8uDP5zA4I/YcFm\n"
    "Xt+8fAQiAiA+A5FZiBvoGVQW5C9lpFw6bUvbenRUCi/QuZFF1aeeWA==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Dsa1024TraditionalAes256Pem[] =
    "-----BEGIN DSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-256-CBC,5FD847E72B204FE6F689A63CE601611E\n"
    "\n"
    "/PPE1ORmXrtThQckqXiyEZteS6DyDz3tapPrWo3iFBxknErAG8oPL9Fl0BnM9G/U\n"
    "w9g0Jrn4utzUDA5AZq4Id3Kxg8zw1c9V0F5z5iK9O94Yywuc7R+uVhs7ZZcwdUvp\n"
    "Ua5gg+/zCgPrVdKbyeL6pakR1dmI3c+e0qKZq7UeWjKSsKPTue2XeP19m2b8z3Sg\n"
    "e0rEMP5+nSZCX0s7o06i2ghzMdpFycswZCnZHtbItvV0bvYTRPy2bZM/YgMvrdT8\n"
    "pv+w9PFuWRfdHQ6eZ9D+I/F6Yywb3DDw30QDh5JoS5hUVNyMDi7063uss6vFxxSg\n"
    "lDYC8J4N1+pMMGgN1TioxEEktFYmLvCJPnEdlzfp9yoIfqkx14SVxtIExb+A0WoY\n"
    "YSeaIKygvykLXW7+qIBAqA7stIx7OlKwG2Xykr2jegwF4Jyu5VZxFHJVsrQoEqP7\n"
    "88/OqmCx+sbgqR7UndElg9/Yy0nF0kjFrbY21uAm6ffxxHQgDtd/DAwApVG1O4Nq\n"
    "aEKmAF85agpb5/zUh9RdyD1jN9G3LMtQJ9uhOVi6yCJfqhfZ0ckUnxsuYCDywpdT\n"
    "u0IYSbqCTpKbGdEx+iw+wA==\n"
    "-----END DSA PRIVATE KEY-----\n";

constexpr char Dsa1024TraditionalDes3Pem[] =
    "-----BEGIN DSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: DES-EDE3-CBC,9C0FBD781A2A6955\n"
    "\n"
    "rJFQxk27d2OZltgCw/poargbzSVepVw9I9rvyNC3O2PvYOE8NNHvOVCMKw/TCfOK\n"
    "iI+4dhrG/U4dnLchcgjmglLcRzDBjzbcXxqOwpxVF2d9tR6d4xmn7CyGmhcmuRGw\n"
    "Eo+TEv5B1ZiO2+i0vn2ym9HEp3Wyd4/W9xGEyxLwyP6TEjoC2NKuJB6BgfSEqrqj\n"
    "aip/0yp2eZdgXqAsP5nN4RqJqh7U6Tx5Vhc7NrO5XZDCa7BkIQGUV2oG/Bnejm6m\n"
    "1aR6RnV3tCzah9wULhtWW3fQQnSDKx92t9bNBP+t2s6WwGsCGRRcaS/l/V1frJxp\n"
    "pn+0tGcoQXpAi4tT0DitjcoLCzWZgm3WFArV+Xbl7Ak/Qi11p1bbhQmI3y2X5wN8\n"
    "F95WAktjdkpzyQepjt5v0CdTqMs5qKi0kj5ZJkAx7EPFzDBg35KkTtofqbJUNm/T\n"
    "NONn0vlWtpLiD4LCa2CzKr++Vsx82GrWUS0hkIZY+2xCP8Ft5eC60GUSZELrbTXK\n"
    "bJmpA1+Zq5x+c7QsNR2RGlS6tc/hsVium2AdiWqlRckid8WV7Qb20yUr84Jg106N\n"
    "6IFKcMO0CCEzVNnNe2CXrw==\n"
    "-----END DSA PRIVATE KEY-----\n";

constexpr char Dsa1024Pkcs8Aes128Pem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIBtTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQpY4z+k0lnAWoe7oH\n"
    "o0b43gICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEAQIEEIM2MqlT8rsZGWzS\n"
    "6iNrzGoEggFQl4XrMe5xxiXNQTYUNd1bbTGAhPV4/+5jhL2+EsKEono0KsZUwVoP\n"
    "2bAOTCsXt9cnoVtXLOYZQ1YHhJI3ZlK/4ZHAjlC9IhfvVYVcCIcO25N4JrhdCND3\n"
    "DylWBaoroZ/yeplGi32iEIDNEZwfp3T8hgE8T8Uv8HmWG0rEH+5RcalLsTJl1e4X\n"
    "pfzzG1q+YOICXbJlqFYhf9Ko5wps7biNwgAd0D9//xnyfIa9bk6/CmXluN6ww0tZ\n"
    "TNJtEhdLQHH3LUImpRGYjN5/WZi6fZQUCDBffTdi/GvOywbsBmDArSJNaQxUepmg\n"
    "Gs6Qe4193+8a+8SmtDZIzXmK+ebSa8y5etMy33kkumPihjfpPo4jqKWUblEsZRCR\n"
    "+oXT9If7+LYiO9BkshRuC24r85EAX81rbV51i+i3HHdgYKEaXdvJYRA6v3+r5uF/\n"
    "hOd+PF5Ab2M1\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char Dsa1024Pkcs8Sha1TripleDesPem[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIBcjAcBgoqhkiG9w0BDAEDMA4ECHURDj3vujHWAgIIAASCAVCn61UgTGEfXyyM\n"
    "+Xmf77AdDnm0tYVZNYis2glm4MRCTt7iPFvQwFrunRAHAxnqyycPe30nw1Z00TRk\n"
    "H6GKrlOvH6YVgOTdjzBfKmGC+nCECZnFXBcItNuA+MMnkZ9GRFIWrAXF6SkbAwif\n"
    "Glq4B8/5gx09si1ZUufNOd+yFK+KhSOIynhGu61hcY0aWnmt3gi8KTxz0i/kTlRW\n"
    "9EEzCxvm0G0+S9tULOuvdRsY1UQU2q1SNjGPPTdYDGblxHCbe2Zm7UphYuMsf/2W\n"
    "GxskDzp6vXJU9HzWwKuaAZwsKRDQ+W1cDvsleoCIUdCU9yYiHnzanZxsxHnxxCUb\n"
    "ax3Eox3oDk0PcEgMkdyBOKGCXO9h40B+fV5xB22mpZHkSbUkJkVweQEoO6QkCPYM\n"
    "lMCY1bVe7P2zyG7VaojjStUdXEPn+hQ5ESU7erZGdVjc/UQJCSY=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char Dsa1024SubprimeHex[] = "FF7BA0EEFEFE99347D4D1BE15196438E073E87B1";
constexpr char Dsa1024PrivateHex[] = "11DA6346B320870AEC204C04B32049E4F119DFF6";
constexpr char Dsa2048SubprimeHex[] = "D976D063343318EA6CAD27FB9667CEC985E9FD8074A19518D8E0E7D023987C21";
constexpr char Dsa2048PrivateHex[] = "3E039159881BE8195416E42F65A45C3A6D4BDB7A74540A2FD0B99145D5A79E58";
constexpr char DsaCompositePrimeHex[] =
    "DC02C0326C091FBA462384F3F14BD417C8475FB701BC0B63B6B6888E8E08776309D43785BD25C077053B4F0DA6F44B57"
    "6FFE2DAEB426EB4D219AAB0C3C861C0B";
constexpr char DsaCompositeSubprimeHex[] = "84587638E081D1B26860D43A2BAD7D6415E909DF";
constexpr char DsaCompositeGeneratorHex[] =
    "175D9FA6A5E521DC453C0C1D8EFDCE03EBC6C4C5F388476D2B0ED7E936D4ADAE86F10940D6B9F06DD7012104A0B25173"
    "84D1F87C0F119016D10BBBB94D4FE7E0";
constexpr char DsaCompositePublicHex[] =
    "7B1BF5192D6DCB36653958AF55E2E42EE5CE4324B260673AFA18D76F5FD0C012D82C5903AE145D9E9F8CDBC81DAE160B"
    "21356DF34F9302B4EFFAB8947436F10C";
constexpr char DsaCompositePrivateHex[] = "37B1292AE599FDBA5B9B1ED795FAF39E6627D239";

constexpr char CorruptEcPrivateKeyPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char UnknownHeaderPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Foo: bar\n"
    "\n"
    "QUJDREVG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char MicOnlyHeaderPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,MIC-ONLY\n"
    "\n"
    "QUJDREVG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char MissingDekInfoPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "\n"
    "QUJDREVG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char WrongVersionHeaderPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 5,ENCRYPTED\n"
    "\n"
    "QUJDREVG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char MissingCommaHeaderPem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4;ENCRYPTED\n"
    "\n"
    "QUJDREVG\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char EmptyRecordPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "-----END PRIVATE KEY-----\n";

constexpr char GarbagePkcs8RecordPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "QUJDREVG\n"
    "-----END PRIVATE KEY-----\n";

int passwordCallbackCalls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "requirement failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

int APS5_VABI CountingPasswordCallback(char* buffer, int size, int readFlag, void* userData) {
    static_cast<void>(buffer);
    static_cast<void>(readFlag);
    static_cast<void>(userData);
    ++passwordCallbackCalls;
    return size;
}

std::vector<unsigned char> BytesOf(const char* text) {
    return std::vector<unsigned char>(text, text + std::strlen(text));
}

std::vector<unsigned char> BodyBytes(const std::vector<unsigned char>& pem) {
    const std::string text(pem.begin(), pem.end());
    const std::size_t begin = text.find("-----\n");
    Require(begin != std::string::npos);
    const std::size_t end = text.find("-----END", begin + 6);
    Require(end != std::string::npos);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<unsigned char> bytes;
    unsigned int accumulator = 0;
    int group = 0;
    for (std::size_t index = begin + 6; index < end; ++index) {
        const char character = text[index];
        if (character == '\n' || character == '\r' || character == ' ' || character == '\t') continue;
        if (character == '=') break;
        const std::size_t value = alphabet.find(character);
        Require(value != std::string::npos);
        accumulator = accumulator << 6 | static_cast<unsigned int>(value);
        ++group;
        if (group == 4) {
            bytes.push_back(static_cast<unsigned char>((accumulator >> 16) & 0xff));
            bytes.push_back(static_cast<unsigned char>((accumulator >> 8) & 0xff));
            bytes.push_back(static_cast<unsigned char>(accumulator & 0xff));
            accumulator = 0;
            group = 0;
        }
    }
    Require(group != 1);
    if (group == 2) bytes.push_back(static_cast<unsigned char>((accumulator >> 4) & 0xff));
    if (group == 3) {
        bytes.push_back(static_cast<unsigned char>((accumulator >> 10) & 0xff));
        bytes.push_back(static_cast<unsigned char>((accumulator >> 2) & 0xff));
    }
    return bytes;
}

std::vector<unsigned char> EncodePemRecord(const std::string& label, const std::vector<unsigned char>& der) {
    constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text = "-----BEGIN " + label + "-----\n";
    std::size_t col = 0;
    for (std::size_t i = 0; i < der.size(); i += 3) {
        const unsigned int b0 = der[i];
        const unsigned int b1 = (i + 1 < der.size()) ? der[i + 1] : 0;
        const unsigned int b2 = (i + 2 < der.size()) ? der[i + 2] : 0;
        const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
        text.push_back(Alphabet[(triple >> 18) & 0x3f]);
        text.push_back(Alphabet[(triple >> 12) & 0x3f]);
        text.push_back((i + 1 < der.size()) ? Alphabet[(triple >> 6) & 0x3f] : '=');
        text.push_back((i + 2 < der.size()) ? Alphabet[triple & 0x3f] : '=');
        col += 4;
        if (col == 64 && i + 3 < der.size()) {
            text.push_back('\n');
            col = 0;
        }
    }
    text += "\n-----END " + label + "-----\n";
    return std::vector<unsigned char>(text.begin(), text.end());
}

void ExpectErrorCode(int expectedLibrary, int expectedReason,
    std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    Require(code != 0, location);
    const int library = static_cast<int>((code >> 24) & 0xff);
    const int reason = static_cast<int>(code & 0xfff);
    Require(library == expectedLibrary && reason == expectedReason, location);
}

void ExpectLastError(int expectedLibrary, int expectedReason,
    std::source_location location = std::source_location::current()) {
    std::uint64_t last = 0;
    std::uint64_t code = ERR_get_error_nid_postfix();
    Require(code != 0, location);
    while (code != 0) {
        last = code;
        code = ERR_get_error_nid_postfix();
    }
    const int library = static_cast<int>((last >> 24) & 0xff);
    const int reason = static_cast<int>(last & 0xfff);
    Require(library == expectedLibrary && reason == expectedReason, location);
}

void ExpectErrorSequence(const std::vector<std::pair<int, int>>& expected,
    std::source_location location = std::source_location::current()) {
    for (const auto& [expectedLibrary, expectedReason] : expected) ExpectErrorCode(expectedLibrary, expectedReason, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

void ExpectEmptyQueue(std::source_location location = std::source_location::current()) {
    Require(ERR_get_error_nid_postfix() == 0, location);
}

LibreSsl::Bio* MemoryBio(const std::vector<unsigned char>& bytes) {
    auto* bio = BIO_new_mem_buf_nid_postfix(bytes.data(), static_cast<int>(bytes.size()));
    Require(bio != nullptr);
    return bio;
}

LibreSsl3EvpPkey* ReadPrivateKey(const std::vector<unsigned char>& pem, LibreSsl3EvpPkey** slot,
    LibreSslPemPasswordCallback callback, void* userData) {
    LibreSsl::Bio* bio = MemoryBio(pem);
    auto* key = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bio, slot, callback, userData);
    BIO_free_nid_postfix(bio);
    return key;
}

LibreSsl3EvpPkey* ReadPublicKey(const std::vector<unsigned char>& pem, LibreSsl3EvpPkey** slot) {
    LibreSsl::Bio* bio = MemoryBio(pem);
    auto* key = PEM_read_bio_PUBKEY_nid_postfix(bio, slot, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    return key;
}

std::vector<unsigned char> WrittenPrivateBytes(LibreSsl3EvpPkey* key, const LibreSsl3EvpCipher* cipher) {
    auto* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr);
    Require(PEM_write_bio_PrivateKey_nid_postfix(bio, key, cipher, nullptr, 0, nullptr, nullptr) == 1);
    char* pointer = nullptr;
    const std::int64_t length = BIO_ctrl_nid_postfix(bio, BioCommandMemPointer, 0, &pointer);
    Require(length > 0 && pointer != nullptr);
    std::vector<unsigned char> written(pointer, pointer + length);
    BIO_free_nid_postfix(bio);
    return written;
}

LibreSsl3EvpPkey* KeyFromPkcs8Der(const std::vector<unsigned char>& der) {
    const unsigned char* cursor = der.data();
    auto* info = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    Require(info != nullptr);
    Require(cursor == der.data() + der.size());
    auto* key = EVP_PKCS82PKEY_nid_postfix(info);
    Require(key != nullptr);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(info);
    return key;
}

LibreSsl3EvpPkey* KeyFromDerThroughBio(const std::vector<unsigned char>& der) {
    LibreSsl::Bio* bio = MemoryBio(der);
    auto* key = d2i_PrivateKey_bio_nid_postfix(bio, nullptr);
    Require(key != nullptr);
    BIO_free_nid_postfix(bio);
    return key;
}

LibreSsl3EvpPkey* KeyFromSpkiDer(const std::vector<unsigned char>& der) {
    const unsigned char* cursor = der.data();
    auto* key = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    Require(key != nullptr);
    Require(cursor == der.data() + der.size());
    return key;
}

void CompareBignums(const void* first, const void* second,
    std::source_location location = std::source_location::current()) {
    Require(first != nullptr && second != nullptr, location);
    const auto* left = static_cast<const Bignum*>(first);
    const auto* right = static_cast<const Bignum*>(second);
    Require(BN_is_negative_nid_postfix(left) == BN_is_negative_nid_postfix(right), location);
    const int leftBits = BN_num_bits_nid_postfix(left);
    const int rightBits = BN_num_bits_nid_postfix(right);
    Require(leftBits >= 0 && leftBits == rightBits, location);
    const std::size_t byteCount = static_cast<std::size_t>((leftBits + 7) / 8);
    std::vector<unsigned char> leftBytes(byteCount);
    std::vector<unsigned char> rightBytes(byteCount);
    if (byteCount > 0) {
        Require(BN_bn2bin_nid_postfix(left, leftBytes.data()) == static_cast<int>(byteCount), location);
        Require(BN_bn2bin_nid_postfix(right, rightBytes.data()) == static_cast<int>(byteCount), location);
    }
    Require(std::memcmp(leftBytes.data(), rightBytes.data(), byteCount) == 0, location);
}

void CompareRsa(const LibreSsl3Rsa* first, const LibreSsl3Rsa* second, bool privateComponents,
    std::source_location location = std::source_location::current()) {
    Require(first != nullptr && second != nullptr, location);
    const void* firstModulus = nullptr;
    const void* firstPublic = nullptr;
    const void* firstPrivate = nullptr;
    const void* secondModulus = nullptr;
    const void* secondPublic = nullptr;
    const void* secondPrivate = nullptr;
    RSA_get0_key_nid_postfix(first, &firstModulus, &firstPublic, &firstPrivate);
    RSA_get0_key_nid_postfix(second, &secondModulus, &secondPublic, &secondPrivate);
    CompareBignums(firstModulus, secondModulus, location);
    CompareBignums(firstPublic, secondPublic, location);
    if (!privateComponents) {
        Require(firstPrivate == nullptr && secondPrivate == nullptr, location);
        return;
    }
    CompareBignums(firstPrivate, secondPrivate, location);
    const void* firstP = nullptr;
    const void* firstQ = nullptr;
    const void* secondP = nullptr;
    const void* secondQ = nullptr;
    RSA_get0_factors_nid_postfix(first, &firstP, &firstQ);
    RSA_get0_factors_nid_postfix(second, &secondP, &secondQ);
    CompareBignums(firstP, secondP, location);
    CompareBignums(firstQ, secondQ, location);
    const void* firstDmp1 = nullptr;
    const void* firstDmq1 = nullptr;
    const void* firstIqmp = nullptr;
    const void* secondDmp1 = nullptr;
    const void* secondDmq1 = nullptr;
    const void* secondIqmp = nullptr;
    RSA_get0_crt_params_nid_postfix(first, &firstDmp1, &firstDmq1, &firstIqmp);
    RSA_get0_crt_params_nid_postfix(second, &secondDmp1, &secondDmq1, &secondIqmp);
    CompareBignums(firstDmp1, secondDmp1, location);
    CompareBignums(firstDmq1, secondDmq1, location);
    CompareBignums(firstIqmp, secondIqmp, location);
}

std::vector<unsigned char> PublicPointBytes(const EcKey* ec) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(ec);
    const EcPoint* point = EC_KEY_get0_public_key_nid_postfix(ec);
    Require(group != nullptr && point != nullptr);
    std::vector<unsigned char> out(1 + 2 * 66);
    const std::size_t size = EC_POINT_point2oct_nid_postfix(
        group, point, PointConversionUncompressed, out.data(), out.size(), nullptr);
    Require(size != 0);
    out.resize(size);
    return out;
}

void ComparePublicRsa(const LibreSsl3Rsa* first, const LibreSsl3Rsa* second,
    std::source_location location = std::source_location::current()) {
    Require(first != nullptr && second != nullptr, location);
    const void* firstModulus = nullptr;
    const void* firstPublic = nullptr;
    const void* secondModulus = nullptr;
    const void* secondPublic = nullptr;
    RSA_get0_key_nid_postfix(first, &firstModulus, &firstPublic, nullptr);
    RSA_get0_key_nid_postfix(second, &secondModulus, &secondPublic, nullptr);
    CompareBignums(firstModulus, secondModulus, location);
    CompareBignums(firstPublic, secondPublic, location);
}

void CompareEc(const EcKey* first, const EcKey* second, int curve, bool privateScalar,
    std::source_location location = std::source_location::current()) {
    Require(first != nullptr && second != nullptr, location);
    Require(first->group != nullptr && second->group != nullptr, location);
    Require(first->group->curve.nid == curve, location);
    Require(second->group->curve.nid == curve, location);
    Require(PublicPointBytes(first) == PublicPointBytes(second), location);
    if (!privateScalar) return;
    Require(first->privateKey != nullptr && second->privateKey != nullptr, location);
    Require(mbedtls_mpi_cmp_mpi(&first->privateKey->value, &second->privateKey->value) == 0, location);
}

void TestRsaPrivateFormats() {
    const std::vector<unsigned char> pkcs8 = BytesOf(RsaPkcs8Pem);
    const std::vector<unsigned char> traditional = BytesOf(RsaTraditionalPem);
    auto* reference = KeyFromPkcs8Der(BodyBytes(pkcs8));
    Require(reference->type == RsaType);

    auto* pkcs8Key = ReadPrivateKey(pkcs8, nullptr, nullptr, nullptr);
    Require(pkcs8Key != nullptr && pkcs8Key->type == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(pkcs8Key->key), static_cast<const LibreSsl3Rsa*>(reference->key), true);

    auto* traditionalKey = ReadPrivateKey(traditional, nullptr, nullptr, nullptr);
    Require(traditionalKey != nullptr && traditionalKey->type == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(traditionalKey->key), static_cast<const LibreSsl3Rsa*>(reference->key),
        true);

    auto* derKey = KeyFromDerThroughBio(BodyBytes(traditional));
    Require(derKey->type == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(derKey->key), static_cast<const LibreSsl3Rsa*>(traditionalKey->key),
        true);

    const std::vector<unsigned char> written = WrittenPrivateBytes(traditionalKey, nullptr);
    Require(written == pkcs8);
    auto* roundTrip = ReadPrivateKey(written, nullptr, nullptr, nullptr);
    Require(roundTrip != nullptr && roundTrip->type == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(roundTrip->key), static_cast<const LibreSsl3Rsa*>(reference->key),
        true);

    EVP_PKEY_free_nid_disambig1_nid_postfix(roundTrip);
    EVP_PKEY_free_nid_disambig1_nid_postfix(derKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(traditionalKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkcs8Key);
    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

void TestRsaPublicKey() {
    const std::vector<unsigned char> publicPem = BytesOf(RsaPublicPem);
    auto* privateKey = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    auto* spkiKey = KeyFromSpkiDer(BodyBytes(publicPem));

    auto* key = ReadPublicKey(publicPem, nullptr);
    Require(key != nullptr && key->type == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(key->key), static_cast<const LibreSsl3Rsa*>(spkiKey->key), false);
    ComparePublicRsa(static_cast<const LibreSsl3Rsa*>(key->key), static_cast<const LibreSsl3Rsa*>(privateKey->key));
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);

    auto* slot = privateKey;
    auto* replaced = ReadPublicKey(publicPem, &slot);
    Require(replaced != nullptr && slot == replaced && replaced != privateKey);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(replaced->key), static_cast<const LibreSsl3Rsa*>(spkiKey->key), false);
    EVP_PKEY_free_nid_disambig1_nid_postfix(replaced);
    EVP_PKEY_free_nid_disambig1_nid_postfix(spkiKey);
}

void TestEcPrivateFormats(int curve, const char* pkcs8Text, const char* traditionalText) {
    const std::vector<unsigned char> pkcs8 = BytesOf(pkcs8Text);
    const std::vector<unsigned char> traditional = BytesOf(traditionalText);
    auto* reference = KeyFromPkcs8Der(BodyBytes(pkcs8));
    Require(reference->type == EcType);

    auto* pkcs8Key = ReadPrivateKey(pkcs8, nullptr, nullptr, nullptr);
    Require(pkcs8Key != nullptr && pkcs8Key->type == EcType);
    CompareEc(static_cast<const EcKey*>(pkcs8Key->key), static_cast<const EcKey*>(reference->key), curve, true);

    auto* traditionalKey = ReadPrivateKey(traditional, nullptr, nullptr, nullptr);
    Require(traditionalKey != nullptr && traditionalKey->type == EcType);
    CompareEc(static_cast<const EcKey*>(traditionalKey->key), static_cast<const EcKey*>(reference->key), curve, true);

    const std::vector<unsigned char> written = WrittenPrivateBytes(traditionalKey, nullptr);
    Require(written == pkcs8);
    auto* roundTrip = ReadPrivateKey(written, nullptr, nullptr, nullptr);
    Require(roundTrip != nullptr && roundTrip->type == EcType);
    CompareEc(static_cast<const EcKey*>(roundTrip->key), static_cast<const EcKey*>(reference->key), curve, true);

    EVP_PKEY_free_nid_disambig1_nid_postfix(roundTrip);
    EVP_PKEY_free_nid_disambig1_nid_postfix(traditionalKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkcs8Key);
    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

void TestEcPublicKey(int curve, const char* publicText, const char* pkcs8Text) {
    auto* privateKey = KeyFromPkcs8Der(BodyBytes(BytesOf(pkcs8Text)));
    auto* key = ReadPublicKey(BytesOf(publicText), nullptr);
    Require(key != nullptr && key->type == EcType);
    CompareEc(static_cast<const EcKey*>(key->key), static_cast<const EcKey*>(privateKey->key), curve, false);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    EVP_PKEY_free_nid_disambig1_nid_postfix(privateKey);
}

void TestConsecutiveRecords() {
    std::vector<unsigned char> combined = BytesOf(RsaPublicPem);
    const std::vector<unsigned char> second = BytesOf(Ec256PublicPem);
    combined.insert(combined.end(), second.begin(), second.end());
    LibreSsl::Bio* bio = MemoryBio(combined);
    auto* first = PEM_read_bio_PUBKEY_nid_postfix(bio, nullptr, nullptr, nullptr);
    Require(first != nullptr && first->type == RsaType);
    auto* next = PEM_read_bio_PUBKEY_nid_postfix(bio, nullptr, nullptr, nullptr);
    Require(next != nullptr && next->type == EcType);
    auto* reference = KeyFromSpkiDer(BodyBytes(second));
    CompareEc(static_cast<const EcKey*>(next->key), static_cast<const EcKey*>(reference->key), Secp256r1, false);
    auto* third = PEM_read_bio_PUBKEY_nid_postfix(bio, nullptr, nullptr, nullptr);
    Require(third == nullptr);
    ExpectErrorCode(PemLibrary, PemNoStartLine);
    BIO_free_nid_postfix(bio);
    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(next);
    EVP_PKEY_free_nid_disambig1_nid_postfix(first);
}

void TestTextBeforeBegin() {
    std::vector<unsigned char> prefixed = BytesOf("read failure\nunexpected diagnostic text\n");
    const std::vector<unsigned char> record = BytesOf(Ec256Pkcs8Pem);
    prefixed.insert(prefixed.end(), record.begin(), record.end());
    auto* key = ReadPrivateKey(prefixed, nullptr, nullptr, nullptr);
    Require(key != nullptr && key->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);

    std::vector<unsigned char> skipped = BytesOf(RsaPublicKeyLabelPem);
    skipped.insert(skipped.end(), record.begin(), record.end());
    auto* afterMismatch = ReadPrivateKey(skipped, nullptr, nullptr, nullptr);
    Require(afterMismatch != nullptr && afterMismatch->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(afterMismatch);
}

void TestOutputReplacement() {
    const std::vector<unsigned char> firstRecord = BytesOf(Ec256Pkcs8Pem);
    const std::vector<unsigned char> secondRecord = BytesOf(Ec384Pkcs8Pem);
    auto* slot = KeyFromPkcs8Der(BodyBytes(firstRecord));
    auto* read = ReadPrivateKey(secondRecord, &slot, nullptr, nullptr);
    Require(read != nullptr && slot == read);
    CompareEc(static_cast<const EcKey*>(slot->key), static_cast<const EcKey*>(read->key), Secp384r1, true);

    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(GarbagePkcs8RecordPem), &slot, nullptr, nullptr) == nullptr);
    ExpectLastError(PemLibrary, ErrorReasonAsn1Library);
    Require(slot == read);
    CompareEc(static_cast<const EcKey*>(slot->key), static_cast<const EcKey*>(read->key), Secp384r1, true);

    auto* publicKeySlot = KeyFromPkcs8Der(BodyBytes(firstRecord));
    const LibreSsl3EvpPkey* previous = publicKeySlot;
    Require(ReadPublicKey(BytesOf(Ec256PublicPem), &publicKeySlot) != nullptr);
    Require(publicKeySlot != nullptr && publicKeySlot != previous);
    EVP_PKEY_free_nid_disambig1_nid_postfix(publicKeySlot);
    EVP_PKEY_free_nid_disambig1_nid_postfix(slot);
}

void TestPasswordCallbackNotCalled() {
    passwordCallbackCalls = 0;
    char userData = 0;
    auto* privateRead = ReadPrivateKey(BytesOf(RsaPkcs8Pem), nullptr, CountingPasswordCallback, &userData);
    Require(privateRead != nullptr);
    auto* publicRead = ReadPublicKey(BytesOf(RsaPublicPem), nullptr);
    Require(publicRead != nullptr);
    const std::vector<unsigned char> ecRecord = BytesOf(Ec256Pkcs8Pem);
    LibreSsl::Bio* bio = MemoryBio(ecRecord);
    auto* ecRead = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bio, nullptr, CountingPasswordCallback, &userData);
    Require(ecRead != nullptr);
    BIO_free_nid_postfix(bio);
    Require(passwordCallbackCalls == 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecRead);
    EVP_PKEY_free_nid_disambig1_nid_postfix(publicRead);
    EVP_PKEY_free_nid_disambig1_nid_postfix(privateRead);
}

using Bytes = std::vector<unsigned char>;
using ErrorList = std::vector<std::pair<int, int>>;

const Bytes Pbes2OidBytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0d};
const Bytes Pbkdf2OidBytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0c};
const Bytes HmacSha256OidBytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x09};
const Bytes HmacSha512_224OidBytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x0c};
const Bytes HmacSha3_256OidBytes{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0e};
const Bytes Aes128CbcOidBytes{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02};
const Bytes Aes192CbcOidBytes{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x16};
const Bytes Aes256CbcOidBytes{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a};
const Bytes UnknownOidBytes{0x2a, 0x03};
const Bytes DesCbcOidBytes{0x2b, 0x0e, 0x03, 0x02, 0x07};
const Bytes DesEde3CbcOidBytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x03, 0x07};
const Bytes DesEdeEcbOidBytes{0x2b, 0x0e, 0x03, 0x02, 0x11};

struct PasswordRequest {
    const char* text = nullptr;
    int result = 0;
    int calls = 0;
    int lastSize = 0;
    int lastWriteFlag = -1;
};

int APS5_VABI SupplyPasswordCallback(char* buffer, int size, int writeFlag, void* userData) {
    auto* request = static_cast<PasswordRequest*>(userData);
    ++request->calls;
    request->lastSize = size;
    request->lastWriteFlag = writeFlag;
    if (request->text == nullptr) return request->result;
    const std::size_t length = std::strlen(request->text);
    Require(size >= 0 && length <= static_cast<std::size_t>(size));
    std::memcpy(buffer, request->text, length);
    return static_cast<int>(length);
}

template<typename Call>
void ExpectUnimplemented(Call call, std::source_location location = std::source_location::current()) {
    bool thrown = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown, location);
}

template<typename Call>
void ExpectInvalidArgument(Call call, std::source_location location = std::source_location::current()) {
    bool thrown = false;
    try {
        call();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown, location);
}

ErrorList DrainErrors() {
    ErrorList errors;
    for (std::uint64_t code = ERR_get_error_nid_postfix(); code != 0; code = ERR_get_error_nid_postfix())
        errors.push_back({static_cast<int>((code >> 24) & 0xff), static_cast<int>(code & 0xfff)});
    return errors;
}

bool Contains(const ErrorList& errors, const std::pair<int, int>& wanted) {
    for (const std::pair<int, int>& error : errors) {
        if (error == wanted) return true;
    }
    return false;
}

ErrorList CipherInitFailure(ErrorList errors) {
    errors.push_back({Pkcs12Errors, Pkcs12AlgorCipherInitError});
    errors.push_back({Pkcs12Errors, Pkcs12PbeCryptError});
    errors.push_back({PemLibrary, ErrorReasonAsn1Library});
    return errors;
}

ErrorList KeygenFailure(ErrorList errors) {
    errors.push_back({EvpErrors, EvpKeygenFailure});
    return CipherInitFailure(std::move(errors));
}

ErrorList StructureFailure(ErrorList errors) {
    errors.push_back({PemLibrary, ErrorReasonAsn1Library});
    return errors;
}

LibreSsl3EvpPkey* ReadWithPassword(const Bytes& pem, const char* password,
    std::source_location location = std::source_location::current()) {
    PasswordRequest request{password};
    ERR_clear_error_nid_postfix();
    auto* key = ReadPrivateKey(pem, nullptr, SupplyPasswordCallback, &request);
    Require(request.calls == 1 && request.lastSize == PemPasswordBufferSize && request.lastWriteFlag == 0, location);
    return key;
}

void ExpectFailedRead(const Bytes& pem, const char* password, int expectedCalls, const ErrorList& errors,
    std::source_location location = std::source_location::current()) {
    PasswordRequest request{password};
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(pem, nullptr, SupplyPasswordCallback, &request) == nullptr, location);
    Require(request.calls == expectedCalls, location);
    ExpectErrorSequence(errors, location);
}

void ExpectEcReference(LibreSsl3EvpPkey* key, const LibreSsl3EvpPkey* reference,
    std::source_location location = std::source_location::current()) {
    Require(key != nullptr && key->type == EcType, location);
    CompareEc(static_cast<const EcKey*>(key->key), static_cast<const EcKey*>(reference->key), Secp256r1, true,
        location);
    ExpectEmptyQueue(location);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void ExpectRsaReference(LibreSsl3EvpPkey* key, const LibreSsl3EvpPkey* reference,
    std::source_location location = std::source_location::current()) {
    Require(key != nullptr && key->type == RsaType, location);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(key->key), static_cast<const LibreSsl3Rsa*>(reference->key), true,
        location);
    ExpectEmptyQueue(location);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

struct TraditionalRecord {
    std::string label;
    std::string dekInfo;
    Bytes data;
};

TraditionalRecord SplitEncryptedRecord(const char* text) {
    const std::string record(text);
    const std::size_t labelEnd = record.find("-----\n");
    const std::size_t dek = record.find("DEK-Info: ");
    Require(labelEnd != std::string::npos && labelEnd > 11 && dek != std::string::npos);
    const std::size_t dekEnd = record.find('\n', dek);
    const std::size_t blank = record.find("\n\n");
    Require(dekEnd != std::string::npos && blank != std::string::npos);
    TraditionalRecord parts;
    parts.label = record.substr(11, labelEnd - 11);
    parts.dekInfo = record.substr(dek + 10, dekEnd - dek - 10);
    const std::string clear = "-----BEGIN X-----\n" + record.substr(blank + 2);
    parts.data = BodyBytes(Bytes(clear.begin(), clear.end()));
    return parts;
}

Bytes EncryptedRecord(const std::string& label, const std::string& dekInfo, const Bytes& data) {
    const Bytes plain = EncodePemRecord(label, data);
    std::string text(plain.begin(), plain.end());
    const std::size_t bodyStart = text.find('\n') + 1;
    text.insert(bodyStart, "Proc-Type: 4,ENCRYPTED\nDEK-Info: " + dekInfo + "\n\n");
    return Bytes(text.begin(), text.end());
}

Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& part : parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}

Bytes Der(unsigned char tag, const Bytes& content) {
    Bytes out{tag};
    const std::size_t size = content.size();
    Require(size <= 0xffff);
    if (size < 0x80) {
        out.push_back(static_cast<unsigned char>(size));
    } else if (size <= 0xff) {
        out.push_back(0x81);
        out.push_back(static_cast<unsigned char>(size));
    } else {
        out.push_back(0x82);
        out.push_back(static_cast<unsigned char>(size >> 8));
        out.push_back(static_cast<unsigned char>(size & 0xff));
    }
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

struct Tlv {
    unsigned char tag = 0;
    Bytes content;
};

std::vector<Tlv> DerChildren(const Bytes& content) {
    std::vector<Tlv> items;
    std::size_t offset = 0;
    while (offset < content.size()) {
        Require(content.size() - offset >= 2);
        Tlv item;
        item.tag = content[offset];
        std::size_t length = content[offset + 1];
        std::size_t header = 2;
        if ((length & 0x80) != 0) {
            const std::size_t count = length & 0x7f;
            Require(count >= 1 && count <= 2 && content.size() - offset >= 2 + count);
            length = 0;
            for (std::size_t index = 0; index < count; ++index) length = (length << 8) | content[offset + 2 + index];
            header += count;
        }
        Require(content.size() - offset - header >= length);
        const auto first = content.begin() + static_cast<std::ptrdiff_t>(offset + header);
        item.content.assign(first, first + static_cast<std::ptrdiff_t>(length));
        items.push_back(item);
        offset += header + length;
    }
    return items;
}

struct Pbes2Parts {
    Bytes salt;
    Bytes iterations;
    Bytes prfOid;
    Bytes cipherOid;
    Bytes iv;
    Bytes data;
};

Pbes2Parts SplitPbes2(const Bytes& der) {
    const std::vector<Tlv> top = DerChildren(der);
    Require(top.size() == 1 && top[0].tag == 0x30);
    const std::vector<Tlv> key = DerChildren(top[0].content);
    Require(key.size() == 2 && key[0].tag == 0x30 && key[1].tag == 0x04);
    const std::vector<Tlv> algorithm = DerChildren(key[0].content);
    Require(algorithm.size() == 2 && algorithm[0].tag == 0x06 && algorithm[0].content == Pbes2OidBytes);
    const std::vector<Tlv> scheme = DerChildren(algorithm[1].content);
    Require(algorithm[1].tag == 0x30 && scheme.size() == 2 && scheme[0].tag == 0x30 && scheme[1].tag == 0x30);
    const std::vector<Tlv> keyFunction = DerChildren(scheme[0].content);
    Require(keyFunction.size() == 2 && keyFunction[0].content == Pbkdf2OidBytes && keyFunction[1].tag == 0x30);
    const std::vector<Tlv> kdf = DerChildren(keyFunction[1].content);
    Require((kdf.size() == 2 || kdf.size() == 3) && kdf[0].tag == 0x04 && kdf[1].tag == 0x02);
    const std::vector<Tlv> encryption = DerChildren(scheme[1].content);
    Require(encryption.size() == 2 && encryption[0].tag == 0x06 && encryption[1].tag == 0x04);
    Pbes2Parts parts;
    parts.salt = kdf[0].content;
    parts.iterations = kdf[1].content;
    if (kdf.size() == 3) {
        const std::vector<Tlv> prf = DerChildren(kdf[2].content);
        Require(kdf[2].tag == 0x30 && prf.size() == 2 && prf[0].tag == 0x06 && prf[1].tag == 0x05);
        parts.prfOid = prf[0].content;
    }
    parts.cipherOid = encryption[0].content;
    parts.iv = encryption[1].content;
    parts.data = key[1].content;
    return parts;
}

Bytes PrfAlgorithm(const Bytes& oid) {
    return Der(0x30, Join({Der(0x06, oid), Der(0x05, {})}));
}

Bytes KdfParameters(const Pbes2Parts& parts) {
    Bytes body = Join({Der(0x04, parts.salt), Der(0x02, parts.iterations)});
    if (!parts.prfOid.empty()) body = Join({body, PrfAlgorithm(parts.prfOid)});
    return Der(0x30, body);
}

Bytes KeyFunction(const Bytes& kdfParameters) {
    return Der(0x30, Join({Der(0x06, Pbkdf2OidBytes), kdfParameters}));
}

Bytes Encryption(const Bytes& cipherOid, const Bytes& parameter) {
    return Der(0x30, Join({Der(0x06, cipherOid), parameter}));
}

Bytes Pbes2Algorithm(const Bytes& keyFunction, const Bytes& encryption) {
    return Der(0x30, Join({Der(0x06, Pbes2OidBytes), Der(0x30, Join({keyFunction, encryption}))}));
}

Bytes EncryptedKey(const Bytes& algorithm, const Bytes& digest) {
    return Der(0x30, Join({algorithm, digest}));
}

Bytes EncryptedKeyPem(const Bytes& der) {
    return EncodePemRecord("ENCRYPTED PRIVATE KEY", der);
}

Bytes Rebuild(const Pbes2Parts& parts) {
    return EncryptedKey(
        Pbes2Algorithm(KeyFunction(KdfParameters(parts)), Encryption(parts.cipherOid, Der(0x04, parts.iv))),
        Der(0x04, parts.data));
}

Bytes MemoryBytes(LibreSsl::Bio* bio) {
    char* pointer = nullptr;
    const std::int64_t length = BIO_ctrl_nid_postfix(bio, BioCommandMemPointer, 0, &pointer);
    if (length <= 0 || pointer == nullptr) return {};
    return Bytes(pointer, pointer + length);
}

void TestEncryptedTraditionalRead() {
    auto* ecReference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* rsaReference = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    for (const char* record : {EcTraditionalAes128Pem, EcTraditionalAes192Pem, EcTraditionalAes256Pem})
        ExpectEcReference(ReadWithPassword(BytesOf(record), TestPassword), ecReference);
    ExpectRsaReference(ReadWithPassword(BytesOf(RsaTraditionalAes256Pem), TestPassword), rsaReference);

    char phrase[] = "anyps5-pem-test";
    ERR_clear_error_nid_postfix();
    ExpectEcReference(ReadPrivateKey(BytesOf(EcTraditionalAes256Pem), nullptr, nullptr, phrase), ecReference);
    ExpectUnimplemented([] { ReadPrivateKey(BytesOf(EcTraditionalAes256Pem), nullptr, nullptr, nullptr); });

    const TraditionalRecord parts = SplitEncryptedRecord(EcTraditionalAes128Pem);
    Require(parts.label == "EC PRIVATE KEY" && parts.dekInfo.size() == 44);
    Require(parts.dekInfo.rfind("AES-128-CBC,", 0) == 0);
    const std::string iv = parts.dekInfo.substr(12);
    std::string lowered = iv;
    for (char& character : lowered) {
        if (character >= 'A' && character <= 'F') character = static_cast<char>(character - 'A' + 'a');
    }
    Require(lowered != iv);
    for (const std::string& dekInfo : {"AES-128-CBC," + iv, "AES-128-CBC," + lowered, "AES128," + iv,
             "AES-128-CBC;" + iv, "AES-128-CBC," + iv + "ZZ"})
        ExpectEcReference(ReadWithPassword(EncryptedRecord(parts.label, dekInfo, parts.data), TestPassword),
            ecReference);

    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReference);
}

void TestEncryptedTraditionalErrors() {
    const Bytes record = BytesOf(EcTraditionalAes128Pem);
    ExpectFailedRead(record, "wrong-password", 1, {{EvpErrors, EvpBadDecrypt}, {PemLibrary, PemBadDecrypt}});

    {
        PasswordRequest request{"wrong-207"};
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request) == nullptr);
        const ErrorList errors = DrainErrors();
        Require(!errors.empty() && errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
        Require(!Contains(errors, {PemLibrary, PemBadDecrypt}));
    }

    for (const int result : {0, -1}) {
        PasswordRequest request;
        request.result = result;
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request) == nullptr);
        Require(request.calls == 1);
        ExpectErrorSequence({{PemLibrary, PemBadPasswordRead}});
    }
    {
        PasswordRequest request;
        request.result = PemPasswordBufferSize + 1;
        ExpectInvalidArgument([&] { ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request); });
    }
    char emptyPhrase[] = "";
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(record, nullptr, nullptr, emptyPhrase) == nullptr);
    ExpectErrorSequence({{PemLibrary, PemBadPasswordRead}});
    ExpectFailedRead(BytesOf(EncryptedHeaderPem), nullptr, 1, {{PemLibrary, PemBadPasswordRead}});

    const TraditionalRecord parts = SplitEncryptedRecord(EcTraditionalAes128Pem);
    const std::string iv = parts.dekInfo.substr(12);
    for (const std::string& dekInfo : {"AES-128-CBX," + iv, "aes-128-cbc," + iv, "," + iv})
        ExpectFailedRead(EncryptedRecord(parts.label, dekInfo, parts.data), TestPassword, 0,
            {{PemLibrary, PemUnsupportedEncryption}});
    for (const std::string& dekInfo : {"AES-128-CBC," + iv.substr(0, 30), "AES-128-CBC,G" + iv.substr(1),
             std::string("AES-128-CBC"), "AES-128-CBC, " + iv})
        ExpectFailedRead(EncryptedRecord(parts.label, dekInfo, parts.data), TestPassword, 0,
            {{PemLibrary, PemBadIvChars}});

    {
        PasswordRequest request{TestPassword};
        ExpectUnimplemented(
            [&] { ReadPrivateKey(BytesOf(EcTraditionalCamelliaPem), nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 0);
    }

    Bytes shortened = parts.data;
    Require(shortened.size() > 16 && shortened.size() % 16 == 0);
    shortened.resize(shortened.size() - 5);
    ExpectFailedRead(EncryptedRecord(parts.label, parts.dekInfo, shortened), TestPassword, 1,
        {{EvpErrors, EvpWrongFinalBlockLength}, {PemLibrary, PemBadDecrypt}});
    shortened.resize(5);
    ExpectFailedRead(EncryptedRecord(parts.label, parts.dekInfo, shortened), TestPassword, 1,
        {{EvpErrors, EvpWrongFinalBlockLength}, {PemLibrary, PemBadDecrypt}});

    const Bytes publicRecord = EncryptedRecord("PUBLIC KEY", parts.dekInfo, parts.data);
    {
        PasswordRequest request{"wrong-password"};
        LibreSsl::Bio* bio = MemoryBio(publicRecord);
        ERR_clear_error_nid_postfix();
        Require(PEM_read_bio_PUBKEY_nid_postfix(bio, nullptr, SupplyPasswordCallback, &request) == nullptr);
        BIO_free_nid_postfix(bio);
        Require(request.calls == 1 && request.lastWriteFlag == 0);
        ExpectErrorSequence({{EvpErrors, EvpBadDecrypt}, {PemLibrary, PemBadDecrypt}});
    }
}

void TestEncryptedPkcs8Read() {
    auto* ecReference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* rsaReference = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    for (const char* record : {EcPkcs8Aes256Sha256Pem, EcPkcs8Aes128Sha1Pem, EcPkcs8Aes192Sha512Pem,
             EcPkcs8Aes256Sha384Pem, EcPkcs8Aes128Md5Pem, EcPkcs8Aes128Sha224Pem, EcPkcs8Aes128Sha512_256Pem})
        ExpectEcReference(ReadWithPassword(BytesOf(record), TestPassword), ecReference);
    Require(SplitPbes2(BodyBytes(BytesOf(EcPkcs8Aes128Sha1Pem))).prfOid.empty());
    ExpectRsaReference(ReadWithPassword(BytesOf(RsaPkcs8Aes256Sha256Pem), TestPassword), rsaReference);

    const Bytes record = BytesOf(EcPkcs8Aes256Sha256Pem);
    char phrase[] = "anyps5-pem-test";
    ERR_clear_error_nid_postfix();
    ExpectEcReference(ReadPrivateKey(record, nullptr, nullptr, phrase), ecReference);
    ExpectUnimplemented([&] { ReadPrivateKey(record, nullptr, nullptr, nullptr); });

    const ErrorList badDecrypt{{EvpErrors, EvpBadDecrypt}, {Pkcs12Errors, Pkcs12CipherFinalError},
        {Pkcs12Errors, Pkcs12PbeCryptError}, {PemLibrary, ErrorReasonAsn1Library}};
    {
        auto* slot = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec384Pkcs8Pem)));
        auto* const before = slot;
        PasswordRequest wrong{"wrong-password"};
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(record, &slot, SupplyPasswordCallback, &wrong) == nullptr);
        Require(wrong.calls == 1 && slot == before);
        ExpectErrorSequence(badDecrypt);
        PasswordRequest refused;
        Require(ReadPrivateKey(record, &slot, SupplyPasswordCallback, &refused) == nullptr);
        Require(refused.calls == 1 && slot == before);
        ExpectErrorSequence({{PemLibrary, PemBadPasswordRead}});
        PasswordRequest right{TestPassword};
        auto* replaced = ReadPrivateKey(record, &slot, SupplyPasswordCallback, &right);
        Require(replaced != nullptr && slot == replaced);
        ExpectEcReference(replaced, ecReference);
    }

    {
        PasswordRequest request{"wrong-104"};
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(BytesOf(EcPkcs8Aes192Sha512Pem), nullptr, SupplyPasswordCallback, &request) == nullptr);
        const ErrorList errors = DrainErrors();
        Require(errors.size() >= 2);
        Require(errors[errors.size() - 2] == std::pair<int, int>{Pkcs12Errors, Pkcs12DecodeError});
        Require(errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
        Require(!Contains(errors, {Pkcs12Errors, Pkcs12CipherFinalError}));
    }

    {
        PasswordRequest request;
        request.result = -1;
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request) == nullptr);
        Require(request.calls == 1 && request.lastSize == PemPasswordBufferSize && request.lastWriteFlag == 0);
        ExpectErrorSequence({{PemLibrary, PemBadPasswordRead}});
        request.result = PemPasswordBufferSize + 1;
        ExpectInvalidArgument([&] { ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request); });
    }

    for (const char* unavailable : {EcPkcs8CamelliaPem, EcPkcs8Sha1Rc4Pem}) {
        PasswordRequest request{TestPassword};
        ExpectUnimplemented(
            [&] { ReadPrivateKey(BytesOf(unavailable), nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 1);
    }

    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReference);
}

void TestEncryptedPkcs8Structure() {
    const Bytes der = BodyBytes(BytesOf(EcPkcs8Aes256Sha256Pem));
    const Pbes2Parts parts = SplitPbes2(der);
    Require(Rebuild(parts) == der);
    Require(parts.cipherOid == Aes256CbcOidBytes && parts.prfOid == HmacSha256OidBytes && parts.iv.size() == 16);
    const Bytes scheme =
        Der(0x30, Join({KeyFunction(KdfParameters(parts)), Encryption(parts.cipherOid, Der(0x04, parts.iv))}));
    const Bytes algorithm = Der(0x30, Join({Der(0x06, Pbes2OidBytes), scheme}));
    const Bytes digest = Der(0x04, parts.data);
    const Bytes body = Join({algorithm, digest});
    auto* reference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    const auto rejected = [](const Bytes& encoded, ErrorList errors,
                              std::source_location location = std::source_location::current()) {
        ExpectFailedRead(EncryptedKeyPem(encoded), TestPassword, 0, StructureFailure(std::move(errors)), location);
    };

    Bytes wrongOuter = der;
    wrongOuter[0] = 0x31;
    rejected(wrongOuter, {{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error}});
    Bytes primitiveOuter = der;
    primitiveOuter[0] = 0x10;
    rejected(primitiveOuter, {{Asn1Errors, Asn1SequenceNotConstructed}});
    Bytes truncated = der;
    truncated.pop_back();
    rejected(truncated, {{Asn1Errors, Asn1TooLong}, {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(Der(0x30, {}), {{Asn1Errors, Asn1FieldMissing}});
    rejected(Der(0x30, algorithm), {{Asn1Errors, Asn1FieldMissing}});
    rejected(Der(0x30, Join({body, Der(0x05, {})})), {{Asn1Errors, Asn1SequenceLengthMismatch}});
    rejected(EncryptedKey(algorithm, Der(0x02, parts.data)),
        {{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error}, {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(EncryptedKey(Der(0x30, {}), digest), {{Asn1Errors, Asn1FieldMissing}, {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(EncryptedKey(Der(0x30, Join({Der(0x06, {0x80, 0x01}), scheme})), digest),
        {{Asn1Errors, Asn1InvalidObjectEncoding}, {Asn1Errors, Asn1NestedAsn1Error},
            {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(EncryptedKey(Der(0x30, Join({Der(0x06, Pbes2OidBytes), Bytes{0x00, 0x00}})), digest),
        {{Asn1Errors, Asn1UnexpectedEoc}, {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(EncryptedKey(Der(0x30, Join({Der(0x06, Pbes2OidBytes), Der(0x05, {0x00})})), digest),
        {{Asn1Errors, Asn1NullIsWrongLength}, {Asn1Errors, Asn1NestedAsn1Error},
            {Asn1Errors, Asn1NestedAsn1Error}});

    const auto withParameter = [&](const Bytes& parameter) {
        return EncryptedKey(Der(0x30, Join({Der(0x06, Pbes2OidBytes), parameter})), digest);
    };
    const std::pair<Bytes, int> refusedParameters[] = {
        {Der(0x01, {0x00, 0x00}), Asn1BooleanIsWrongLength},
        {Der(0x01, {}), Asn1BooleanIsWrongLength},
        {Der(0x03, {}), Asn1StringTooShort},
        {Der(0x03, {0x08, 0x00}), Asn1InvalidBitStringBitsLeft},
        {Der(0x02, {}), Asn1LengthMismatch},
        {Der(0x0a, {0xff, 0x80}), Asn1LengthMismatch},
        {Der(0x1e, {0x00, 0x41, 0x00}), Asn1BmpStringIsWrongLength},
        {Der(0x1c, {0x00, 0x00, 0x00}), Asn1UniversalStringIsWrongLength},
        {Der(0x22, {}), Asn1TypeNotPrimitive},
        {Der(0x25, {}), Asn1TypeNotPrimitive},
        {Der(0x10, {}), Asn1TypeNotConstructed},
        {Der(0x11, {}), Asn1TypeNotConstructed},
    };
    for (const auto& [parameter, reason] : refusedParameters)
        rejected(withParameter(parameter),
            {{Asn1Errors, reason}, {Asn1Errors, Asn1NestedAsn1Error}, {Asn1Errors, Asn1NestedAsn1Error}});
    rejected(EncryptedKey(Der(0x30, Join({Der(0x26, Pbes2OidBytes), scheme})), digest),
        {{Asn1Errors, Asn1TypeNotPrimitive}, {Asn1Errors, Asn1NestedAsn1Error}, {Asn1Errors, Asn1NestedAsn1Error}});
    for (const Bytes& parameter : {Der(0x01, {0x7f}), Der(0x03, {0x07, 0xff}), Der(0x02, {0xff, 0x00}),
             Der(0x1e, {0x00, 0x41}), Der(0x1c, {0x00, 0x00, 0x00, 0x41}), Der(0x0c, {0x41}), Der(0x80, {0x01}),
             Der(0xa0, {0x05, 0x00})})
        ExpectFailedRead(EncryptedKeyPem(withParameter(parameter)), TestPassword, 1,
            KeygenFailure({{EvpErrors, EvpDecodeError}}));

    Bytes longLength{0x30, 0x84, 0x00, 0x00, static_cast<unsigned char>(body.size() >> 8),
        static_cast<unsigned char>(body.size() & 0xff)};
    longLength.insert(longLength.end(), body.begin(), body.end());
    ExpectEcReference(ReadWithPassword(EncryptedKeyPem(longLength), TestPassword), reference);
    Bytes longTag = der;
    longTag[0] = 0x3f;
    longTag.insert(longTag.begin() + 1, 0x10);
    ExpectEcReference(ReadWithPassword(EncryptedKeyPem(longTag), TestPassword), reference);
    Bytes trailing = der;
    trailing.push_back(0x00);
    ExpectEcReference(ReadWithPassword(EncryptedKeyPem(trailing), TestPassword), reference);

    Bytes indefinite{0x30, 0x80};
    indefinite.insert(indefinite.end(), body.begin(), body.end());
    indefinite.push_back(0x00);
    indefinite.push_back(0x00);
    for (const Bytes& unmodelled : {indefinite, EncryptedKey(algorithm, Der(0x24, digest)),
             EncryptedKey(Der(0x30, Join({Der(0x06, Pbes2OidBytes), Der(0x17, {0x30})})), digest)}) {
        PasswordRequest request{TestPassword};
        ExpectUnimplemented(
            [&] { ReadPrivateKey(EncryptedKeyPem(unmodelled), nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 0);
    }

    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

void TestEncryptedPkcs8Parameters() {
    const Bytes der = BodyBytes(BytesOf(EcPkcs8Aes256Sha256Pem));
    const Pbes2Parts parts = SplitPbes2(der);
    const Bytes salt = Der(0x04, parts.salt);
    const Bytes iterations = Der(0x02, parts.iterations);
    const Bytes prf = PrfAlgorithm(parts.prfOid);
    const Bytes kdf = KdfParameters(parts);
    const Bytes keyFunction = KeyFunction(kdf);
    const Bytes encryption = Encryption(parts.cipherOid, Der(0x04, parts.iv));
    const Bytes digest = Der(0x04, parts.data);
    auto* reference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    const auto record = [&](const Bytes& algorithm) { return EncryptedKeyPem(EncryptedKey(algorithm, digest)); };
    const auto withKdf = [&](const Bytes& kdfBody) {
        return record(Pbes2Algorithm(KeyFunction(Der(0x30, kdfBody)), encryption));
    };
    const auto withEncryption = [&](const Bytes& alternative) {
        return record(Pbes2Algorithm(keyFunction, alternative));
    };

    ExpectFailedRead(record(Der(0x30, Join({Der(0x06, UnknownOidBytes), Der(0x05, {})}))), TestPassword, 1,
        CipherInitFailure({{EvpErrors, EvpUnknownPbeAlgorithm}}));
    ExpectFailedRead(record(Der(0x30, Join({Der(0x06, Pbkdf2OidBytes), kdf}))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpNoCipherSet}}));
    ExpectFailedRead(record(Der(0x30, Join({Der(0x06, Pbkdf2OidBytes), Der(0x05, {})}))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpNoCipherSet}}));
    ExpectFailedRead(record(Der(0x30, Der(0x06, Pbes2OidBytes))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(record(Der(0x30, Join({Der(0x06, Pbes2OidBytes), Der(0x05, {})}))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(record(Der(0x30, Join({Der(0x06, Pbes2OidBytes), Der(0x30, keyFunction)}))), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1FieldMissing}, {EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(record(Pbes2Algorithm(Der(0x30, Join({Der(0x06, Pbes2OidBytes), kdf})), encryption)), TestPassword,
        1, KeygenFailure({{EvpErrors, EvpUnsupportedKeyDerivationFunction}}));
    ExpectFailedRead(record(Pbes2Algorithm(Der(0x30, Der(0x06, Pbkdf2OidBytes)), encryption)), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpDecodeError}}));

    ExpectFailedRead(withEncryption(Encryption(UnknownOidBytes, Der(0x04, parts.iv))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedCipher}}));
    ExpectFailedRead(withEncryption(Encryption(parts.cipherOid, Der(0x05, {}))), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1DataIsWrong}, {EvpErrors, EvpCipherParameterError}}));
    ExpectFailedRead(withEncryption(Encryption(parts.cipherOid, Der(0x04, Bytes(8, 0)))), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpCipherParameterError}}));
    {
        PasswordRequest request{TestPassword};
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(withEncryption(Der(0x30, Der(0x06, parts.cipherOid))), nullptr, SupplyPasswordCallback,
                    &request)
            == nullptr);
        const ErrorList errors = DrainErrors();
        Require(errors.size() >= 2);
        Require(errors[errors.size() - 2] == std::pair<int, int>{Pkcs12Errors, Pkcs12DecodeError});
        Require(errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
    }

    ExpectFailedRead(withKdf(Join({Der(0x02, {0x01}), iterations, prf})), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedSaltType}}));
    ExpectFailedRead(withKdf(Join({salt, Der(0x02, {0x00}), prf})), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedNumberOfRounds}}));
    ExpectFailedRead(withKdf(Join({salt, Der(0x02, {0xff}), prf})), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedNumberOfRounds}}));
    ExpectFailedRead(withKdf(Join({salt, Der(0x02, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}), prf})),
        TestPassword, 1, KeygenFailure({{Asn1Errors, Asn1TooLarge}, {EvpErrors, EvpUnsupportedNumberOfRounds}}));
    ExpectFailedRead(withKdf(Join({salt, Der(0x02, {0x00, 0x01}), prf})), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error},
            {EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(withKdf(Join({salt, iterations, Der(0x02, {0x10}), prf})), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedKeylength}}));
    ExpectFailedRead(withKdf(Join({salt, iterations, PrfAlgorithm(UnknownOidBytes)})), TestPassword, 1,
        KeygenFailure({{EvpErrors, EvpUnsupportedPrf}}));
    ExpectFailedRead(withKdf(Join({Bytes{0x00, 0x00}, iterations, prf})), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1UnexpectedEoc}, {EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(withKdf(Join({salt, iterations, prf, Der(0x05, {})})), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1SequenceLengthMismatch}, {EvpErrors, EvpDecodeError}}));
    ExpectFailedRead(withKdf(salt), TestPassword, 1,
        KeygenFailure({{Asn1Errors, Asn1FieldMissing}, {EvpErrors, EvpDecodeError}}));

    ExpectEcReference(ReadWithPassword(withKdf(Join({salt, iterations, Der(0x02, {0x20}), prf})), TestPassword),
        reference);
    ExpectEcReference(ReadWithPassword(withKdf(Join({salt, iterations, Der(0x30, Der(0x06, parts.prfOid))})),
                          TestPassword),
        reference);

    for (const Bytes& missingDigest : {HmacSha512_224OidBytes, HmacSha3_256OidBytes}) {
        PasswordRequest request{TestPassword};
        ExpectUnimplemented([&] {
            ReadPrivateKey(withKdf(Join({salt, iterations, PrfAlgorithm(missingDigest)})), nullptr,
                SupplyPasswordCallback, &request);
        });
        Require(request.calls == 1);
    }

    const ErrorList wrongFinalBlock{{EvpErrors, EvpWrongFinalBlockLength}, {Pkcs12Errors, Pkcs12CipherFinalError},
        {Pkcs12Errors, Pkcs12PbeCryptError}, {PemLibrary, ErrorReasonAsn1Library}};
    const Bytes algorithm = Pbes2Algorithm(keyFunction, encryption);
    Bytes shortened = parts.data;
    Require(shortened.size() > 16 && shortened.size() % 16 == 0);
    shortened.pop_back();
    ExpectFailedRead(EncryptedKeyPem(EncryptedKey(algorithm, Der(0x04, shortened))), TestPassword, 1, wrongFinalBlock);
    ExpectFailedRead(EncryptedKeyPem(EncryptedKey(algorithm, Der(0x04, {}))), TestPassword, 1, wrongFinalBlock);

    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

int WriteEncrypted(LibreSsl3EvpPkey* key, const LibreSsl3EvpCipher* cipher, unsigned char* keyString, int keyLength,
    LibreSslPemPasswordCallback callback, void* userData, Bytes& written) {
    auto* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr);
    const int result = PEM_write_bio_PrivateKey_nid_postfix(bio, key, cipher, keyString, keyLength, callback, userData);
    written = MemoryBytes(bio);
    BIO_free_nid_postfix(bio);
    return result;
}

const LibreSsl3EvpCipher* CipherObject(const void* cipher) {
    Require(cipher != nullptr);
    return static_cast<const LibreSsl3EvpCipher*>(cipher);
}

void TestEncryptedWrite() {
    const Bytes plainPem = BytesOf(Ec256Pkcs8Pem);
    const std::size_t plainLength = BodyBytes(plainPem).size();
    auto* ecKey = KeyFromPkcs8Der(BodyBytes(plainPem));
    auto* rsaKey = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    const auto* const aes128 = CipherObject(EVP_aes_128_cbc_nid_postfix());
    const auto* const aes192 = CipherObject(EVP_aes_192_cbc_nid_postfix());
    const auto* const aes256 = CipherObject(EVP_aes_256_cbc_nid_postfix());
    unsigned char phrase[] = "anyps5-pem-test";
    const int phraseLength = static_cast<int>(sizeof(phrase) - 1);
    const std::string beginLine = "-----BEGIN ENCRYPTED PRIVATE KEY-----\n";
    const std::string endLine = "-----END ENCRYPTED PRIVATE KEY-----\n";
    Bytes written;

    const std::pair<const LibreSsl3EvpCipher*, const Bytes*> ciphers[] = {
        {aes128, &Aes128CbcOidBytes}, {aes192, &Aes192CbcOidBytes}, {aes256, &Aes256CbcOidBytes}};
    for (const auto& [cipher, oid] : ciphers) {
        ERR_clear_error_nid_postfix();
        Require(WriteEncrypted(ecKey, cipher, phrase, phraseLength, nullptr, nullptr, written) == 1);
        ExpectEmptyQueue();
        const std::string text(written.begin(), written.end());
        Require(text.size() > beginLine.size() + endLine.size());
        Require(text.compare(0, beginLine.size(), beginLine) == 0);
        Require(text.compare(text.size() - endLine.size(), endLine.size(), endLine) == 0);
        const Bytes der = BodyBytes(written);
        const Pbes2Parts parts = SplitPbes2(der);
        Require(Rebuild(parts) == der);
        Require(parts.cipherOid == *oid);
        Require(parts.iv.size() == 16 && parts.salt.size() == 8);
        Require(parts.iterations == Bytes({0x08, 0x00}));
        Require(parts.prfOid.empty());
        Require(parts.data.size() == (plainLength / 16 + 1) * 16);
        ExpectEcReference(ReadWithPassword(written, TestPassword), ecKey);

        Bytes again;
        Require(WriteEncrypted(ecKey, cipher, phrase, phraseLength, nullptr, nullptr, again) == 1);
        const Pbes2Parts repeated = SplitPbes2(BodyBytes(again));
        Require(repeated.salt != parts.salt && repeated.iv != parts.iv);
    }

    Require(WriteEncrypted(rsaKey, aes256, phrase, phraseLength, nullptr, nullptr, written) == 1);
    ExpectRsaReference(ReadWithPassword(written, TestPassword), rsaKey);

    Require(WriteEncrypted(ecKey, aes256, phrase, -1, nullptr, nullptr, written) == 1);
    ExpectEcReference(ReadWithPassword(written, TestPassword), ecKey);
    Require(WriteEncrypted(ecKey, aes256, phrase, 6, nullptr, nullptr, written) == 1);
    ExpectEcReference(ReadWithPassword(written, "anyps5"), ecKey);

    PasswordRequest unused{"unused-phrase"};
    Require(WriteEncrypted(ecKey, aes256, phrase, phraseLength, SupplyPasswordCallback, &unused, written) == 1);
    Require(unused.calls == 0);
    ExpectEcReference(ReadWithPassword(written, TestPassword), ecKey);

    PasswordRequest request{TestPassword};
    Require(WriteEncrypted(ecKey, aes256, nullptr, 0, SupplyPasswordCallback, &request, written) == 1);
    Require(request.calls == 1 && request.lastSize == PemPasswordBufferSize && request.lastWriteFlag == 1);
    ExpectEcReference(ReadWithPassword(written, TestPassword), ecKey);

    char defaultPhrase[] = "anyps5-pem-test";
    Require(WriteEncrypted(ecKey, aes256, nullptr, 0, nullptr, defaultPhrase, written) == 1);
    ExpectEcReference(ReadWithPassword(written, TestPassword), ecKey);
    ExpectUnimplemented([&] { WriteEncrypted(ecKey, aes256, nullptr, 0, nullptr, nullptr, written); });

    std::string longPhrase(static_cast<std::size_t>(PemPasswordBufferSize) + 6, 'a');
    const std::string cutPhrase(static_cast<std::size_t>(PemPasswordBufferSize), 'a');
    const std::string shortPhrase(static_cast<std::size_t>(PemPasswordBufferSize) - 1, 'a');
    ERR_clear_error_nid_postfix();
    Require(WriteEncrypted(ecKey, aes128, nullptr, 0, nullptr, longPhrase.data(), written) == 1);
    ExpectEcReference(ReadWithPassword(written, cutPhrase.c_str()), ecKey);
    ExpectEcReference(ReadPrivateKey(written, nullptr, nullptr, longPhrase.data()), ecKey);
    {
        PasswordRequest shorter{shortPhrase.c_str()};
        Require(ReadPrivateKey(written, nullptr, SupplyPasswordCallback, &shorter) == nullptr);
        const ErrorList errors = DrainErrors();
        Require(!errors.empty() && errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
    }

    for (const int result : {0, -1}) {
        PasswordRequest refused;
        refused.result = result;
        ERR_clear_error_nid_postfix();
        Require(WriteEncrypted(ecKey, aes256, nullptr, 0, SupplyPasswordCallback, &refused, written) == 0);
        Require(refused.calls == 1 && written.empty());
        ExpectErrorSequence({{PemLibrary, PemReadKey}});
    }
    {
        PasswordRequest oversized;
        oversized.result = PemPasswordBufferSize + 1;
        ExpectInvalidArgument(
            [&] { WriteEncrypted(ecKey, aes256, nullptr, 0, SupplyPasswordCallback, &oversized, written); });
    }

    ERR_clear_error_nid_postfix();
    Require(WriteEncrypted(ecKey, aes256, phrase, -2, nullptr, nullptr, written) == 0);
    Require(std::string(written.begin(), written.end()) == beginLine + endLine);
    ExpectErrorSequence({{EvpErrors, EvpKeygenFailure}, {Pkcs12Errors, Pkcs12AlgorCipherInitError},
        {Pkcs12Errors, Pkcs12EncryptError}, {Pkcs12Errors, Pkcs12EncryptError}});

    LibreSsl3EvpCipher foreign{};
    foreign.nid = 419;
    foreign.blockSize = 16;
    foreign.keyLength = 16;
    foreign.ivLength = 16;
    ExpectUnimplemented([&] { WriteEncrypted(ecKey, &foreign, phrase, phraseLength, nullptr, nullptr, written); });

    ERR_clear_error_nid_postfix();
    Require(PEM_write_bio_PrivateKey_nid_postfix(nullptr, ecKey, aes256, phrase, phraseLength, nullptr, nullptr) == 0);
    ExpectErrorCode(PemLibrary, ErrorReasonBufLibrary);
    ERR_clear_error_nid_postfix();
    ExpectInvalidArgument([&] { WriteEncrypted(nullptr, aes256, phrase, phraseLength, nullptr, nullptr, written); });

    Require(WrittenPrivateBytes(ecKey, nullptr) == plainPem);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecKey);
}

Bytes Pkcs5PbeOid(unsigned char last) {
    return Bytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, last};
}

Bytes Pkcs12PbeOid(unsigned char last) {
    return Bytes{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, last};
}

struct PbeParts {
    Bytes oid;
    Bytes salt;
    Bytes iterations;
    Bytes data;
};

PbeParts SplitPbe(const Bytes& der) {
    const std::vector<Tlv> top = DerChildren(der);
    Require(top.size() == 1 && top[0].tag == 0x30);
    const std::vector<Tlv> key = DerChildren(top[0].content);
    Require(key.size() == 2 && key[0].tag == 0x30 && key[1].tag == 0x04);
    const std::vector<Tlv> algorithm = DerChildren(key[0].content);
    Require(algorithm.size() == 2 && algorithm[0].tag == 0x06 && algorithm[1].tag == 0x30);
    const std::vector<Tlv> parameter = DerChildren(algorithm[1].content);
    Require(parameter.size() == 2 && parameter[0].tag == 0x04 && parameter[1].tag == 0x02);
    PbeParts parts;
    parts.oid = algorithm[0].content;
    parts.salt = parameter[0].content;
    parts.iterations = parameter[1].content;
    parts.data = key[1].content;
    return parts;
}

Bytes PbeParameter(const Bytes& salt, const Bytes& iterations) {
    return Der(0x30, Join({Der(0x04, salt), Der(0x02, iterations)}));
}

Bytes PbeKey(const Bytes& oid, const Bytes& parameter, const Bytes& data) {
    return EncryptedKey(Der(0x30, Join({Der(0x06, oid), parameter})), Der(0x04, data));
}

Bytes PbeRecord(const Bytes& oid, const Bytes& parameter, const Bytes& data) {
    return EncryptedKeyPem(PbeKey(oid, parameter, data));
}

void ExpectGarbageAfterDecrypt(const Bytes& pem, const char* password, bool pkcs8,
    std::source_location location = std::source_location::current()) {
    PasswordRequest request{password};
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(pem, nullptr, SupplyPasswordCallback, &request) == nullptr, location);
    Require(request.calls == 1, location);
    const ErrorList errors = DrainErrors();
    Require(!errors.empty() && errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library}, location);
    Require(!Contains(errors, {EvpErrors, EvpBadDecrypt}), location);
    Require(!Contains(errors, {EvpErrors, EvpWrongFinalBlockLength}), location);
    Require(!Contains(errors, {PemLibrary, PemBadDecrypt}), location);
    Require(!Contains(errors, {Pkcs12Errors, Pkcs12CipherFinalError}), location);
    if (!pkcs8) return;
    Require(errors.size() >= 2, location);
    Require(errors[errors.size() - 2] == std::pair<int, int>{Pkcs12Errors, Pkcs12DecodeError}, location);
}

void TestDesTraditional() {
    auto* ecReference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* rsaReference = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    for (const char* record : {EcTraditionalDes3Pem, EcTraditionalDes2Pem, EcTraditionalDesPem})
        ExpectEcReference(ReadWithPassword(BytesOf(record), TestPassword), ecReference);
    ExpectRsaReference(ReadWithPassword(BytesOf(RsaTraditionalDes3Pem), TestPassword), rsaReference);

    char phrase[] = "anyps5-pem-test";
    ERR_clear_error_nid_postfix();
    ExpectEcReference(ReadPrivateKey(BytesOf(EcTraditionalDes3Pem), nullptr, nullptr, phrase), ecReference);

    struct Variant {
        const char* record;
        const char* name;
        const char* alias;
        const char* validPadding;
    };
    const Variant variants[] = {
        {EcTraditionalDes3Pem, "DES-EDE3-CBC", "DES3", "wrong-162"},
        {EcTraditionalDes2Pem, "DES-EDE-CBC", nullptr, "wrong-300"},
        {EcTraditionalDesPem, "DES-CBC", "DES", "wrong-945"},
    };
    for (const Variant& variant : variants) {
        const Bytes record = BytesOf(variant.record);
        const TraditionalRecord parts = SplitEncryptedRecord(variant.record);
        const std::string prefix = std::string(variant.name) + ",";
        Require(parts.label == "EC PRIVATE KEY" && parts.dekInfo.rfind(prefix, 0) == 0);
        Require(parts.dekInfo.size() == prefix.size() + 16);
        Require(parts.data.size() > 8 && parts.data.size() % 8 == 0);
        const std::string iv = parts.dekInfo.substr(prefix.size());
        std::string lowered = iv;
        for (char& character : lowered) {
            if (character >= 'A' && character <= 'F') character = static_cast<char>(character - 'A' + 'a');
        }
        Require(lowered != iv);

        std::vector<std::string> accepted{prefix + lowered, prefix + iv + iv, std::string(variant.name) + ";" + iv};
        if (variant.alias != nullptr) accepted.push_back(std::string(variant.alias) + "," + iv);
        for (const std::string& dekInfo : accepted)
            ExpectEcReference(ReadWithPassword(EncryptedRecord(parts.label, dekInfo, parts.data), TestPassword),
                ecReference);

        ExpectFailedRead(record, "wrong-password", 1, {{EvpErrors, EvpBadDecrypt}, {PemLibrary, PemBadDecrypt}});
        ExpectGarbageAfterDecrypt(record, variant.validPadding, false);

        for (const std::string& dekInfo : {prefix + iv.substr(0, 14), prefix + "G" + iv.substr(1), prefix})
            ExpectFailedRead(EncryptedRecord(parts.label, dekInfo, parts.data), TestPassword, 0,
                {{PemLibrary, PemBadIvChars}});

        Bytes shortened = parts.data;
        shortened.resize(shortened.size() - 3);
        ExpectFailedRead(EncryptedRecord(parts.label, parts.dekInfo, shortened), TestPassword, 1,
            {{EvpErrors, EvpWrongFinalBlockLength}, {PemLibrary, PemBadDecrypt}});
        shortened.resize(5);
        ExpectFailedRead(EncryptedRecord(parts.label, parts.dekInfo, shortened), TestPassword, 1,
            {{EvpErrors, EvpWrongFinalBlockLength}, {PemLibrary, PemBadDecrypt}});
    }

    const std::pair<const char*, const char*> paddingLookalikes[] = {
        {EcTraditionalDesPem, "wrong-21"}, {EcTraditionalDesPem, "wrong-161"}, {EcTraditionalDes3Pem, "wrong-36"}};
    for (const auto& [record, password] : paddingLookalikes)
        ExpectFailedRead(BytesOf(record), password, 1, {{EvpErrors, EvpBadDecrypt}, {PemLibrary, PemBadDecrypt}});

    const TraditionalRecord parts = SplitEncryptedRecord(EcTraditionalDes3Pem);
    const std::string iv = parts.dekInfo.substr(13);
    for (const char* name : {"DES-ECB", "DES-EDE", "DES-EDE3", "DES-CFB", "DES-OFB", "DES-EDE-CFB", "DES-EDE3-CFB",
             "DES-EDE3-OFB", "DESX", "DESX-CBC", "RC4", "CAMELLIA-128-CBC"}) {
        PasswordRequest request{TestPassword};
        const Bytes record = EncryptedRecord(parts.label, std::string(name) + "," + iv, parts.data);
        ExpectUnimplemented([&] { ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 0);
    }
    for (const char* name : {"DES-EDE3-CBD", "DES2", "3DES", "des3", "DES-EDE2-CBC"})
        ExpectFailedRead(EncryptedRecord(parts.label, std::string(name) + "," + iv, parts.data), TestPassword, 0,
            {{PemLibrary, PemUnsupportedEncryption}});

    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReference);
}

void TestDesPkcs8() {
    auto* ecReference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* rsaReference = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    const char* const records[] = {EcPkcs8Des3Pem, EcPkcs8DesPem, EcPkcs8Md5DesPem, EcPkcs8Sha1DesPem,
        EcPkcs8Pbes1Pem, EcPkcs8Sha1TwoKeyDesPem};
    for (const char* record : records)
        ExpectEcReference(ReadWithPassword(BytesOf(record), TestPassword), ecReference);
    ExpectRsaReference(ReadWithPassword(BytesOf(RsaPkcs8Sha1TripleDesPem), TestPassword), rsaReference);

    Require(SplitPbes2(BodyBytes(BytesOf(EcPkcs8Des3Pem))).cipherOid == DesEde3CbcOidBytes);
    Require(SplitPbes2(BodyBytes(BytesOf(EcPkcs8DesPem))).cipherOid == DesCbcOidBytes);
    Require(SplitPbe(BodyBytes(BytesOf(EcPkcs8Md5DesPem))).oid == Pkcs5PbeOid(0x03));
    Require(SplitPbe(BodyBytes(BytesOf(EcPkcs8Sha1DesPem))).oid == Pkcs5PbeOid(0x0a));
    Require(SplitPbe(BodyBytes(BytesOf(EcPkcs8Pbes1Pem))).oid == Pkcs12PbeOid(0x03));
    Require(SplitPbe(BodyBytes(BytesOf(EcPkcs8Sha1TwoKeyDesPem))).oid == Pkcs12PbeOid(0x04));
    Require(SplitPbe(BodyBytes(BytesOf(RsaPkcs8Sha1TripleDesPem))).oid == Pkcs12PbeOid(0x03));

    char phrase[] = "anyps5-pem-test";
    for (const char* record : {EcPkcs8DesPem, EcPkcs8Sha1DesPem, EcPkcs8Sha1TwoKeyDesPem}) {
        ERR_clear_error_nid_postfix();
        ExpectEcReference(ReadPrivateKey(BytesOf(record), nullptr, nullptr, phrase), ecReference);
    }

    const ErrorList badDecrypt{{EvpErrors, EvpBadDecrypt}, {Pkcs12Errors, Pkcs12CipherFinalError},
        {Pkcs12Errors, Pkcs12PbeCryptError}, {PemLibrary, ErrorReasonAsn1Library}};
    for (const char* record : records) ExpectFailedRead(BytesOf(record), "wrong-password", 1, badDecrypt);
    ExpectFailedRead(BytesOf(RsaPkcs8Sha1TripleDesPem), "wrong-password", 1, badDecrypt);
    ExpectFailedRead(BytesOf(EcPkcs8Sha1DesPem), "wrong-1", 1, badDecrypt);
    ExpectFailedRead(BytesOf(EcPkcs8Sha1DesPem), "wrong-267", 1, badDecrypt);
    ExpectFailedRead(BytesOf(EcPkcs8Sha1TwoKeyDesPem), "wrong-33", 1, badDecrypt);
    ExpectGarbageAfterDecrypt(BytesOf(EcPkcs8DesPem), "wrong-426", true);
    ExpectGarbageAfterDecrypt(BytesOf(EcPkcs8Sha1DesPem), "wrong-675", true);
    ExpectGarbageAfterDecrypt(BytesOf(EcPkcs8Sha1TwoKeyDesPem), "wrong-416", true);

    {
        auto* slot = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec384Pkcs8Pem)));
        auto* const before = slot;
        PasswordRequest wrong{"wrong-password"};
        ERR_clear_error_nid_postfix();
        Require(ReadPrivateKey(BytesOf(EcPkcs8Pbes1Pem), &slot, SupplyPasswordCallback, &wrong) == nullptr);
        Require(wrong.calls == 1 && slot == before);
        ExpectErrorSequence(badDecrypt);
        PasswordRequest right{TestPassword};
        auto* replaced = ReadPrivateKey(BytesOf(EcPkcs8Pbes1Pem), &slot, SupplyPasswordCallback, &right);
        Require(replaced != nullptr && slot == replaced);
        ExpectEcReference(replaced, ecReference);
    }

    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReference);
}

void TestDesPbes2Parameters() {
    auto* reference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    const ErrorList wrongFinalBlock{{EvpErrors, EvpWrongFinalBlockLength}, {Pkcs12Errors, Pkcs12CipherFinalError},
        {Pkcs12Errors, Pkcs12PbeCryptError}, {PemLibrary, ErrorReasonAsn1Library}};
    struct Variant {
        const char* record;
        const Bytes* cipherOid;
        unsigned char keyLength;
        unsigned char otherKeyLength;
    };
    const Variant variants[] = {{EcPkcs8DesPem, &DesCbcOidBytes, 0x08, 0x18},
        {EcPkcs8Des3Pem, &DesEde3CbcOidBytes, 0x18, 0x10}};
    for (const Variant& variant : variants) {
        const Bytes der = BodyBytes(BytesOf(variant.record));
        const Pbes2Parts parts = SplitPbes2(der);
        Require(Rebuild(parts) == der);
        Require(parts.cipherOid == *variant.cipherOid && parts.iv.size() == 8 && parts.prfOid == HmacSha256OidBytes);
        Require(parts.data.size() > 8 && parts.data.size() % 8 == 0);
        const Bytes salt = Der(0x04, parts.salt);
        const Bytes iterations = Der(0x02, parts.iterations);
        const Bytes prf = PrfAlgorithm(parts.prfOid);
        const Bytes keyFunction = KeyFunction(KdfParameters(parts));
        const Bytes encryption = Encryption(parts.cipherOid, Der(0x04, parts.iv));
        const Bytes digest = Der(0x04, parts.data);
        const auto record = [&](const Bytes& algorithm) { return EncryptedKeyPem(EncryptedKey(algorithm, digest)); };
        const auto withIv = [&](const Bytes& parameter) {
            return record(Pbes2Algorithm(keyFunction, Encryption(parts.cipherOid, parameter)));
        };
        const auto withKdf = [&](const Bytes& kdfBody) {
            return record(Pbes2Algorithm(KeyFunction(Der(0x30, kdfBody)), encryption));
        };

        for (const Bytes& parameter : {Der(0x04, Bytes(16, 0)), Der(0x04, Bytes(7, 0)), Der(0x04, Bytes(9, 0)),
                 Der(0x04, {})})
            ExpectFailedRead(withIv(parameter), TestPassword, 1,
                KeygenFailure({{EvpErrors, EvpCipherParameterError}}));
        ExpectFailedRead(withIv(Der(0x05, {})), TestPassword, 1,
            KeygenFailure({{Asn1Errors, Asn1DataIsWrong}, {EvpErrors, EvpCipherParameterError}}));
        {
            PasswordRequest request{TestPassword};
            ERR_clear_error_nid_postfix();
            const Bytes withoutIv = record(Pbes2Algorithm(keyFunction, Der(0x30, Der(0x06, parts.cipherOid))));
            Require(ReadPrivateKey(withoutIv, nullptr, SupplyPasswordCallback, &request) == nullptr);
            Require(request.calls == 1);
            const ErrorList errors = DrainErrors();
            Require(errors.size() >= 2);
            Require(errors[errors.size() - 2] == std::pair<int, int>{Pkcs12Errors, Pkcs12DecodeError});
            Require(errors.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
            Require(!Contains(errors, {Pkcs12Errors, Pkcs12CipherFinalError}));
        }

        ExpectEcReference(
            ReadWithPassword(withKdf(Join({salt, iterations, Der(0x02, {variant.keyLength}), prf})), TestPassword),
            reference);
        ExpectFailedRead(withKdf(Join({salt, iterations, Der(0x02, {variant.otherKeyLength}), prf})), TestPassword, 1,
            KeygenFailure({{EvpErrors, EvpUnsupportedKeylength}}));
        ExpectFailedRead(withKdf(Join({salt, Der(0x02, {0x00}), prf})), TestPassword, 1,
            KeygenFailure({{EvpErrors, EvpUnsupportedNumberOfRounds}}));

        const Bytes algorithm = Pbes2Algorithm(keyFunction, encryption);
        Bytes shortened = parts.data;
        shortened.pop_back();
        ExpectFailedRead(EncryptedKeyPem(EncryptedKey(algorithm, Der(0x04, shortened))), TestPassword, 1,
            wrongFinalBlock);
        ExpectFailedRead(EncryptedKeyPem(EncryptedKey(algorithm, Der(0x04, {}))), TestPassword, 1, wrongFinalBlock);

        PasswordRequest request{TestPassword};
        const Bytes ecbRecord = record(Pbes2Algorithm(keyFunction, Encryption(DesEdeEcbOidBytes, Der(0x04, parts.iv))));
        ExpectUnimplemented([&] { ReadPrivateKey(ecbRecord, nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 1);
    }
    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

void TestDesPbeParameters() {
    auto* reference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    const ErrorList wrongFinalBlock{{EvpErrors, EvpWrongFinalBlockLength}, {Pkcs12Errors, Pkcs12CipherFinalError},
        {Pkcs12Errors, Pkcs12PbeCryptError}, {PemLibrary, ErrorReasonAsn1Library}};
    struct Family {
        const char* record;
        std::pair<int, int> decodeError;
        std::pair<int, int> roundsError;
    };
    const Family families[] = {
        {EcPkcs8Sha1DesPem, {EvpErrors, EvpDecodeError}, {EvpErrors, EvpUnsupportedNumberOfRounds}},
        {EcPkcs8Md5DesPem, {EvpErrors, EvpDecodeError}, {EvpErrors, EvpUnsupportedNumberOfRounds}},
        {EcPkcs8Sha1TwoKeyDesPem, {Pkcs12Errors, Pkcs12DecodeError}, {Pkcs12Errors, Pkcs12DecodeError}},
        {EcPkcs8Pbes1Pem, {Pkcs12Errors, Pkcs12DecodeError}, {Pkcs12Errors, Pkcs12DecodeError}},
    };
    for (const Family& family : families) {
        const Bytes der = BodyBytes(BytesOf(family.record));
        const PbeParts parts = SplitPbe(der);
        Require(PbeKey(parts.oid, PbeParameter(parts.salt, parts.iterations), parts.data) == der);
        Require(parts.salt.size() == 8 && parts.data.size() > 8 && parts.data.size() % 8 == 0);
        const Bytes salt = Der(0x04, parts.salt);
        const Bytes iterations = Der(0x02, parts.iterations);
        const auto with = [&](const Bytes& parameter) { return PbeRecord(parts.oid, parameter, parts.data); };
        const auto refused = [&](const Bytes& pem, ErrorList errors, const std::pair<int, int>& last,
                                 std::source_location location = std::source_location::current()) {
            errors.push_back(last);
            ExpectFailedRead(pem, TestPassword, 1, KeygenFailure(std::move(errors)), location);
        };

        ExpectEcReference(ReadWithPassword(with(Der(0x30, Join({salt, iterations}))), TestPassword), reference);

        refused(EncryptedKeyPem(EncryptedKey(Der(0x30, Der(0x06, parts.oid)), Der(0x04, parts.data))), {},
            family.decodeError);
        refused(with(Der(0x05, {})), {}, family.decodeError);
        refused(with(salt), {}, family.decodeError);
        refused(with(Der(0x30, {})), {{Asn1Errors, Asn1FieldMissing}}, family.decodeError);
        refused(with(Der(0x30, salt)), {{Asn1Errors, Asn1FieldMissing}}, family.decodeError);
        refused(with(Der(0x30, Join({Der(0x02, {0x01}), iterations}))),
            {{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error}, {Asn1Errors, Asn1NestedAsn1Error}},
            family.decodeError);
        refused(with(Der(0x30, Join({salt, Der(0x04, {0x01})}))),
            {{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error}, {Asn1Errors, Asn1NestedAsn1Error}},
            family.decodeError);
        refused(with(Der(0x30, Join({salt, iterations, Der(0x05, {})}))),
            {{Asn1Errors, Asn1SequenceLengthMismatch}}, family.decodeError);
        refused(with(Der(0x30, Join({Bytes{0x00, 0x00}, iterations}))), {{Asn1Errors, Asn1UnexpectedEoc}},
            family.decodeError);
        refused(with(Der(0x30, Join({salt, Der(0x02, {0x00, 0x01})}))),
            {{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error}}, family.decodeError);
        refused(with(Der(0x30, Join({salt, Der(0x02, {0x00})}))), {}, family.roundsError);
        refused(with(Der(0x30, Join({salt, Der(0x02, {0xff})}))), {}, family.roundsError);
        refused(with(Der(0x30, Join({salt, Der(0x02, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})}))),
            {{Asn1Errors, Asn1TooLarge}}, family.roundsError);

        const Bytes parameter = PbeParameter(parts.salt, parts.iterations);
        Bytes shortened = parts.data;
        shortened.pop_back();
        ExpectFailedRead(PbeRecord(parts.oid, parameter, shortened), TestPassword, 1, wrongFinalBlock);
        ExpectFailedRead(PbeRecord(parts.oid, parameter, {}), TestPassword, 1, wrongFinalBlock);
    }

    const PbeParts md5 = SplitPbe(BodyBytes(BytesOf(EcPkcs8Md5DesPem)));
    const Bytes parameter = PbeParameter(md5.salt, md5.iterations);
    const ErrorList unknownDigest = CipherInitFailure({{EvpErrors, EvpUnknownDigest}});
    for (const Bytes& oid : {Pkcs5PbeOid(0x01), Pkcs5PbeOid(0x04)}) {
        ExpectFailedRead(PbeRecord(oid, parameter, md5.data), TestPassword, 1, unknownDigest);
        ExpectFailedRead(PbeRecord(oid, Der(0x05, {}), md5.data), TestPassword, 1, unknownDigest);
        ExpectFailedRead(EncryptedKeyPem(EncryptedKey(Der(0x30, Der(0x06, oid)), Der(0x04, md5.data))), TestPassword, 1,
            unknownDigest);
    }
    for (const Bytes& oid : {Pkcs5PbeOid(0x06), Pkcs5PbeOid(0x0b), Pkcs12PbeOid(0x01), Pkcs12PbeOid(0x02),
             Pkcs12PbeOid(0x05), Pkcs12PbeOid(0x06)}) {
        PasswordRequest request{TestPassword};
        const Bytes record = PbeRecord(oid, parameter, md5.data);
        ExpectUnimplemented([&] { ReadPrivateKey(record, nullptr, SupplyPasswordCallback, &request); });
        Require(request.calls == 1);
    }
    const ErrorList unknownScheme = CipherInitFailure({{EvpErrors, EvpUnknownPbeAlgorithm}});
    for (const Bytes& oid : {Pkcs5PbeOid(0x02), Pkcs5PbeOid(0x0e), Pkcs12PbeOid(0x07)})
        ExpectFailedRead(PbeRecord(oid, parameter, md5.data), TestPassword, 1, unknownScheme);

    EVP_PKEY_free_nid_disambig1_nid_postfix(reference);
}

void TestErrors() {
    std::vector<unsigned char> damaged = BytesOf(RsaPkcs8Pem);
    std::size_t bodyStart = 0;
    while (damaged[bodyStart] != '\n') ++bodyStart;
    damaged[++bodyStart] = '!';
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(damaged, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, PemBadBase64Decode);

    std::string truncated(RsaPkcs8Pem);
    truncated.erase(truncated.rfind("-----END"));
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(std::vector<unsigned char>(truncated.begin(), truncated.end()), nullptr, nullptr, nullptr)
        == nullptr);
    ExpectErrorCode(PemLibrary, PemBadEndLine);

    const std::vector<unsigned char> certificate = BytesOf(CertificatePem);
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(certificate, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, PemNoStartLine);
    ERR_clear_error_nid_postfix();
    Require(ReadPublicKey(certificate, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, PemNoStartLine);

    ERR_clear_error_nid_postfix();
    auto* emptyBio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(emptyBio != nullptr);
    Require(PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(emptyBio, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, PemNoStartLine);
    Require(PEM_read_bio_PUBKEY_nid_postfix(emptyBio, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, PemNoStartLine);
    BIO_free_nid_postfix(emptyBio);

    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(EmptyRecordPem), nullptr, nullptr, nullptr) == nullptr);
    ExpectEmptyQueue();

    auto* key = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    ERR_clear_error_nid_postfix();
    Require(PEM_write_bio_PrivateKey_nid_postfix(nullptr, key, nullptr, nullptr, 0, nullptr, nullptr) == 0);
    ExpectErrorCode(PemLibrary, 7);
    ERR_clear_error_nid_postfix();
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void ExpectHeaderFailure(const char* record, int reason) {
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(record), nullptr, CountingPasswordCallback, nullptr) == nullptr);
    ExpectErrorCode(PemLibrary, reason);
    ExpectEmptyQueue();
}

void ExpectSilentHeaderFailure(const char* record) {
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(record), nullptr, CountingPasswordCallback, nullptr) == nullptr);
    ExpectEmptyQueue();
}

void TestHeaders() {
    passwordCallbackCalls = 0;
    ExpectHeaderFailure(UnknownHeaderPem, PemNotProcType);
    ExpectHeaderFailure(MicOnlyHeaderPem, PemNotEncrypted);
    ExpectHeaderFailure(MissingDekInfoPem, PemNotDekInfo);
    ExpectSilentHeaderFailure(WrongVersionHeaderPem);
    ExpectSilentHeaderFailure(MissingCommaHeaderPem);
    Require(passwordCallbackCalls == 0);
}

void TestLabels() {
    std::vector<unsigned char> skipped = BytesOf(UnknownPrivateKeyLabelPem);
    for (const char* record : {RsaPssPrivateKeyLabelPem, Ed25519PrivateKeyLabelPem, RsaPublicKeyLabelPem}) {
        const std::vector<unsigned char> bytes = BytesOf(record);
        skipped.insert(skipped.end(), bytes.begin(), bytes.end());
    }
    const std::vector<unsigned char> valid = BytesOf(Ec256Pkcs8Pem);
    skipped.insert(skipped.end(), valid.begin(), valid.end());
    ERR_clear_error_nid_postfix();
    auto* afterSkipped = ReadPrivateKey(skipped, nullptr, nullptr, nullptr);
    Require(afterSkipped != nullptr && afterSkipped->type == EcType);
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(afterSkipped);

    std::string lowered(RsaTraditionalPem);
    for (std::size_t position = lowered.find("RSA PRIVATE KEY"); position != std::string::npos;
         position = lowered.find("RSA PRIVATE KEY", position))
        lowered.replace(position, 3, "rsa");
    auto* loweredKey = ReadPrivateKey(std::vector<unsigned char>(lowered.begin(), lowered.end()), nullptr, nullptr, nullptr);
    Require(loweredKey != nullptr && loweredKey->type == RsaType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(loweredKey);

    std::string mixed(Ec256TraditionalPem);
    for (std::size_t position = mixed.find("EC PRIVATE KEY"); position != std::string::npos;
         position = mixed.find("EC PRIVATE KEY", position))
        mixed.replace(position, 2, "eC");
    auto* mixedKey = ReadPrivateKey(std::vector<unsigned char>(mixed.begin(), mixed.end()), nullptr, nullptr, nullptr);
    Require(mixedKey != nullptr && mixedKey->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(mixedKey);

    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(DsaPrivateKeyLabelPem), nullptr, nullptr, nullptr) == nullptr);
    ExpectLastError(PemLibrary, ErrorReasonAsn1Library);
}

void TestCorruptTraditionalEc() {
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(CorruptEcPrivateKeyPem), nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1TooLong}, {EcLibrary, ErrorReasonEcLibrary},
        {ErrorReasonAsn1Library, Asn1TooLong}, {PemLibrary, ErrorReasonAsn1Library}});
}

void TestPkcs8InTraditionalLabelFallback() {
    const std::vector<unsigned char> ecInRsa = BytesOf(Ec256Pkcs8InRsaLabelPem);
    const std::vector<unsigned char> rsaInEc = BytesOf(RsaPkcs8InEcLabelPem);
    auto* ecReference = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* rsaReference = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    auto* ecTraditional = ReadPrivateKey(BytesOf(Ec256TraditionalPem), nullptr, nullptr, nullptr);
    auto* rsaTraditional = ReadPrivateKey(BytesOf(RsaTraditionalPem), nullptr, nullptr, nullptr);

    ERR_clear_error_nid_postfix();
    auto* bioEcInRsa1 = MemoryBio(ecInRsa);
    auto* keyEcInRsa1 = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioEcInRsa1, nullptr, nullptr, nullptr);
    BIO_free_nid_postfix(bioEcInRsa1);
    Require(keyEcInRsa1 != nullptr);
    Require(keyEcInRsa1->type == EcType);
    Require(EVP_PKEY_id_nid_postfix(keyEcInRsa1) == EcType);
    Require(EVP_PKEY_base_id_nid_postfix(keyEcInRsa1) == EcType);
    CompareEc(static_cast<const EcKey*>(keyEcInRsa1->key), static_cast<const EcKey*>(ecReference->key), Secp256r1, true);
    CompareEc(static_cast<const EcKey*>(keyEcInRsa1->key), static_cast<const EcKey*>(ecTraditional->key), Secp256r1, true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary}});

    ERR_clear_error_nid_postfix();
    LibreSsl3EvpPkey* slotEcNull = nullptr;
    auto* bioEcInRsaNull = MemoryBio(ecInRsa);
    auto* keyEcInRsaNull = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioEcInRsaNull, &slotEcNull, nullptr, nullptr);
    BIO_free_nid_postfix(bioEcInRsaNull);
    Require(keyEcInRsaNull != nullptr);
    Require(slotEcNull == keyEcInRsaNull);
    Require(keyEcInRsaNull->type == EcType);
    Require(EVP_PKEY_id_nid_postfix(keyEcInRsaNull) == EcType);
    Require(EVP_PKEY_base_id_nid_postfix(keyEcInRsaNull) == EcType);
    CompareEc(static_cast<const EcKey*>(keyEcInRsaNull->key), static_cast<const EcKey*>(ecReference->key), Secp256r1, true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary}});

    ERR_clear_error_nid_postfix();
    auto* rsaPreExisting = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    auto* slotEcReplace = rsaPreExisting;
    auto* bioEcInRsaReplace = MemoryBio(ecInRsa);
    auto* keyEcInRsaReplace = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioEcInRsaReplace, &slotEcReplace, nullptr, nullptr);
    BIO_free_nid_postfix(bioEcInRsaReplace);
    Require(keyEcInRsaReplace != nullptr);
    Require(slotEcReplace == keyEcInRsaReplace);
    Require(keyEcInRsaReplace->type == EcType);
    Require(EVP_PKEY_id_nid_postfix(keyEcInRsaReplace) == EcType);
    Require(EVP_PKEY_base_id_nid_postfix(keyEcInRsaReplace) == EcType);
    CompareEc(static_cast<const EcKey*>(keyEcInRsaReplace->key), static_cast<const EcKey*>(ecReference->key), Secp256r1, true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary}});
    ERR_clear_error_nid_postfix();
    ExpectEmptyQueue();

    ERR_clear_error_nid_postfix();
    auto* bioRsaInEc1 = MemoryBio(rsaInEc);
    auto* keyRsaInEc1 = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioRsaInEc1, nullptr, nullptr, nullptr);
    BIO_free_nid_postfix(bioRsaInEc1);
    Require(keyRsaInEc1 != nullptr);
    Require(keyRsaInEc1->type == RsaType);
    Require(EVP_PKEY_id_nid_postfix(keyRsaInEc1) == RsaType);
    Require(EVP_PKEY_base_id_nid_postfix(keyRsaInEc1) == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(keyRsaInEc1->key), static_cast<const LibreSsl3Rsa*>(rsaReference->key), true);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(keyRsaInEc1->key), static_cast<const LibreSsl3Rsa*>(rsaTraditional->key), true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {EcLibrary, ErrorReasonEcLibrary}});

    ERR_clear_error_nid_postfix();
    LibreSsl3EvpPkey* slotRsaNull = nullptr;
    auto* bioRsaInEcNull = MemoryBio(rsaInEc);
    auto* keyRsaInEcNull = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioRsaInEcNull, &slotRsaNull, nullptr, nullptr);
    BIO_free_nid_postfix(bioRsaInEcNull);
    Require(keyRsaInEcNull != nullptr);
    Require(slotRsaNull == keyRsaInEcNull);
    Require(keyRsaInEcNull->type == RsaType);
    Require(EVP_PKEY_id_nid_postfix(keyRsaInEcNull) == RsaType);
    Require(EVP_PKEY_base_id_nid_postfix(keyRsaInEcNull) == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(keyRsaInEcNull->key), static_cast<const LibreSsl3Rsa*>(rsaReference->key), true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {EcLibrary, ErrorReasonEcLibrary}});

    ERR_clear_error_nid_postfix();
    auto* ecPreExisting = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    auto* slotRsaReplace = ecPreExisting;
    auto* bioRsaInEcReplace = MemoryBio(rsaInEc);
    auto* keyRsaInEcReplace = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bioRsaInEcReplace, &slotRsaReplace, nullptr, nullptr);
    BIO_free_nid_postfix(bioRsaInEcReplace);
    Require(keyRsaInEcReplace != nullptr);
    Require(slotRsaReplace == keyRsaInEcReplace);
    Require(keyRsaInEcReplace->type == RsaType);
    Require(EVP_PKEY_id_nid_postfix(keyRsaInEcReplace) == RsaType);
    Require(EVP_PKEY_base_id_nid_postfix(keyRsaInEcReplace) == RsaType);
    CompareRsa(static_cast<const LibreSsl3Rsa*>(keyRsaInEcReplace->key), static_cast<const LibreSsl3Rsa*>(rsaReference->key), true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {EcLibrary, ErrorReasonEcLibrary}});
    ERR_clear_error_nid_postfix();
    ExpectEmptyQueue();

    EVP_PKEY_free_nid_disambig1_nid_postfix(keyEcInRsa1);
    EVP_PKEY_free_nid_disambig1_nid_postfix(keyEcInRsaNull);
    EVP_PKEY_free_nid_disambig1_nid_postfix(keyEcInRsaReplace);
    EVP_PKEY_free_nid_disambig1_nid_postfix(keyRsaInEc1);
    EVP_PKEY_free_nid_disambig1_nid_postfix(keyRsaInEcNull);
    EVP_PKEY_free_nid_disambig1_nid_postfix(keyRsaInEcReplace);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecTraditional);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaTraditional);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReference);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReference);
}

void TestNormalTraditionalCleanQueue() {
    ERR_clear_error_nid_postfix();
    const std::vector<unsigned char> rsaTradBytes = BytesOf(RsaTraditionalPem);
    auto* rsaBio = MemoryBio(rsaTradBytes);
    auto* rsaKey = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(rsaBio, nullptr, nullptr, nullptr);
    BIO_free_nid_postfix(rsaBio);
    Require(rsaKey != nullptr);
    Require(rsaKey->type == RsaType);
    Require(EVP_PKEY_id_nid_postfix(rsaKey) == RsaType);
    Require(EVP_PKEY_base_id_nid_postfix(rsaKey) == RsaType);
    ExpectEmptyQueue();

    auto* rsaRef = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    CompareRsa(static_cast<const LibreSsl3Rsa*>(rsaKey->key), static_cast<const LibreSsl3Rsa*>(rsaRef->key), true);

    const void* rsaN = nullptr;
    const void* rsaE = nullptr;
    const void* rsaD = nullptr;
    RSA_get0_key_nid_postfix(static_cast<const LibreSsl3Rsa*>(rsaKey->key), &rsaN, &rsaE, &rsaD);
    Require(rsaN != nullptr && rsaE != nullptr && rsaD != nullptr);
    const void* refN = nullptr;
    const void* refE = nullptr;
    const void* refD = nullptr;
    RSA_get0_key_nid_postfix(static_cast<const LibreSsl3Rsa*>(rsaRef->key), &refN, &refE, &refD);
    CompareBignums(rsaN, refN);
    CompareBignums(rsaE, refE);
    CompareBignums(rsaD, refD);

    LibreSsl3EvpPkey* rsaSlotNull = nullptr;
    auto* rsaBioNull = MemoryBio(rsaTradBytes);
    auto* rsaKeyNull = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(rsaBioNull, &rsaSlotNull, nullptr, nullptr);
    BIO_free_nid_postfix(rsaBioNull);
    Require(rsaKeyNull != nullptr);
    Require(rsaSlotNull == rsaKeyNull);
    ExpectEmptyQueue();
    CompareRsa(static_cast<const LibreSsl3Rsa*>(rsaKeyNull->key), static_cast<const LibreSsl3Rsa*>(rsaRef->key), true);

    auto* rsaSlotReplace = rsaKey;
    auto* rsaBioReplace = MemoryBio(rsaTradBytes);
    auto* rsaReplaced = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(rsaBioReplace, &rsaSlotReplace, nullptr, nullptr);
    BIO_free_nid_postfix(rsaBioReplace);
    Require(rsaReplaced != nullptr);
    Require(rsaSlotReplace == rsaReplaced);
    ExpectEmptyQueue();
    CompareRsa(static_cast<const LibreSsl3Rsa*>(rsaReplaced->key), static_cast<const LibreSsl3Rsa*>(rsaRef->key), true);

    ERR_clear_error_nid_postfix();
    const std::vector<unsigned char> ecTradBytes = BytesOf(Ec256TraditionalPem);
    auto* ecBio = MemoryBio(ecTradBytes);
    auto* ecKey = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(ecBio, nullptr, nullptr, nullptr);
    BIO_free_nid_postfix(ecBio);
    Require(ecKey != nullptr);
    Require(ecKey->type == EcType);
    Require(EVP_PKEY_id_nid_postfix(ecKey) == EcType);
    Require(EVP_PKEY_base_id_nid_postfix(ecKey) == EcType);
    ExpectEmptyQueue();

    auto* ecRef = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    CompareEc(static_cast<const EcKey*>(ecKey->key), static_cast<const EcKey*>(ecRef->key), Secp256r1, true);

    const auto* ecKeyStruct = static_cast<const EcKey*>(ecKey->key);
    const auto* ecRefStruct = static_cast<const EcKey*>(ecRef->key);
    Require(ecKeyStruct->group != nullptr && ecRefStruct->group != nullptr);
    Require(ecKeyStruct->group->curve.nid == Secp256r1);
    Require(ecRefStruct->group->curve.nid == Secp256r1);
    Require(ecKeyStruct->privateKey != nullptr && ecRefStruct->privateKey != nullptr);
    Require(mbedtls_mpi_cmp_mpi(&ecKeyStruct->privateKey->value, &ecRefStruct->privateKey->value) == 0);

    LibreSsl3EvpPkey* ecSlotNull = nullptr;
    auto* ecBioNull = MemoryBio(ecTradBytes);
    auto* ecKeyNull = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(ecBioNull, &ecSlotNull, nullptr, nullptr);
    BIO_free_nid_postfix(ecBioNull);
    Require(ecKeyNull != nullptr);
    Require(ecSlotNull == ecKeyNull);
    ExpectEmptyQueue();
    CompareEc(static_cast<const EcKey*>(ecKeyNull->key), static_cast<const EcKey*>(ecRef->key), Secp256r1, true);

    auto* ecSlotReplace = ecKey;
    auto* ecBioReplace = MemoryBio(ecTradBytes);
    auto* ecReplaced = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(ecBioReplace, &ecSlotReplace, nullptr, nullptr);
    BIO_free_nid_postfix(ecBioReplace);
    Require(ecReplaced != nullptr);
    Require(ecSlotReplace == ecReplaced);
    ExpectEmptyQueue();
    CompareEc(static_cast<const EcKey*>(ecReplaced->key), static_cast<const EcKey*>(ecRef->key), Secp256r1, true);

    ERR_clear_error_nid_postfix();
    ExpectEmptyQueue();

    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaKeyNull);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReplaced);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaRef);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecKeyNull);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecReplaced);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecRef);
}

void TestTraditionalDerValidationAndContext() {
    const std::vector<unsigned char> ec384Pkcs8Der = BodyBytes(BytesOf(Ec384Pkcs8Pem));
    auto* ec384Ref = KeyFromPkcs8Der(ec384Pkcs8Der);
    const std::vector<unsigned char> ec384InRsaPem = EncodePemRecord("RSA PRIVATE KEY", ec384Pkcs8Der);
    ERR_clear_error_nid_postfix();
    auto* ec384FromRsaLabel = ReadPrivateKey(ec384InRsaPem, nullptr, nullptr, nullptr);
    Require(ec384FromRsaLabel != nullptr);
    Require(ec384FromRsaLabel->type == EcType);
    CompareEc(static_cast<const EcKey*>(ec384FromRsaLabel->key), static_cast<const EcKey*>(ec384Ref->key), Secp384r1, true);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary}});
    EVP_PKEY_free_nid_disambig1_nid_postfix(ec384FromRsaLabel);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ec384Ref);

    auto* preservedSlot = KeyFromPkcs8Der(BodyBytes(BytesOf(Ec256Pkcs8Pem)));
    LibreSsl3EvpPkey* originalSlot = preservedSlot;

    const std::vector<unsigned char> ecVer0WrongKeyTagPem = EncodePemRecord(
        "EC PRIVATE KEY", {0x30, 0x05, 0x02, 0x01, 0x00, 0x05, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(ecVer0WrongKeyTagPem, &preservedSlot, nullptr, nullptr) == nullptr);
    Require(preservedSlot == originalSlot);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {EcLibrary, ErrorReasonEcLibrary},
        {ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> ecMissingPrivKeyPem = EncodePemRecord(
        "EC PRIVATE KEY", {0x30, 0x03, 0x02, 0x01, 0x01});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(ecMissingPrivKeyPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1FieldMissing}, {EcLibrary, ErrorReasonEcLibrary},
        {ErrorReasonAsn1Library, Asn1FieldMissing}, {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaWrongModTagPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x05, 0x02, 0x01, 0x00, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaWrongModTagPem, &preservedSlot, nullptr, nullptr) == nullptr);
    Require(preservedSlot == originalSlot);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary},
        {ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {PemLibrary, ErrorReasonAsn1Library}});
    EVP_PKEY_free_nid_disambig1_nid_postfix(preservedSlot);

    const std::vector<unsigned char> rsaVer1WrongModTagPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x05, 0x02, 0x01, 0x01, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaVer1WrongModTagPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary},
        {ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaWideVersionWrongModTagPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x09, 0x02, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaWideVersionWrongModTagPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {RsaLibrary, ErrorReasonRsaLibrary},
        {ErrorReasonAsn1Library, Asn1WrongTag}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaLongSentinelPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x08, 0x02, 0x04, 0x7f, 0xff, 0xff, 0xff, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaLongSentinelPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1IntegerTooLargeForLong}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {RsaLibrary, ErrorReasonRsaLibrary}, {ErrorReasonAsn1Library, Asn1WrongTag},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaSignedOverflowPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x0d, 0x02, 0x09, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaSignedOverflowPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1TooLarge}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {RsaLibrary, ErrorReasonRsaLibrary}, {ErrorReasonAsn1Library, Asn1WrongTag},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaUnsignedOverflowPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x0d, 0x02, 0x09, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0x04, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaUnsignedOverflowPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1TooLarge}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {RsaLibrary, ErrorReasonRsaLibrary}, {ErrorReasonAsn1Library, Asn1WrongTag},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaEmptyModIntPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x05, 0x02, 0x01, 0x00, 0x02, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaEmptyModIntPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1LengthMismatch}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {RsaLibrary, ErrorReasonRsaLibrary}, {ErrorReasonAsn1Library, Asn1WrongTag},
        {ErrorReasonAsn1Library, Asn1NestedAsn1Error}, {ErrorReasonAsn1Library, Asn1NestedAsn1Error},
        {PemLibrary, ErrorReasonAsn1Library}});

    const std::vector<unsigned char> rsaMissingModPem = EncodePemRecord(
        "RSA PRIVATE KEY", {0x30, 0x03, 0x02, 0x01, 0x00});
    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(rsaMissingModPem, nullptr, nullptr, nullptr) == nullptr);
    ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1FieldMissing}, {RsaLibrary, ErrorReasonRsaLibrary},
        {ErrorReasonAsn1Library, Asn1FieldMissing}, {PemLibrary, ErrorReasonAsn1Library}});
}

class GuestPemFileFixture {
public:
    explicit GuestPemFileFixture(const std::string& content) {
        std::random_device random;
        bool created = false;
        for (int attempt = 0; attempt < 16 && !created; ++attempt) {
            directory = "libressl3-pem-fixture-" + std::to_string(random());
            created = std::filesystem::create_directory(directory);
        }
        Require(created);
        path = directory + "/key.pem";
        FileStream* writer = fopen_nid_postfix(path.c_str(), "wb");
        Require(writer != nullptr);
        if (!content.empty()) {
            const std::size_t written = fwrite_nid_postfix(content.data(), 1, content.size(), writer);
            Require(written == content.size());
        }
        Require(fclose_nid_postfix(writer) == 0);
        stream = fopen_nid_postfix(path.c_str(), "rb");
        Require(stream != nullptr);
    }

    ~GuestPemFileFixture() {
        if (stream != nullptr) fclose_nid_postfix(stream);
        remove_nid_postfix(path.c_str());
        std::error_code error;
        std::filesystem::remove(directory, error);
    }

    FileStream* Get() const { return stream; }

    void CloseAndVerify(std::source_location location = std::source_location::current()) {
        Require(stream != nullptr, location);
        FileStream* target = stream;
        stream = nullptr;
        Require(fclose_nid_postfix(target) == 0, location);
    }

private:
    std::string directory;
    std::string path;
    FileStream* stream = nullptr;
};

int APS5_VABI CountingRsaFilePasswordCallback(char*, int, int, void* userData) {
    auto* count = static_cast<int*>(userData);
    ++(*count);
    return 0;
}

void TestRsaPrivateFile() {
    constexpr int EvpLibrary = 6;
    constexpr int EvpRExpectingAnRsaKey = 127;

    auto* rsaReferencePkey = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    const auto* rsaReference = static_cast<const LibreSsl3Rsa*>(rsaReferencePkey->key);

    {
        ERR_clear_error_nid_postfix();
        std::string payload = RsaTraditionalPem;
        payload += "AFTER_PEM_MARKER\n";
        GuestPemFileFixture file(payload);
        int callbackCount = 0;
        LibreSsl3Rsa* key = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
            file.Get(), nullptr, CountingRsaFilePasswordCallback, &callbackCount);
        Require(key != nullptr);
        Require(callbackCount == 0);
        Require(key->method == nullptr);
        Require(key->references == 1);
        CompareRsa(key, rsaReference, true);
        Require(RSA_check_key_nid_postfix(key) == 1);
        ExpectEmptyQueue();

        char line[64] {};
        Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), file.Get()) != nullptr);
        Require(std::strcmp(line, "AFTER_PEM_MARKER\n") == 0);
        file.CloseAndVerify();

        Require(RSA_up_ref_nid_postfix(key) == 1);
        Require(key->references == 2);
        RSA_free_nid_disambig1_nid_postfix(key);
        Require(key->references == 1);
        CompareRsa(key, rsaReference, true);
        RSA_free_nid_disambig1_nid_postfix(key);
    }

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(RsaPkcs8Pem);
        LibreSsl3Rsa* slot = nullptr;
        LibreSsl3Rsa* key = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr);
        Require(key != nullptr);
        Require(slot == key);
        Require(key->method == nullptr);
        Require(key->references == 1);
        CompareRsa(key, rsaReference, true);
        Require(RSA_check_key_nid_postfix(key) == 1);
        ExpectEmptyQueue();
        file.CloseAndVerify();
        RSA_free_nid_disambig1_nid_postfix(key);
    }

    LibreSsl3Rsa* preserved = nullptr;
    {
        GuestPemFileFixture seed(RsaPkcs8Pem);
        preserved = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(seed.Get(), nullptr, nullptr, nullptr);
        Require(preserved != nullptr);
        seed.CloseAndVerify();
    }
    Require(RSA_up_ref_nid_postfix(preserved) == 1);
    Require(preserved->references == 2);

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(RsaTraditionalPem);
        LibreSsl3Rsa* slot = preserved;
        LibreSsl3Rsa* replaced = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr);
        Require(replaced != nullptr);
        Require(slot == replaced);
        Require(replaced != preserved);
        Require(replaced->references == 1);
        Require(preserved->references == 1);
        CompareRsa(replaced, rsaReference, true);
        ExpectEmptyQueue();
        file.CloseAndVerify();

        LibreSsl3Rsa* secondSlot = replaced;
        GuestPemFileFixture secondFile(RsaPkcs8Pem);
        LibreSsl3Rsa* secondReplaced = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
            secondFile.Get(), &secondSlot, nullptr, nullptr);
        Require(secondReplaced != nullptr);
        Require(secondSlot == secondReplaced);
        Require(secondReplaced != replaced);
        Require(secondReplaced->references == 1);
        ExpectEmptyQueue();
        secondFile.CloseAndVerify();
        RSA_free_nid_disambig1_nid_postfix(secondReplaced);
    }

    Require(RSA_up_ref_nid_postfix(preserved) == 1);
    Require(preserved->references == 2);

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(Ec256Pkcs8Pem);
        LibreSsl3Rsa* slot = preserved;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
        Require(slot == preserved);
        Require(preserved->references == 2);
        ExpectErrorCode(EvpLibrary, EvpRExpectingAnRsaKey);
        ExpectEmptyQueue();
        file.CloseAndVerify();
    }

    {
        ERR_clear_error_nid_postfix();
        const std::vector<unsigned char> malformedBytes = EncodePemRecord("RSA PRIVATE KEY", {0x30, 0x03, 0x02, 0x01, 0x00});
        const std::string malformed(reinterpret_cast<const char*>(malformedBytes.data()), malformedBytes.size());
        GuestPemFileFixture file(malformed);
        LibreSsl3Rsa* slot = preserved;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
        Require(slot == preserved);
        Require(preserved->references == 2);
        ExpectErrorSequence({{ErrorReasonAsn1Library, Asn1FieldMissing}, {RsaLibrary, ErrorReasonRsaLibrary},
            {ErrorReasonAsn1Library, Asn1FieldMissing}, {PemLibrary, ErrorReasonAsn1Library}});
        file.CloseAndVerify();
    }

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file("not a pem key\n");
        LibreSsl3Rsa* slot = preserved;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
        Require(slot == preserved);
        Require(preserved->references == 2);
        ExpectErrorCode(PemLibrary, PemNoStartLine);
        ExpectEmptyQueue();
        file.CloseAndVerify();
    }

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(RsaEncryptedPem);
        LibreSsl3Rsa* slot = preserved;
        int callbackCount = 0;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
                    file.Get(), &slot, CountingRsaFilePasswordCallback, &callbackCount)
            == nullptr);
        Require(callbackCount == 1);
        ExpectErrorSequence({{PemLibrary, PemBadPasswordRead}});
        Require(slot == preserved);
        Require(preserved->references == 2);
        file.CloseAndVerify();
    }

    for (const char* record :
        {RsaPkcs8Aes256Sha256Pem, RsaTraditionalAes256Pem, RsaTraditionalDes3Pem, RsaPkcs8Sha1TripleDesPem}) {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(record);
        PasswordRequest request{TestPassword};
        LibreSsl3Rsa* key = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
            file.Get(), nullptr, SupplyPasswordCallback, &request);
        Require(key != nullptr);
        Require(request.calls == 1 && request.lastWriteFlag == 0);
        CompareRsa(key, rsaReference, true);
        ExpectEmptyQueue();
        file.CloseAndVerify();
        RSA_free_nid_disambig1_nid_postfix(key);
    }

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(DsaPrivateKeyLabelPem);
        LibreSsl3Rsa* slot = preserved;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
        ExpectLastError(PemLibrary, ErrorReasonAsn1Library);
        Require(slot == preserved);
        Require(preserved->references == 2);
        file.CloseAndVerify();
    }

    {
        ERR_clear_error_nid_postfix();
        GuestPemFileFixture file(Dsa1024TraditionalPem);
        LibreSsl3Rsa* slot = preserved;
        Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
        ExpectErrorSequence({{EvpErrors, EvpExpectingAnRsaKey}});
        Require(slot == preserved);
        Require(preserved->references == 2);
        file.CloseAndVerify();
    }

    {
        LibreSsl3Rsa* slot = preserved;
        bool threw = false;
        try {
            PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(nullptr, &slot, nullptr, nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Require(slot == preserved);
        Require(preserved->references == 2);
    }

    RSA_free_nid_disambig1_nid_postfix(preserved);
    RSA_free_nid_disambig1_nid_postfix(preserved);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReferencePkey);
}

void TestRsaPrivateFileSequential() {
    constexpr int EvpLibrary = 6;
    constexpr int EvpRExpectingAnRsaKey = 127;

    auto* rsaReferencePkey = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    const auto* rsaReference = static_cast<const LibreSsl3Rsa*>(rsaReferencePkey->key);

    ERR_clear_error_nid_postfix();
    std::string payload = RsaTraditionalPem;
    payload += Ec256Pkcs8Pem;
    payload += RsaPkcs8Pem;
    payload += "AFTER_SEQUENTIAL_MARKER\n";
    GuestPemFileFixture file(payload);

    LibreSsl3Rsa* slot = nullptr;
    LibreSsl3Rsa* first = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr);
    Require(first != nullptr);
    Require(slot == first);
    Require(first->method == nullptr);
    Require(first->references == 1);
    CompareRsa(first, rsaReference, true);
    Require(RSA_check_key_nid_postfix(first) == 1);
    ExpectEmptyQueue();

    Require(RSA_up_ref_nid_postfix(first) == 1);
    Require(first->references == 2);

    Require(PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr) == nullptr);
    Require(slot == first);
    Require(first->references == 2);
    CompareRsa(first, rsaReference, true);
    ExpectErrorCode(EvpLibrary, EvpRExpectingAnRsaKey);
    ExpectEmptyQueue();

    LibreSsl3Rsa* third = PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(file.Get(), &slot, nullptr, nullptr);
    Require(third != nullptr);
    Require(third != first);
    Require(slot == third);
    Require(third->method == nullptr);
    Require(third->references == 1);
    Require(first->references == 1);
    CompareRsa(third, rsaReference, true);
    Require(RSA_check_key_nid_postfix(third) == 1);
    ExpectEmptyQueue();

    char line[64] {};
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), file.Get()) != nullptr);
    Require(std::strcmp(line, "AFTER_SEQUENTIAL_MARKER\n") == 0);
    file.CloseAndVerify();

    RSA_free_nid_disambig1_nid_postfix(first);
    RSA_free_nid_disambig1_nid_postfix(third);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaReferencePkey);
    ExpectEmptyQueue();
}

const Bytes DsaOidBytes{0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x01};
const Bytes DsaWithSha1OidBytes{0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x03};
const Bytes DsaOldOidBytes{0x2b, 0x0e, 0x03, 0x02, 0x0c};
const Bytes DsaWithShaOidBytes{0x2b, 0x0e, 0x03, 0x02, 0x0d};
const Bytes DsaWithSha1OldOidBytes{0x2b, 0x0e, 0x03, 0x02, 0x1b};
const Bytes P256OidBytes{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
const Bytes DsaZeroVersion{0x02, 0x01, 0x00};

constexpr std::size_t DsaFlagsOffset = 16;
constexpr std::size_t DsaMontgomeryOffset = 24;
constexpr std::size_t DsaReferencesOffset = 32;
constexpr std::size_t DsaExtraDataOffset = 40;
constexpr std::size_t DsaMethodOffset = 48;

struct DsaNumbers {
    Bytes p;
    Bytes q;
    Bytes g;
    Bytes pub;
    Bytes priv;
};

Bytes FromHex(const char* text) {
    std::string digits(text);
    if (digits.size() % 2 != 0) digits.insert(digits.begin(), '0');
    Bytes out;
    for (std::size_t index = 0; index < digits.size(); index += 2)
        out.push_back(static_cast<unsigned char>(std::stoul(digits.substr(index, 2), nullptr, 16)));
    return out;
}

Bytes Trimmed(Bytes value) {
    std::size_t zeros = 0;
    while (zeros < value.size() && value[zeros] == 0) ++zeros;
    value.erase(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(zeros));
    return value;
}

Bytes Sum(const Bytes& first, const Bytes& second) {
    Bytes out;
    unsigned int carry = 0;
    for (std::size_t index = 0; index < first.size() || index < second.size() || carry != 0; ++index) {
        unsigned int total = carry;
        if (index < first.size()) total += first[first.size() - 1 - index];
        if (index < second.size()) total += second[second.size() - 1 - index];
        out.insert(out.begin(), static_cast<unsigned char>(total & 0xff));
        carry = total >> 8;
    }
    return Trimmed(out);
}

Bytes Negated(const Bytes& magnitude) {
    Bytes out = magnitude;
    unsigned int carry = 1;
    for (std::size_t index = out.size(); index-- > 0;) {
        const unsigned int total = static_cast<unsigned int>(out[index] ^ 0xff) + carry;
        out[index] = static_cast<unsigned char>(total & 0xff);
        carry = total >> 8;
    }
    return out;
}

Bytes DerUnsigned(const Bytes& magnitude) {
    Bytes content = Trimmed(magnitude);
    if (content.empty() || (content.front() & 0x80) != 0) content.insert(content.begin(), 0x00);
    return Der(0x02, content);
}

DsaNumbers DsaNumbersOf(const char* traditionalPem) {
    const std::vector<Tlv> outer = DerChildren(BodyBytes(BytesOf(traditionalPem)));
    Require(outer.size() == 1 && outer[0].tag == 0x30);
    const std::vector<Tlv> fields = DerChildren(outer[0].content);
    Require(fields.size() == 6);
    for (const Tlv& field : fields) Require(field.tag == 0x02);
    Require(fields[0].content.size() == 1 && fields[0].content[0] == 0);
    return {Trimmed(fields[1].content), Trimmed(fields[2].content), Trimmed(fields[3].content),
        Trimmed(fields[4].content), Trimmed(fields[5].content)};
}

DsaNumbers DsaCompositeNumbers() {
    return {Trimmed(FromHex(DsaCompositePrimeHex)), Trimmed(FromHex(DsaCompositeSubprimeHex)),
        Trimmed(FromHex(DsaCompositeGeneratorHex)), Trimmed(FromHex(DsaCompositePublicHex)),
        Trimmed(FromHex(DsaCompositePrivateHex))};
}

Bytes DsaParametersDer(const DsaNumbers& numbers) {
    return Der(0x30, Join({DerUnsigned(numbers.p), DerUnsigned(numbers.q), DerUnsigned(numbers.g)}));
}

Bytes DsaTraditionalDer(const Bytes& version, const DsaNumbers& numbers) {
    return Der(0x30, Join({version, DerUnsigned(numbers.p), DerUnsigned(numbers.q), DerUnsigned(numbers.g),
        DerUnsigned(numbers.pub), DerUnsigned(numbers.priv)}));
}

Bytes DsaPkcs8Der(const Bytes& oid, const Bytes& parameter, const Bytes& key) {
    return Der(0x30, Join({DsaZeroVersion, Der(0x30, Join({Der(0x06, oid), parameter})), Der(0x04, key)}));
}

Bytes DsaLabelPem(const Bytes& der) {
    return EncodePemRecord("DSA PRIVATE KEY", der);
}

Bytes DsaPkcs8Pem(const Bytes& der) {
    return EncodePemRecord("PRIVATE KEY", der);
}

Bytes BignumMagnitude(const void* number, std::source_location location = std::source_location::current()) {
    Require(number != nullptr, location);
    const auto* value = static_cast<const Bignum*>(number);
    Require(BN_is_negative_nid_postfix(value) == 0, location);
    const int bits = BN_num_bits_nid_postfix(value);
    Require(bits >= 0, location);
    Bytes bytes(static_cast<std::size_t>((bits + 7) / 8));
    if (!bytes.empty())
        Require(BN_bn2bin_nid_postfix(value, bytes.data()) == static_cast<int>(bytes.size()), location);
    return bytes;
}

LibreSsl3Dsa* DsaOf(LibreSsl3EvpPkey* key, std::source_location location = std::source_location::current()) {
    Require(key != nullptr && key->type == DsaType && key->method == nullptr, location);
    Require(EVP_PKEY_base_id_nid_postfix(key) == DsaType, location);
    auto* dsa = static_cast<LibreSsl3Dsa*>(EVP_PKEY_get0_DSA_nid_postfix(key));
    Require(dsa != nullptr && dsa == key->key, location);
    return dsa;
}

void ExpectDsaNumbers(const LibreSsl3Dsa* dsa, const DsaNumbers& expected,
    std::source_location location = std::source_location::current()) {
    const void* p = nullptr;
    const void* q = nullptr;
    const void* g = nullptr;
    const void* publicKey = nullptr;
    const void* privateKey = nullptr;
    DSA_get0_pqg_nid_postfix(dsa, &p, &q, &g);
    DSA_get0_key_nid_postfix(dsa, &publicKey, &privateKey);
    Require(BignumMagnitude(p, location) == expected.p, location);
    Require(BignumMagnitude(q, location) == expected.q, location);
    Require(BignumMagnitude(g, location) == expected.g, location);
    Require(BignumMagnitude(publicKey, location) == expected.pub, location);
    Require(BignumMagnitude(privateKey, location) == expected.priv, location);
}

void ExpectDsaKey(LibreSsl3EvpPkey* key, const DsaNumbers& expected, std::int64_t version,
    std::source_location location = std::source_location::current()) {
    const LibreSsl3Dsa* dsa = DsaOf(key, location);
    ExpectDsaNumbers(dsa, expected, location);
    Require(dsa->pad == 0 && dsa->version == version, location);
    std::array<unsigned char, sizeof(LibreSsl3Dsa::rest)> rest{};
    rest[DsaFlagsOffset] = 1;
    rest[DsaReferencesOffset] = 1;
    Require(std::memcmp(dsa->rest, rest.data(), rest.size()) == 0, location);
}

LibreSsl3EvpPkey* ReadPlainKey(const Bytes& pem) {
    ERR_clear_error_nid_postfix();
    return ReadPrivateKey(pem, nullptr, nullptr, nullptr);
}

void ExpectDsaRead(const Bytes& pem, const DsaNumbers& expected, std::int64_t version,
    std::source_location location = std::source_location::current()) {
    auto* key = ReadPlainKey(pem);
    ExpectDsaKey(key, expected, version, location);
    ExpectEmptyQueue(location);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void ExpectDsaFailure(const Bytes& pem, const ErrorList& errors,
    std::source_location location = std::source_location::current()) {
    Require(ReadPlainKey(pem) == nullptr, location);
    ExpectErrorSequence(errors, location);
}

ErrorList DsaTraditionalFailure(ErrorList errors) {
    errors.push_back({Asn1Errors, Asn1WrongTag});
    errors.push_back({Asn1Errors, Asn1NestedAsn1Error});
    errors.push_back({Asn1Errors, Asn1NestedAsn1Error});
    errors.push_back({PemLibrary, ErrorReasonAsn1Library});
    return errors;
}

ErrorList DsaStructureFailure(ErrorList errors) {
    errors.push_back({DsaLibrary, ErrorReasonDsaLibrary});
    return DsaTraditionalFailure(errors);
}

ErrorList DsaPkcs8Failure(ErrorList errors) {
    errors.push_back({EvpErrors, EvpPrivateKeyDecodeError});
    errors.push_back({PemLibrary, ErrorReasonAsn1Library});
    return errors;
}

Bytes Pkcs8InfoDer(const LibreSsl3Pkcs8PrivKeyInfo* info) {
    const int length = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(info, nullptr);
    Require(length > 0);
    Bytes der(static_cast<std::size_t>(length));
    unsigned char* cursor = der.data();
    Require(i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(info, &cursor) == length);
    Require(cursor == der.data() + der.size());
    return der;
}

Bytes Pkcs8DerOfKey(const LibreSsl3EvpPkey* key) {
    auto* info = EVP_PKEY2PKCS8_nid_postfix(key);
    Require(info != nullptr);
    const Bytes der = Pkcs8InfoDer(info);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(info);
    return der;
}

void TestDsaPrivateFormats(const char* traditionalText, const char* pkcs8Text, const char* subprimeHex,
    const char* privateHex, std::size_t primeBytes) {
    const DsaNumbers numbers = DsaNumbersOf(traditionalText);
    Require(numbers.q == FromHex(subprimeHex) && numbers.priv == FromHex(privateHex));
    Require(numbers.p.size() == primeBytes && (numbers.p.front() & 0x80) != 0);
    const Bytes traditionalDer = BodyBytes(BytesOf(traditionalText));
    const Bytes pkcs8Der = BodyBytes(BytesOf(pkcs8Text));
    Require(DsaTraditionalDer(DsaZeroVersion, numbers) == traditionalDer);
    Require(DsaPkcs8Der(DsaOidBytes, DsaParametersDer(numbers), DerUnsigned(numbers.priv)) == pkcs8Der);

    auto* traditionalKey = ReadPlainKey(BytesOf(traditionalText));
    ExpectDsaKey(traditionalKey, numbers, 0);
    ExpectEmptyQueue();
    auto* pkcs8Key = ReadPlainKey(BytesOf(pkcs8Text));
    ExpectDsaKey(pkcs8Key, numbers, 0);
    ExpectEmptyQueue();

    Require(WrittenPrivateBytes(traditionalKey, nullptr) == BytesOf(pkcs8Text));
    Require(WrittenPrivateBytes(pkcs8Key, nullptr) == BytesOf(pkcs8Text));
    Require(Pkcs8DerOfKey(traditionalKey) == pkcs8Der);
    ExpectEmptyQueue();

    const unsigned char* cursor = pkcs8Der.data();
    auto* info = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(pkcs8Der.size()));
    Require(info != nullptr && cursor == pkcs8Der.data() + pkcs8Der.size());
    Require(Pkcs8InfoDer(info) == pkcs8Der);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(info);

    ExpectDsaRead(DsaLabelPem(Join({traditionalDer, Bytes(3, 0x00)})), numbers, 0);

    auto* fallbackKey = ReadPlainKey(DsaLabelPem(pkcs8Der));
    ExpectDsaKey(fallbackKey, numbers, 0);
    ExpectErrorSequence({{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error},
        {Asn1Errors, Asn1NestedAsn1Error}, {DsaLibrary, ErrorReasonDsaLibrary}});

    EVP_PKEY_free_nid_disambig1_nid_postfix(fallbackKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkcs8Key);
    EVP_PKEY_free_nid_disambig1_nid_postfix(traditionalKey);
}

void TestDsaEncryptedRead() {
    const DsaNumbers numbers = DsaNumbersOf(Dsa1024TraditionalPem);
    for (const char* record : {Dsa1024TraditionalAes256Pem, Dsa1024TraditionalDes3Pem, Dsa1024Pkcs8Aes128Pem,
             Dsa1024Pkcs8Sha1TripleDesPem}) {
        auto* key = ReadWithPassword(BytesOf(record), TestPassword);
        ExpectDsaKey(key, numbers, 0);
        ExpectEmptyQueue();
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    }
}

void TestDsaTraditionalVersions() {
    const DsaNumbers numbers = DsaNumbersOf(Dsa1024TraditionalPem);
    const std::vector<std::pair<Bytes, std::int64_t>> versions{
        {{0x02, 0x01, 0x01}, 1},
        {{0x02, 0x01, 0xff}, -1},
        {{0x02, 0x00}, 0},
        {{0x02, 0x04, 0x7f, 0xff, 0xff, 0xfe}, 0x7ffffffe},
        {{0x02, 0x05, 0x00, 0x80, 0x00, 0x00, 0x00}, 0x80000000LL},
        {{0x02, 0x08, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, INT64_MAX},
        {{0x02, 0x08, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, INT64_MIN},
    };
    for (const auto& [version, expected] : versions)
        ExpectDsaRead(DsaLabelPem(DsaTraditionalDer(version, numbers)), numbers, expected);

    ExpectDsaFailure(DsaLabelPem(DsaTraditionalDer({0x02, 0x04, 0x7f, 0xff, 0xff, 0xff}, numbers)),
        DsaStructureFailure({{Asn1Errors, Asn1IntegerTooLargeForLong}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(
        DsaLabelPem(DsaTraditionalDer({0x02, 0x09, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, numbers)),
        DsaStructureFailure({{Asn1Errors, Asn1TooLarge}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(DsaLabelPem(DsaTraditionalDer({0x02, 0x02, 0x00, 0x01}, numbers)),
        {{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error}, {DsaLibrary, ErrorReasonDsaLibrary},
            {Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error},
            {PemLibrary, ErrorReasonAsn1Library}});
}

void TestDsaTraditionalChecks() {
    const DsaNumbers good = DsaNumbersOf(Dsa1024TraditionalPem);
    Require(good.q.size() == 20 && good.q.front() == 0xff && (good.q.back() & 1) == 1 && (good.p.back() & 1) == 1);
    const auto expect = [](const DsaNumbers& numbers, int reason,
                            std::source_location location = std::source_location::current()) {
        ExpectDsaFailure(DsaLabelPem(DsaTraditionalDer(DsaZeroVersion, numbers)),
            DsaTraditionalFailure({{DsaLibrary, reason}}), location);
    };

    DsaNumbers numbers = good;
    numbers.p.back() &= 0xfe;
    expect(numbers, DsaInvalidParameters);
    numbers = good;
    numbers.q.back() &= 0xfe;
    expect(numbers, DsaInvalidParameters);
    numbers = good;
    numbers.g = {0x01};
    expect(numbers, DsaInvalidParameters);
    numbers.g = {};
    expect(numbers, DsaInvalidParameters);
    numbers.g = Sum(good.g, good.p);
    expect(numbers, DsaInvalidParameters);
    numbers.g = good.p;
    expect(numbers, DsaInvalidParameters);

    numbers = good;
    numbers.q.front() = 0x7f;
    expect(numbers, DsaBadQValue);
    numbers.q = good.q;
    numbers.q.insert(numbers.q.begin(), 0x01);
    expect(numbers, DsaBadQValue);

    numbers = good;
    numbers.p = Bytes(1250, 0xff);
    expect(numbers, DsaBadQValue);
    numbers.p.insert(numbers.p.begin(), 0x01);
    expect(numbers, DsaModulusTooLarge);
    numbers.p = Bytes(64, 0xff);
    numbers.g = {0x02};
    numbers.pub = {0x02};
    expect(numbers, DsaBadQValue);
    numbers.p.front() = 0x7f;
    expect(numbers, DsaInvalidParameters);

    numbers = good;
    numbers.pub = {0x01};
    expect(numbers, DsaInvalidParameters);
    numbers.pub = {};
    expect(numbers, DsaInvalidParameters);
    numbers.pub = good.p;
    expect(numbers, DsaInvalidParameters);
    numbers = good;
    numbers.priv = {};
    expect(numbers, DsaInvalidParameters);
    numbers.priv = good.q;
    expect(numbers, DsaInvalidParameters);

    numbers = good;
    numbers.q = Sum(good.q, {0x02});
    expect(numbers, DsaBadQValue);
    numbers = good;
    numbers.g = Sum(good.g, {0x01});
    expect(numbers, DsaInvalidParameters);
    expect(DsaCompositeNumbers(), DsaBadQValue);

    ExpectDsaFailure(DsaLabelPem(Der(0x30, Join({DsaZeroVersion, Der(0x02, good.p), DerUnsigned(good.q),
                         DerUnsigned(good.g), DerUnsigned(good.pub), DerUnsigned(good.priv)}))),
        DsaTraditionalFailure({{DsaLibrary, DsaInvalidParameters}}));
    const Bytes negatedSubprime = Negated(good.q);
    Require(negatedSubprime.size() == good.q.size() && (negatedSubprime.front() & 0x80) == 0);
    ExpectDsaFailure(DsaLabelPem(Der(0x30, Join({DsaZeroVersion, DerUnsigned(good.p),
                         Der(0x02, Join({Bytes{0xff}, negatedSubprime})), DerUnsigned(good.g),
                         DerUnsigned(good.pub), DerUnsigned(good.priv)}))),
        DsaTraditionalFailure({{DsaLibrary, DsaBadQValue}}));
    ExpectDsaFailure(DsaLabelPem(Der(0x30, Join({DsaZeroVersion, DerUnsigned(good.p), DerUnsigned(good.q),
                         DerUnsigned(good.g), DerUnsigned(good.pub), Bytes{0x02, 0x01, 0xff}}))),
        DsaTraditionalFailure({{DsaLibrary, DsaInvalidParameters}}));

    numbers = good;
    numbers.priv = {0x01};
    ExpectDsaRead(DsaLabelPem(DsaTraditionalDer(DsaZeroVersion, numbers)), numbers, 0);
    numbers = good;
    numbers.pub = {0x02};
    ExpectDsaRead(DsaLabelPem(DsaTraditionalDer(DsaZeroVersion, numbers)), numbers, 0);
}

void TestDsaTraditionalStructure() {
    const DsaNumbers good = DsaNumbersOf(Dsa1024TraditionalPem);
    const Bytes p = DerUnsigned(good.p);
    const Bytes q = DerUnsigned(good.q);
    const Bytes g = DerUnsigned(good.g);
    const Bytes pub = DerUnsigned(good.pub);
    const Bytes priv = DerUnsigned(good.priv);
    const auto record = [](std::initializer_list<Bytes> fields) { return DsaLabelPem(Der(0x30, Join(fields))); };

    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, pub}), DsaStructureFailure({{Asn1Errors, Asn1FieldMissing}}));
    ExpectDsaFailure(record({DsaZeroVersion}),
        {{Asn1Errors, Asn1FieldMissing}, {DsaLibrary, ErrorReasonDsaLibrary}, {Asn1Errors, Asn1FieldMissing},
            {PemLibrary, ErrorReasonAsn1Library}});
    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, pub, priv, priv}),
        DsaStructureFailure({{Asn1Errors, Asn1SequenceLengthMismatch}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, Der(0x04, good.q), g, pub, priv}),
        DsaStructureFailure({{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error},
            {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, q, Der(0x22, good.g), pub, priv}),
        DsaStructureFailure({{Asn1Errors, Asn1TypeNotPrimitive}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, Bytes{0x02, 0x00}, priv}),
        DsaStructureFailure({{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, pub, Bytes{0x02, 0x02, 0x00, 0x01}}),
        DsaStructureFailure({{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, pub, Bytes{0x02, 0x02, 0xff, 0x80}}),
        DsaStructureFailure({{Asn1Errors, Asn1LengthMismatch}, {Asn1Errors, Asn1NestedAsn1Error}}));
    ExpectDsaFailure(record({DsaZeroVersion, p, q, g, pub, Bytes{0x02, 0x05, 0x01}}),
        DsaStructureFailure({{Asn1Errors, Asn1TooLong}, {Asn1Errors, Asn1NestedAsn1Error},
            {Asn1Errors, Asn1NestedAsn1Error}}));

    ExpectDsaFailure(DsaLabelPem({0x10, 0x03, 0x02, 0x01, 0x00}),
        {{Asn1Errors, Asn1SequenceNotConstructed}, {DsaLibrary, ErrorReasonDsaLibrary},
            {Asn1Errors, Asn1SequenceNotConstructed}, {PemLibrary, ErrorReasonAsn1Library}});
    ExpectDsaFailure(DsaLabelPem({0x30, 0x00}),
        {{Asn1Errors, Asn1FieldMissing}, {DsaLibrary, ErrorReasonDsaLibrary}, {Asn1Errors, Asn1FieldMissing},
            {PemLibrary, ErrorReasonAsn1Library}});

    ERR_clear_error_nid_postfix();
    Require(ReadPrivateKey(BytesOf(DsaPrivateKeyLabelPem), nullptr, nullptr, nullptr) == nullptr);
    const ErrorList garbage = DrainErrors();
    Require(garbage.size() >= 4);
    Require(garbage[0] == std::pair<int, int>(Asn1Errors, Asn1WrongTag));
    Require(garbage[1] == std::pair<int, int>(Asn1Errors, Asn1NestedAsn1Error));
    Require(garbage[2] == std::pair<int, int>(DsaLibrary, ErrorReasonDsaLibrary));
    Require(garbage.back() == std::pair<int, int>(PemLibrary, ErrorReasonAsn1Library));
}

void TestDsaPkcs8Checks() {
    const DsaNumbers good = DsaNumbersOf(Dsa1024TraditionalPem);
    const Bytes parameters = DsaParametersDer(good);
    const Bytes key = DerUnsigned(good.priv);
    const auto record = [](const Bytes& oid, const Bytes& parameter, const Bytes& privateKey) {
        return DsaPkcs8Pem(DsaPkcs8Der(oid, parameter, privateKey));
    };

    for (const Bytes& oid : {DsaOidBytes, DsaWithSha1OidBytes, DsaOldOidBytes, DsaWithShaOidBytes,
             DsaWithSha1OldOidBytes})
        ExpectDsaRead(record(oid, parameters, key), good, 0);

    const DsaNumbers composite = DsaCompositeNumbers();
    ExpectDsaRead(record(DsaOidBytes, DsaParametersDer(composite), DerUnsigned(composite.priv)), composite, 0);
    ExpectDsaRead(record(DsaOidBytes, parameters, Join({key, Bytes{0x05, 0x00}})), good, 0);

    for (const Bytes& parameter : {Bytes{}, Bytes{0x05, 0x00}, Der(0x06, P256OidBytes)})
        ExpectDsaFailure(record(DsaOidBytes, parameter, key),
            DsaPkcs8Failure({{DsaLibrary, DsaParameterEncodingError}}));

    ExpectDsaFailure(record(DsaOidBytes, Der(0x30, Join({DerUnsigned(good.p), DerUnsigned(good.q)})), key),
        DsaPkcs8Failure({{Asn1Errors, Asn1FieldMissing}, {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes,
                         Der(0x30, Join({DerUnsigned(good.p), DerUnsigned(good.q), DerUnsigned(good.g),
                                    DerUnsigned(good.g)})),
                         key),
        DsaPkcs8Failure({{Asn1Errors, Asn1SequenceLengthMismatch}, {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes,
                         Der(0x30, Join({DerUnsigned(good.p), DerUnsigned(good.q), Der(0x04, good.g)})), key),
        DsaPkcs8Failure({{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error},
            {Asn1Errors, Asn1NestedAsn1Error}, {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes, parameters, Der(0x04, good.priv)),
        DsaPkcs8Failure({{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error},
            {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes, parameters, Der(0x22, good.priv)),
        DsaPkcs8Failure({{Asn1Errors, Asn1TypeNotPrimitive}, {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes, parameters, {0x02, 0x02, 0x00, 0x05}),
        DsaPkcs8Failure({{Asn1Errors, Asn1LengthMismatch}, {DsaLibrary, DsaDecodeError}}));
    ExpectDsaFailure(record(DsaOidBytes, parameters, {0x02, 0x00}),
        DsaPkcs8Failure({{Asn1Errors, Asn1LengthMismatch}, {DsaLibrary, DsaDecodeError}}));

    for (const Bytes& privateKey : {Bytes{0x02, 0x01, 0x00}, Bytes{0x02, 0x01, 0xff}, DerUnsigned(good.q)})
        ExpectDsaFailure(record(DsaOidBytes, parameters, privateKey),
            DsaPkcs8Failure({{DsaLibrary, DsaInvalidParameters}}));

    DsaNumbers numbers = good;
    numbers.q.front() = 0x7f;
    ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
        DsaPkcs8Failure({{DsaLibrary, DsaBadQValue}}));
    numbers = good;
    numbers.p.back() &= 0xfe;
    ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
        DsaPkcs8Failure({{DsaLibrary, DsaInvalidParameters}}));
    numbers = good;
    numbers.p = Bytes(1251, 0xff);
    ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
        DsaPkcs8Failure({{DsaLibrary, DsaModulusTooLarge}}));
    numbers = good;
    numbers.g = good.p;
    ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
        DsaPkcs8Failure({{DsaLibrary, DsaInvalidParameters}}));
    for (const std::size_t subprimeBytes : {std::size_t{28}, std::size_t{32}}) {
        numbers = good;
        numbers.q = Bytes(subprimeBytes, 0xff);
        ExpectDsaRead(record(DsaOidBytes, DsaParametersDer(numbers), key), numbers, 0);
        numbers.q.front() = 0x7f;
        ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
            DsaPkcs8Failure({{DsaLibrary, DsaBadQValue}}));
        numbers.q = Bytes(subprimeBytes, 0xff);
        numbers.q.insert(numbers.q.begin(), 0x01);
        ExpectDsaFailure(record(DsaOidBytes, DsaParametersDer(numbers), key),
            DsaPkcs8Failure({{DsaLibrary, DsaBadQValue}}));
    }
}

void TestDsaSlots() {
    const DsaNumbers small = DsaNumbersOf(Dsa1024TraditionalPem);
    const DsaNumbers large = DsaNumbersOf(Dsa2048TraditionalPem);

    LibreSsl3EvpPkey* slot = ReadPlainKey(BytesOf(Dsa1024Pkcs8Pem));
    LibreSsl3EvpPkey* previous = slot;
    ERR_clear_error_nid_postfix();
    auto* traditional = ReadPrivateKey(BytesOf(Dsa2048TraditionalPem), &slot, nullptr, nullptr);
    Require(traditional != nullptr && slot == traditional && traditional != previous);
    ExpectDsaKey(slot, large, 0);
    ExpectEmptyQueue();

    previous = slot;
    auto* pkcs8 = ReadPrivateKey(BytesOf(Dsa1024Pkcs8Pem), &slot, nullptr, nullptr);
    Require(pkcs8 != nullptr && slot == pkcs8 && pkcs8 != previous);
    ExpectDsaKey(slot, small, 0);
    ExpectEmptyQueue();

    DsaNumbers broken = small;
    broken.q = Sum(small.q, {0x02});
    Require(ReadPrivateKey(DsaLabelPem(DsaTraditionalDer(DsaZeroVersion, broken)), &slot, nullptr, nullptr)
        == nullptr);
    ExpectErrorSequence(DsaTraditionalFailure({{DsaLibrary, DsaBadQValue}}));
    Require(slot == pkcs8);
    ExpectDsaKey(slot, small, 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(slot);
}

void WriteGuestInt(unsigned char* target, std::int32_t value) {
    std::memcpy(target, &value, sizeof(value));
}

void TestDsaObject() {
    const DsaNumbers numbers = DsaNumbersOf(Dsa1024TraditionalPem);

    auto* first = ReadPlainKey(BytesOf(Dsa1024TraditionalPem));
    LibreSsl3Dsa* dsa = DsaOf(first);
    WriteGuestInt(dsa->rest + DsaReferencesOffset, 2);
    EVP_PKEY_free_nid_disambig1_nid_postfix(first);
    ExpectDsaNumbers(dsa, numbers);
    auto* second = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(second != nullptr && EVP_PKEY_assign_nid_postfix(second, DsaType, dsa) == 1);
    ExpectDsaKey(second, numbers, 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(second);

    for (const std::size_t offset : {DsaMontgomeryOffset, DsaExtraDataOffset, DsaMethodOffset}) {
        auto* key = ReadPlainKey(BytesOf(Dsa1024TraditionalPem));
        LibreSsl3Dsa* object = DsaOf(key);
        object->rest[offset] = 1;
        ExpectUnimplemented([key] { EVP_PKEY_free_nid_disambig1_nid_postfix(key); });
        Require(key->key == object);
        ExpectDsaNumbers(object, numbers);
        object->rest[offset] = 0;
        WriteGuestInt(object->rest + DsaReferencesOffset, 1);
        key->references = 1;
        ExpectDsaKey(key, numbers, 0);
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    }

    auto* rsaKey = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    LibreSsl3Rsa* rsa = EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(rsaKey);
    Require(rsa != nullptr);
    auto* replaced = ReadPlainKey(BytesOf(Dsa1024TraditionalPem));
    LibreSsl3Dsa* shared = DsaOf(replaced);
    WriteGuestInt(shared->rest + DsaReferencesOffset, 2);
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(replaced, rsa) == 1);
    Require(replaced->type == RsaType && replaced->key == rsa);
    std::int32_t references = 0;
    std::memcpy(&references, shared->rest + DsaReferencesOffset, sizeof(references));
    Require(references == 1);
    ExpectDsaNumbers(shared, numbers);
    auto* holder = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(holder != nullptr && EVP_PKEY_assign_nid_postfix(holder, DsaType, shared) == 1);
    EVP_PKEY_free_nid_disambig1_nid_postfix(holder);
    EVP_PKEY_free_nid_disambig1_nid_postfix(replaced);
    RSA_free_nid_disambig1_nid_postfix(rsa);
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsaKey);
    ExpectEmptyQueue();
}

void TestDsaEncoding() {
    const DsaNumbers numbers = DsaNumbersOf(Dsa1024TraditionalPem);
    auto* key = ReadPlainKey(BytesOf(Dsa1024TraditionalPem));
    LibreSsl3Dsa* dsa = DsaOf(key);
    const void* const generator = dsa->g;
    const void* const privateKey = dsa->privKey;
    const Bytes p = DerUnsigned(numbers.p);
    const Bytes q = DerUnsigned(numbers.q);
    const Bytes priv = DerUnsigned(numbers.priv);

    struct Encoding {
        std::uint64_t limb;
        std::int32_t top;
        std::int32_t negative;
        Bytes integer;
    };
    const std::vector<Encoding> encodings{
        {0x81, 1, 1, {0x02, 0x02, 0xff, 0x7f}},
        {0x80, 1, 1, {0x02, 0x01, 0x80}},
        {0x80, 1, 0, {0x02, 0x02, 0x00, 0x80}},
        {0x7f, 1, 0, {0x02, 0x01, 0x7f}},
        {0x01, 1, 1, {0x02, 0x01, 0xff}},
        {0x100, 1, 1, {0x02, 0x02, 0xff, 0x00}},
        {0x8000000000000000ULL, 1, 0, {0x02, 0x09, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0x8000000000000000ULL, 1, 1, {0x02, 0x08, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0x55, 0, 0, {0x02, 0x01, 0x00}},
        {0x55, 0, 1, {0x02, 0x01, 0x00}},
    };
    for (const Encoding& encoding : encodings) {
        std::uint64_t limb = encoding.limb;
        LibreSsl3Bignum crafted{&limb, encoding.top, 1, encoding.negative, 0};
        dsa->g = &crafted;
        Require(Pkcs8DerOfKey(key) == DsaPkcs8Der(DsaOidBytes, Der(0x30, Join({p, q, encoding.integer})), priv));
        dsa->privKey = &crafted;
        dsa->g = generator;
        Require(Pkcs8DerOfKey(key) == DsaPkcs8Der(DsaOidBytes, DsaParametersDer(numbers), encoding.integer));
        dsa->privKey = privateKey;
    }

    dsa->g = nullptr;
    Require(Pkcs8DerOfKey(key) == DsaPkcs8Der(DsaOidBytes, Der(0x30, Join({p, q})), priv));
    dsa->g = generator;
    dsa->privKey = nullptr;
    ExpectInvalidArgument([key] { EVP_PKEY2PKCS8_nid_postfix(key); });
    dsa->privKey = privateKey;
    Require(Pkcs8DerOfKey(key) == BodyBytes(BytesOf(Dsa1024Pkcs8Pem)));
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

constexpr char Dsa1024PublicPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBtzCCASwGByqGSM44BAEwggEfAoGBAOcs/xi7X5F5t//ZlFbAvu4WX7uRF8Pm\n"
    "7FEGBb5a7UIxxmbUrBMMmnHgLf6yW1PnlvTOhZBXnfB1NpI9DnwDOT5Heoj+jEP5\n"
    "gv3Qog9eA2dfdkfcErVxIfsyHVNx/eb9n/WOls3WlLN5pE+V1G5uEm/7MY+08NcB\n"
    "dH6FePNXBFt5AhUA/3ug7v7+mTR9TRvhUZZDjgc+h7ECgYEAmmFW6ol7V+WTBclf\n"
    "i9JjwAf6JeS7WHaQHK7rgjbpCaJoS9tDoRNwqomPwomVDCoYx4oh7s8oddADqTin\n"
    "i978bHQfr67i9hvNOCswK/h3ra9mQYpbAG7nqMBto7TvVR5yXgmQXp7J/qmLk1mg\n"
    "s5POl1RGdtpJNhEdaVUOX7zH9poDgYQAAoGADzHe1puObV00zBbFAUbqCXmBmLT+\n"
    "5EAgKsLtgpbfrfyKVt2+fnN7hoM8wwmrOJ4o0JsCktSkArh6vQCXkIuZq945K79/\n"
    "IXW930MsJvYAYqcADXks3vFGiEaCsjBrgAIWO4rZ4dFGXXf+dcGOpuYXA42AOSEY\n"
    "NjxfCaSDpCkXG6M=\n"
    "-----END PUBLIC KEY-----\n";

constexpr char Dsa2048PublicPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIDRjCCAjkGByqGSM44BAEwggIsAoIBAQDH/igz3Ag85SjHHRXCga1HkOvpKiRR\n"
    "R+uVbNYUZrxvA7kIkswurZpo1OEKQiN1pHUE4kvInrwZJGuF6oz1uRlhMqIkuKrW\n"
    "eOOGYwP9PZZACzbZRyQ9HCkp46loeT4F88890j2pbmOFAP7hJrSZWrOOJ2i+KU0Q\n"
    "NgPMknVC8bQvMraEsiOHZpE0d0HS0IY2aMsgf7Of38SCoyjkGaSqfm8VZ8J5lBpi\n"
    "Wqom8tfI1xXivh9InCEg8OaiR3gUPXGyDboa34iS/R47O4fFnhjG8ixTnRNxHlD/\n"
    "BHlNnTvN+t+AxYJWfdmu+1zq+L/eB8XBybtO7K7h6LT+mhyNe0gxUqu1AiEA2XbQ\n"
    "YzQzGOpsrSf7lmfOyYXp/YB0oZUY2ODn0COYfCECggEAWyKMmLZikAtWE+X//L7I\n"
    "NzvWT8iV2vIFrN4mf/RdOk2d0U24g2KIewdosezDkSmXX8BAZtZQMOm+vP4UT7mF\n"
    "DJ9F48Fd7ccVyDT0fV8pMNHUr1dYmHImtNSKbT2iF4toUA02lZ69+1Dpltegpsf5\n"
    "VYVItCPNeDllIl9WJ0GG5LqaYNxfWo01bW+9JVCsrPl9zC/VkNETWuzCes39A1zk\n"
    "HWWFMZlaDmQRatIhYoKo35x3OVsyYS4LXXvcmuIgxB1WFZZf4JeVQTv3tiIMJWJ4\n"
    "9/bSzIhTbmuAg17kvhwTsUF9tt7nJx/1v5a9Rrd6rsU+1u8uDP5zA4I/YcFmXt+8\n"
    "fAOCAQUAAoIBAHRmmuKNMzke+N7HYRu1jnv6HQ/BhAmaU/lom6zeGiwBnb5qglu+\n"
    "fbh8YFyYzZ7DWtJZnSIGjTzsuPvuVDu4yOXxxPhGoa3erwi2LtOaa6xuN6R5GSfV\n"
    "SIXyOthXA59JqFbypuVamdIs5df2Qd15d1yu1XrUocCZbZmdcZho98ivgWMNARig\n"
    "3uPGv3YmebWffceo1hU+OzXVejpVFZGXtGYwnBN9NXWTz3opVPXb+HnkPbsXfs8z\n"
    "PQoMCMK3l+eyOkoQ3my0XOm2cA6Mv4VcWr719qoZQHJ5a8SP0NrswoRzIlBZPC0T\n"
    "V1E31RBZ/4P5dqIIxhe7rVoEzpaUY2N6/No=\n"
    "-----END PUBLIC KEY-----\n";

constexpr int X509Errors = 11;
constexpr int X509PublicKeyDecodeError = 125;

Bytes DsaSpkiDer(const Bytes& oid, const Bytes& parameter, const Bytes& key) {
    return Der(0x30, Join({Der(0x30, Join({Der(0x06, oid), parameter})), Der(0x03, Join({Bytes{0x00}, key}))}));
}

LibreSsl3EvpPkey* DecodeSpkiDer(const Bytes& der) {
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = der.data();
    auto* key = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    if (key != nullptr) Require(cursor == der.data() + der.size());
    return key;
}

void ExpectDsaPublic(LibreSsl3EvpPkey* key, const DsaNumbers& expected, bool parameters,
    std::source_location location = std::source_location::current()) {
    const LibreSsl3Dsa* dsa = DsaOf(key, location);
    const void* p = nullptr;
    const void* q = nullptr;
    const void* g = nullptr;
    const void* publicKey = nullptr;
    const void* privateKey = nullptr;
    DSA_get0_pqg_nid_postfix(dsa, &p, &q, &g);
    DSA_get0_key_nid_postfix(dsa, &publicKey, &privateKey);
    if (parameters) {
        Require(BignumMagnitude(p, location) == expected.p, location);
        Require(BignumMagnitude(q, location) == expected.q, location);
        Require(BignumMagnitude(g, location) == expected.g, location);
    } else {
        Require(p == nullptr && q == nullptr && g == nullptr, location);
    }
    Require(BignumMagnitude(publicKey, location) == expected.pub, location);
    Require(privateKey == nullptr, location);
    Require(dsa->pad == 0 && dsa->version == 0, location);
    std::array<unsigned char, sizeof(LibreSsl3Dsa::rest)> rest{};
    rest[DsaFlagsOffset] = 1;
    rest[DsaReferencesOffset] = 1;
    Require(std::memcmp(dsa->rest, rest.data(), rest.size()) == 0, location);
}

void ExpectDsaPublicVector(const char* privatePem, const char* publicPem) {
    const DsaNumbers numbers = DsaNumbersOf(privatePem);
    const Bytes der = BodyBytes(BytesOf(publicPem));
    Require(der == DsaSpkiDer(DsaOidBytes, DsaParametersDer(numbers), DerUnsigned(numbers.pub)));
    auto* key = DecodeSpkiDer(der);
    ExpectDsaPublic(key, numbers, true);
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);

    ERR_clear_error_nid_postfix();
    key = ReadPublicKey(BytesOf(publicPem), nullptr);
    ExpectDsaPublic(key, numbers, true);
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void ExpectDsaPublicFailure(const Bytes& der, ErrorList errors,
    std::source_location location = std::source_location::current()) {
    Require(DecodeSpkiDer(der) == nullptr, location);
    errors.push_back({X509Errors, X509PublicKeyDecodeError});
    ExpectErrorSequence(errors, location);
}

void TestDsaPublicKeys() {
    ExpectDsaPublicVector(Dsa1024TraditionalPem, Dsa1024PublicPem);
    ExpectDsaPublicVector(Dsa2048TraditionalPem, Dsa2048PublicPem);

    const DsaNumbers good = DsaNumbersOf(Dsa1024TraditionalPem);
    const Bytes parameters = DsaParametersDer(good);
    const Bytes publicKey = DerUnsigned(good.pub);
    const Bytes one{0x02, 0x01, 0x01};
    DsaNumbers unit = good;
    unit.pub = Bytes{0x01};

    for (const Bytes& oid : {DsaOidBytes, DsaWithSha1OidBytes, DsaOldOidBytes, DsaWithShaOidBytes,
             DsaWithSha1OldOidBytes}) {
        auto* key = DecodeSpkiDer(DsaSpkiDer(oid, parameters, publicKey));
        ExpectDsaPublic(key, good, true);
        ExpectEmptyQueue();
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    }

    for (const Bytes& parameter : {Bytes{}, Bytes{0x05, 0x00}}) {
        auto* key = DecodeSpkiDer(DsaSpkiDer(DsaOidBytes, parameter, publicKey));
        ExpectDsaPublic(key, good, false);
        ExpectEmptyQueue();
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);

        key = DecodeSpkiDer(DsaSpkiDer(DsaOidBytes, parameter, one));
        ExpectDsaPublic(key, unit, false);
        ExpectEmptyQueue();
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    }

    auto* trailing = DecodeSpkiDer(DsaSpkiDer(DsaOidBytes, parameters, Join({publicKey, Bytes{0x05, 0x00}})));
    ExpectDsaPublic(trailing, good, true);
    ExpectEmptyQueue();
    EVP_PKEY_free_nid_disambig1_nid_postfix(trailing);

    ExpectDsaPublicFailure(DsaSpkiDer(DsaOidBytes, Der(0x06, P256OidBytes), publicKey),
        {{DsaLibrary, DsaParameterEncodingError}});
    ExpectDsaPublicFailure(
        DsaSpkiDer(DsaOidBytes, Der(0x30, Join({DerUnsigned(good.p), DerUnsigned(good.q)})), publicKey),
        {{Asn1Errors, Asn1FieldMissing}, {DsaLibrary, DsaDecodeError}});
    ExpectDsaPublicFailure(DsaSpkiDer(DsaOidBytes, parameters, Der(0x04, good.pub)),
        {{Asn1Errors, Asn1WrongTag}, {Asn1Errors, Asn1NestedAsn1Error}, {DsaLibrary, DsaDecodeError}});
    ExpectDsaPublicFailure(DsaSpkiDer(DsaOidBytes, parameters, one), {{DsaLibrary, DsaInvalidParameters}});
    ExpectDsaPublicFailure(DsaSpkiDer(DsaOidBytes, parameters, DerUnsigned(good.p)),
        {{DsaLibrary, DsaInvalidParameters}});
    DsaNumbers evenSubprime = good;
    evenSubprime.q.back() &= 0xfe;
    ExpectDsaPublicFailure(DsaSpkiDer(DsaOidBytes, DsaParametersDer(evenSubprime), publicKey),
        {{DsaLibrary, DsaInvalidParameters}});
}

void TestDsaAutoPrivateKey() {
    for (const char* pem : {Dsa1024TraditionalPem, Dsa2048TraditionalPem}) {
        const DsaNumbers numbers = DsaNumbersOf(pem);
        ERR_clear_error_nid_postfix();
        auto* key = KeyFromDerThroughBio(BodyBytes(BytesOf(pem)));
        ExpectDsaKey(key, numbers, 0);
        ExpectEmptyQueue();
        EVP_PKEY_free_nid_disambig1_nid_postfix(key);
    }

    const DsaNumbers good = DsaNumbersOf(Dsa1024TraditionalPem);
    DsaNumbers evenSubprime = good;
    evenSubprime.q.back() &= 0xfe;
    const Bytes octet = Der(0x04, Bytes{0x01});
    for (const Bytes& der : {DsaTraditionalDer(DsaZeroVersion, evenSubprime),
             Der(0x30, Join({octet, octet, octet, octet, octet, octet}))}) {
        Require(ReadPlainKey(DsaLabelPem(der)) == nullptr);
        ErrorList expected = DrainErrors();
        Require(expected.size() >= 2);
        Require(expected.back() == std::pair<int, int>{PemLibrary, ErrorReasonAsn1Library});
        expected.pop_back();
        Require(expected.front().first == DsaLibrary || expected.front().first == Asn1Errors);

        ERR_clear_error_nid_postfix();
        LibreSsl::Bio* bio = MemoryBio(der);
        Require(d2i_PrivateKey_bio_nid_postfix(bio, nullptr) == nullptr);
        BIO_free_nid_postfix(bio);
        ExpectErrorSequence(expected);
    }
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size == 0 ? 1 : size);
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestRsaPrivateFormats();
    TestRsaPublicKey();
    TestEcPrivateFormats(Secp256r1, Ec256Pkcs8Pem, Ec256TraditionalPem);
    TestEcPrivateFormats(Secp384r1, Ec384Pkcs8Pem, Ec384TraditionalPem);
    TestEcPublicKey(Secp256r1, Ec256PublicPem, Ec256Pkcs8Pem);
    TestEcPublicKey(Secp384r1, Ec384PublicPem, Ec384Pkcs8Pem);
    TestConsecutiveRecords();
    TestTextBeforeBegin();
    TestOutputReplacement();
    TestPasswordCallbackNotCalled();
    TestEncryptedTraditionalRead();
    TestEncryptedTraditionalErrors();
    TestEncryptedPkcs8Read();
    TestEncryptedPkcs8Structure();
    TestEncryptedPkcs8Parameters();
    TestEncryptedWrite();
    TestDesTraditional();
    TestDesPkcs8();
    TestDesPbes2Parameters();
    TestDesPbeParameters();
    TestHeaders();
    TestLabels();
    TestCorruptTraditionalEc();
    TestErrors();
    TestPkcs8InTraditionalLabelFallback();
    TestNormalTraditionalCleanQueue();
    TestTraditionalDerValidationAndContext();
    TestRsaPrivateFile();
    TestRsaPrivateFileSequential();
    TestDsaPrivateFormats(Dsa1024TraditionalPem, Dsa1024Pkcs8Pem, Dsa1024SubprimeHex, Dsa1024PrivateHex, 128);
    TestDsaPrivateFormats(Dsa2048TraditionalPem, Dsa2048Pkcs8Pem, Dsa2048SubprimeHex, Dsa2048PrivateHex, 256);
    TestDsaEncryptedRead();
    TestDsaTraditionalVersions();
    TestDsaTraditionalChecks();
    TestDsaTraditionalStructure();
    TestDsaPkcs8Checks();
    TestDsaSlots();
    TestDsaObject();
    TestDsaEncoding();
    TestDsaPublicKeys();
    TestDsaAutoPrivateKey();
    return 0;
}
