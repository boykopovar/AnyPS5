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
constexpr int Asn1WrongTag = 168;

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

void TestEncryptedRejected() {
    bool thrown = false;
    try {
        ReadPrivateKey(BytesOf(RsaEncryptedPem), nullptr, CountingPasswordCallback, nullptr);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);

    thrown = false;
    try {
        ReadPrivateKey(BytesOf(EncryptedHeaderPem), nullptr, CountingPasswordCallback, nullptr);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);

    thrown = false;
    auto* key = KeyFromPkcs8Der(BodyBytes(BytesOf(RsaPkcs8Pem)));
    const void* cipher = EVP_aes_256_cbc_nid_postfix();
    Require(cipher != nullptr);
    try {
        WrittenPrivateBytes(key, static_cast<const LibreSsl3EvpCipher*>(cipher));
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(passwordCallbackCalls == 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
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

    bool thrown = false;
    try {
        ReadPrivateKey(BytesOf(DsaPrivateKeyLabelPem), nullptr, nullptr, nullptr);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
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
    TestEncryptedRejected();
    TestHeaders();
    TestLabels();
    TestCorruptTraditionalEc();
    TestErrors();
    TestPkcs8InTraditionalLabelFallback();
    TestNormalTraditionalCleanQueue();
    TestTraditionalDerValidationAndContext();
    return 0;
}
