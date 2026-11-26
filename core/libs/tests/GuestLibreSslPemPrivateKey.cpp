#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>

namespace {

constexpr char Aes192Key[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBrjBIBgkqhkiG9w0BBQ0wOzAaBgkqhkiG9w0BBQwwDQQIdCcEFMuNuSgCAQgw
HQYJYIZIAWUDBAEWBBAieM3Bn8ZJ+nbVrVpWRFETBIIBYDqWRzL5y/DTouWfO+9z
t+HA4vtp/1yB3KlDsITU2yCSX28jiaX8OvtMWs5pxPqL4CI0N2UtSTUCI4w8XVhg
cvedUvD1zNECrzIeWqhMVIoDTatMucoxGVdg9cU08O+UunBUIW4PK3tcFHJJmqbV
fXA5IcM/f2I61g558n1yRkvSxSxaNtFZd9uQAqSh8XAHVzwNyPA9g0iSQEZRI2hB
+RlEDKn04l7+t/sJ8q09+CTOaNC8VD4QWTgcN3AmxLVjL+ytVdPF14VvVBCtqRSx
gPniBUvBkJyftmv14CsadIMU8lFN+xy9vh4oVqwo6dEIrTo7PKDb6a15gEIAEpHs
k4T6QUv7sbTjV6HM30jOtV14uLuzR6dGzmyjtKokfIxalWYVLueUOaad2QytY7X8
wVZznkX8vUrVIJ024K7+y78BQmDGtNB3cS4j0xQpN7EFnVkPDclwdTgH58WoBAdH
uyo=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char DesCbcKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBojA8BgkqhkiG9w0BBQ0wLzAaBgkqhkiG9w0BBQwwDQQIrzJZxSaeU+8CAQgw
EQYFKw4DAgcECAqTgDEXscCNBIIBYPo7W0DV/f1tHCaxRZIDwjNsweGZHjCcdi2K
NrONRxJb1CTREvCfDwTGWKQlJiJnDB7L+mXl769eibBGt08s8EWhvjRtfI+SUtF/
fhVuH4c9Df4D8gSIMpgph18bvzepdqHIR2Ye9aG3X14QBnT/IEx4lXTM2wCEPj75
iZtV82ko/QGnwl202LQ3RujTYg4KgTQyQomtnuj/uShXMt0Cn1kCrmarKeNgGe0l
UAwYYEn+Y8Av8fB2ALHtDVsH5BsH8nWg1rlWyml7HVwx9o4UExGKY4Or8hX+khiS
tfkViQHcl0onubbkGcQIp5jwxOsTXCaFOVMh98YIJZrad1mYBr/QIKOfQr8hgBZz
bj6vu0jx4Cd2ekiz7HC3f4EkUHVY8UNJ6tVSQwkxLNs6Ik8kzf/vB1tadx7CJF+W
YBcB5JvUFSmpEG9jD8QwWtexsQfDVsp03507qrNQqmDIxf0mldQ=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char Sha224Key[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBvDBWBgkqhkiG9w0BBQ0wSTAoBgkqhkiG9w0BBQwwGwQIFUqqqUAj5WgCAQgw
DAYIKoZIhvcNAggFADAdBglghkgBZQMEAQIEEDMPAi8KHLjAxwkoLHCsCeEEggFg
6IEh7fK79sSfU7cLIOJUSYaDrRcRNlGNcjp3nqWw1jN+GI4wpGIQH5pIw5H9TNKn
ybJOOmK0B5qPIFjXzN42Jcr6ySIIwoqrri4bjtd3btn9zAMSb35qLMmKFSjSWLRt
hcCcpMdTheK82cjlhY8rxulN8VRyGV5XN7P8y3ZGSWuuxs9p7nXstKbmDQGwiMJ0
QwOt9asXaz2F2FULNk6to0MCaWyn81MiaumSE2jt54YF4NumC4zTXtmL9dJXCsXn
NxKUcLeVFOyy/uiYRTynyRhwY631bgDJJLbehwbkQgnBWJdcINWHmAb6D8zawppO
hpqmYnobplXmY+UAEGyQw2Iki8bkF7f0VQ0ahZ1WdLHJM5SBnziTY/KRrU+WR0Rz
tNoa4PD3vqKhLr+yKhfeLHdUEPJDQWlvnfxCqXUJQsxhznSXS4lx8TOewNg0k2O4
94ffr4lhPglP6ikscTKw9g==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char Sha384Key[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBvDBWBgkqhkiG9w0BBQ0wSTAoBgkqhkiG9w0BBQwwGwQI8C0NOC16o78CAQgw
DAYIKoZIhvcNAgoFADAdBglghkgBZQMEAQIEEHRroCrcEv9HOIJE7TueJYoEggFg
tu3wBtckQN4Ajd/iQgiqx5thHp6EUPSK4xZeV4MTGd7n2colYYT/bIVqTR4RXOPc
KuO41zx9bQ2tb5TCS0I/dR4lxcIDgLw28dNB5bP4VSDx9RvuihJPilcoiQb4uOpC
a3NqkY9Gl2OE2MDwIu3Lu7k0rYOEiCfR20ENdb5bwuPmiYMgwON6IHvjrVocLx3N
w+ZlekZoIl9m3t4AuxKWdzp+Bq8RiPvPreaIDlVKttWcpAlvhO1NfNEqEKUWVFZw
yBbN+JPld8vYLo2MWB8050asX4Q0SB/i8cMwil0JB79rZVG82DQuA92DdzvDwzTM
jRC1LDvyxR3ide9wifPO5MwRVJEaxrnYc2XQdPE3geUhVn5E7KffxXvoqpfLVsXY
XH5mLn4S3d5QcC0ziMoQdhohThC87qSgeZMSK279+5BVkv9OCAgI1okNvU4mGMU2
brYlYcpVALKiArArWNTYhg==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char Sha512Key[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBvDBWBgkqhkiG9w0BBQ0wSTAoBgkqhkiG9w0BBQwwGwQIEThf+7G9aNACAQgw
DAYIKoZIhvcNAgsFADAdBglghkgBZQMEAQIEEFCeJ0k9PSifw0yUTlqYRFgEggFg
ifJJ8quzEPggog0Ctdk/7IyWevLEIG5PANBqZ0hb1v9obLMMcK96YavN6MryMXVY
9GyuMwGlvMP7nhYBnWCG+D1zrvNcbuvAF/BLY1PfyJI+XVrlym2tk/Ogq7M5BeGq
+FQpO1tdpChI+eImdB7yiZRh9aqMaz3kYBzoRx4pEKMbz9C2NYhxOGliMRz47tKC
xkAkTqaUWhzWLk/SztG9HRKuSOMrL25ykBAsfTJJMMGqgE7aQeVqS+3YaT3Fi6Kt
z+Vo/zce1Shc/rLLv/6C5kaZI3Tz07bz+YDTTU7CBb54pyNEyiG0PZvDP3HbVgAO
9sJa27B5gOfR0nRJky9rdF6c/apJ1XPwetOKM139580AutL2dOAGX0JpVrB4qm5+
FgATQwyCaUPNNEpkQ/T6R/2pU0qlcS8Uzdoj+VMWo61iRXTlutDrIhT6b9HHrANY
s8tNZ4c9mEaKIiQzj6Bn2w==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char PbeMd5DesKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBgDAaBgkqhkiG9w0BBQMwDQQI7u5j5rVX55gCAQgEggFgppNi1b9cywxKxMHm
dm2V6c91f6RexjIs2jn5xFMI8V4izAdgaOO8itHOTk4vCBQtBwCVIIEuMx/YdwRz
npErejwsbYaiZ5Ms9YtNrEkb8eNACcTblVdQJY5zny087AA1seOZpBP7jLG98xbB
+mQru7/YXFHBf89oUXaafEhwVPw6U6Auja03/dgl8zbi/nIl1XuKVsViclnDlAJe
GGMssBo7vCAcOTWJYKQeFTrH9agA+244mjp3qzF4idB6J/uSnJ/I8o382VwD5xTc
Lp1/6E9lJTD8AF9qsPLyR23ugN81psNvUQmpB4e8as97pjWrHnN6aoEo4EmZNYSD
lCVoXqeropiMCltKL5lP/uoBj99rhNgpEUFMl34GS74uil4HMQTA0xcD7ON3QVct
sNNSXytNdvpyaieeGaw2UgtBR/EYV8vnQNNN96GHSMS1QmxdzWLeU2/g7DfnQ+fk
YTwlCQ==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char PbeSha1DesKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBgDAaBgkqhkiG9w0BBQowDQQImFRsibZX5jkCAQgEggFgp8jU/VHCXtm+qCdC
6lhScTZwlizfZz/+hBkPr4MLyLvPaASvg/G/DSwb71isAeNBDmmEfkUGknPnKIcj
5F7K2hU4Qts0HBRnS6SDYp6MW2k17Vx5qJ86nSTbeWwt1WvBMiHi5dSrCOrZ4VY+
+360tYbswHg9Cpoj9S9YhV/vH/uDzPRXIxpm6rx4z2Q+awP32PpRGPK+Wwx1zQst
wNj/+R6sij+2repT/6rIiePGAp5ddNNV2/qb6jPWK2z7F/RH3Xtf7S/lrZnHRac0
gmMCa5vVKubcN3tGoGef7h8ycBlYEoMUEqc846spt4v1QsVYCvPaq0dgIE2tszjy
d62XaWyo4lsAn6DcHgG4FvPqmt4uOAFaSww7x/OrZ5k4cfE9t8VgC+3mMArpb2NN
7q5jMdqyJkrSGKTKptfHhb3vQ/JPYo0mzz7MpEPn1DjzQc6bXvUceaeOHHRgR0Qc
GkZHWw==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char SmallPbeSha1ThreeDesKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBgTAbBgoqhkiG9w0BDAEDMA0ECI4BmmWhtaURAgEIBIIBYGfcZPD3lPzZRTUO
ILm4G/hw0LIV9MCWZC76KHkt4XNQb5pjrpnY0amhfrftvxpzzWfV2JWe/RmDaYPH
+tOOb8/gQk5V3ZCLwucEvVIVSs23PW8p+TX5h7ujqzwAEnGeOvRlCIOZJRV+HnsK
41YVpZPGXsPb8BU0wdDkBgh8NLyorcJP8c/9rMFJkilzvcDGfu6STjcqr3imp+E7
PuJvVANXvX2Atf/rJCJ5hfOyBStlflz5HQM0dDVaUutimOkZRdlBLcgHfhhHyVKt
FQdmLG7rVdCx8vO4CFO4cNT0rlT6U3/BQl9g+F+EzaI0L+CZpArwIrAp6+6qRUCo
U3IA4Mh7I6l5CBJ5ib+SD66ebcFPABZS2X1IRTtH5O8skcmDSrYDk4ncToktKqoK
voiAK8y86TiYAJpfIb4j5kdsRhjXuQH0T3MgRZAB+OAVM51LmRFjHJDR0NJQxarv
l91JiCw=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char UnknownPbeKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBqTBDBgQqAwQFMDswGgYJKoZIhvcNAQUMMA0ECAxeBcB1o7C9AgEIMB0GCWCG
SAFlAwQBAgQQf9fi0N55bZN619AMglF7NQSCAWColHo57D00oXA6Y/fsPeaPMrnE
qtXRzs/6hwejMmLzF5tMJCvoOrSeFJhzqAYinrM4g3pY9rmHUzQ0gGb/tBWdSNoX
uTPlY4IsK/b8kZk+xs+r1g8Pw9UtmtiOTf1TQ1x4E4z/A/+65YtP2zTGSJTDFgnd
x0mFo1La8aJ3gPjvDDoDYp0nd8kcAiy3jsoLltjLF+AtYQ29IPDG5BdR06s9fIw2
Zp7sNG6FUHDaCnxNsWobZGTqEiaFQGt1pnez11ksXhnKyF8g/aUo1WiE6Hr3ifUb
tELARHFPL4HlkuBHRNMYiafVnYq27DHbSpZ4svs3vUjr7kIFZ4qSKELK7bdBDLkb
jKbwAHGco2AD2LpGqI3wtiPPn1DK1+WU+AEFU1YTz3OQDU6bHtS/ErBcRZ8jUQQD
kmzdEGgTSVpijX8NT9V+Eph/12OODKURJjO2S4hkHya369ORZK/T42o2+fi7
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char InvalidPbeParameterKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBczANBgkqhkiG9w0BBQ0FAASCAWCjmHnjBH3SjteVGUW0R5+X5vUT14vojeBM
PlRD5gXjictt23vE/82Ekt1/mgMzyHaE7pQ+LrsYTMz19EvoM1AMLMxmAkP3bW7O
X+nXhr462bDcMFWcLdGA+/WkVgP5tvucLANJ68NGc9aa5EbXNhcNeP9eSsWflPtk
+yGQP4K58wY4Ly/J8LW5JSdg2PYF/M1M4HTc6Dq8V8Vws2WjSIafG6HJetXat764
VRtKTzRHvzFAxpmN21C6BbuMOQFrdpxpnHWfDFkzgBhXs/WBT7mOpBh/PYACF/ln
hKPhlXxf1DDGDKIpUtk51yVGszpipXJTEym7n1+K8hSlGE4EM5eegPKBgnXO7KJ+
nDFXHgVSWZxaiouR6k43obUkiFV9trvvK0v+TsT2FNW3WdJRYkfJhsBoKTmXjhti
hw4VsPp0rzwoPWdXnJ/7TzSY48jsHOgybx070byQi2GNAsBwl4tV
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char DecodeFailureKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBrjBIBgkqhkiG9w0BBQ0wOzAaBgkqhkiG9w0BBQwwDQQIU2ounpEUwe0CAQgw
HQYJYIZIAWUDBAECBBDl1OHn4yldrylrQRC14/MFBIIBYLPi8NTR9f96un05tuxd
BGX9QjMSdKYLQF0/6f64Qfhv7RPwy4OTG2p2Ka62lzUlB0o1E0m8A/IAomL5+nqf
pF1NPPCvXui5EySzacL4yfIsx1R2zDAT+g+Uqncfq5NBcx92DxtRDFuv7D4yvTXV
+Z9kfSvGVKQILa+y+AmCOaudvzKR11PtgFFwJvK9ln02vXDWkCVf6aH7w0mz8+Kc
grfBLrqvIT932YZzjFvnXUgr0HHl05/C2OHCQlQ3qVS00n8RLBBz8uSvpK2un34a
VVTFF6FIEnivHT/T61dmh/qajuGM8qdILEtOS1xpf7sOteDxwhIwGPb2sczpJdyG
2lDhYkknCMj7PhSM4CFVxWlMgZ3oVAMKo5TLZtFHStlddAcgbcfmy5EOIp2+Mowg
KkyGGz/A/gPt9EQOCdEfEPWjiYrTgF/+KU7Tx89aGM0wN+2Duudf2m7JVgpwK6j6
bHA=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char UnsupportedKdfKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBtzBRBgkqhkiG9w0BBQ0wRDAjBgQqAwQFMBsECK+w1RAZH8XIAgEIMAwGCCqG
SIb3DQIJBQAwHQYJYIZIAWUDBAECBBCnbuTJFgSzNJKG3Lq2UsDaBIIBYEcSOgGb
dXZsbzfOYI2gW7iVJ6ifMJxOQ/Qzh92mUmzqDgJRfLnPXeq97BBbNPHot/WREqVX
a33prIXqpM3azNYSpwVI6kdo0PowCH22wcARP1bHKxxCBvQfjd3xW4XUDS/JjTou
ky4IipTclV6YArx3OhdT/cfYt2Xa+OCSNL5bzDnUZMy717Obt/s6sfliza8mdttG
N7GzZtuYkb1TTA/88HNL+eQWN50WVSevICjlTnmo/RDjX7fbRXVq0+XrWZEugAGq
qUYJk2T4l+B1wvOAtqc0bTXP4QIYiTpHRtBbYphO+osIaTQgV86HkWd2nS7NOvaU
eKPYivD0Gum3iVzaKtODKZOVbsdPuaAgeU85ciA/SudM8X0/bP4QIiKKArukiyGU
IaPkloTAn11hXaEuWxZLuZFn1lS9brQ6C4BM1JUKUlDkl59tj8stzt0g1WurhCGa
raqLJz2ZICcqOeY=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char UnsupportedCipherKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBtzBRBgkqhkiG9w0BBQ0wRDAoBgkqhkiG9w0BBQwwGwQIS/H4xufCFo8CAQgw
DAYIKoZIhvcNAgkFADAYBgQqAwQFBBB8u7oopfTT8LrPtTuRejeHBIIBYDGWgoeU
gjudjinchR9qiYrSwkw1GMhP0nZd2vqKQB5mLD5Q162Z7BSAtgeBqXzULrlJFffr
mTZ9YiYzhvJVCnwA+mxPfRUxSHgByLYOFIia3+Oy5nAhhizPBl9JWAfNQlGp/DzS
XgsX6+Wfuw8IpqGpl70AYxbu5kKDbGShEMG3cDqK0oHq++HxyNIoT+N4FYLKzOGm
KKYUnUCHAg6MaRY6N959wgTNcMAURTs/bvodNEqsInDyQ+1tbXnVCokdCkIDFUCA
AxAl5b6A9CU+RvX7PtL/7MMqHfVWIwSW1NaxDyevt7wVLMMlT9CGcNe6tf6B1f9Z
oRcpr/TTfIOspyPhHiU2yxr2BnfRJUmnElipgIbGl3WY8N93gDjUqkwf3MMuMZjZ
3bSYSC6jfh86npAoX9EpBYcNr1sUggR0lBi3w+2+EZk7ci+QdB/omKPLsmgBrQB3
X6BdWVRhKKYAkV8=
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char UnsupportedPrfKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBuDBSBgkqhkiG9w0BBQ0wRTAkBgkqhkiG9w0BBQwwFwQIyJHJ/VxoE78CAQgw
CAYEKgMEBQUAMB0GCWCGSAFlAwQBAgQQTuBZnP2kAExu0MI8SNjLDQSCAWCyRv9L
85mOTQx8Z/fIQjWQXcTeMpYVpo8H00w53v+/a3AgSU87EiixuMB269GPherSGV7u
q9ncDyCqdf+qTzwOmSMjMf1vgedkn/FIPSb1q1WKDO2qV+qxlBiq2F5qDcBL8Ink
oLpG/JwuatFs+TxYde3ZLyZ8uhG4NUP4o3SPBYOy0bKN5a+7XK8QS30PcYSCDpN/
ml70AcndIc5f3zKjRyC96FtzY6O8+Q4OeTer7v+JniXWxGwjUNp4NeOs/XrASCUc
pqVxPMEcEf+HhEHTWIdMCJ2VbBXRP+utsvmRThZcLOS7TZH4COnhdup9hVxFwLUX
jDYiiVnQVoalxTKm9GwxNFawNsoKwvw4cH/9Pl0P12nZzXB34wvOm2XnxuZ0lmx3
0O0oFWZxAfd8gViiMnkNVN5S/AWRbhpqSbecS97iWmqznEZI90LUlEyGmR8oMgm1
5YjG8ZOBtwTzbuA7
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char InvalidKdfParameterKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBoTA7BgkqhkiG9w0BBQ0wLjANBgkqhkiG9w0BBQwFADAdBglghkgBZQMEAQIE
EHEccUYbbsmW6tda2YWESAcEggFgvvqe1Rp12bvrNCZWUtnIR2Fkvt16tM8Ml9lO
UrCEdb3/UKVZNf8OhW5beEPWqDIzcdqzMSKmH8gfc0MTsA6GIDqwp+IrtFgKVTZ9
tnSrOCIUPzyUj54l9gxJo8mHNyvTVrsmnS/EH4cVMrDXXzDmOBfCYJBPydAnY5FW
Pt+dNRwB5KDs3IaW7T8wzyTfb3mcsfiNKiejJqcaG2fwjH8m/3Bz6UsHtGGjYBEO
L+sM87E0+7av3JWTUaInTWEXavVeJxFOoFNT3nx1CG1J5CQ00AwTTHQMmh0Ny3Jo
WBPasxysN/6sHESNnaAVQyjs6vDN7hRU7DGFcETP4u5hnoDdF0fa9xL9OXXIZZBT
bUE1cXzXHS8mMtaUY3nhyO30mAbwo17Y+fY/FetWIFY/QdT8UJ7ALN+VJWpRDArd
91Is4IE2giXb+z9qSPIqHVWW0nQEv8MYGhQzJvByzaHABc5mfQ==
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char InvalidCipherParameterKey[] = R"pem(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIBrDBGBgkqhkiG9w0BBQ0wOTAoBgkqhkiG9w0BBQwwGwQIytMit9DQQ3oCAQgw
DAYIKoZIhvcNAgkFADANBglghkgBZQMEAQIFAASCAWAlGaPB5XzwYDbiwo5fVVxa
/vbVnNBsDB3WqKdi0iX3DDqo5POYrkokp4Q8NXhCT8PAm0PVjfhuL+xhnDDSnxtM
Fme5x32I993MZXLHGfJf8p/ksNv+KXhACRxoyexAjm8Al/ILqmm/MfFoAqnNl4gJ
TPfJQwMx7ErNfvpv2fkCHKNT4I+hlHkCH8TXHVptLQHFQe9M/HXGarpevnDuCXn0
Py4wiNI73Pz+nJFr+cRInDZyXBguILdCMnXiDfotLsIEuPQmULM+91taxSS+edK9
pK4t/64BLWwvEiAoLDtiQYQBZt/Um7LC2coN0zPi/pEWpUcut5ABblodxh13cAhS
zdWPxdqWnEOxcozyMAH8K8AdXe/DLBRfm+i2E1+2XVZCNrnoh4VeFixJbji0aMVX
ce6GoyUeDGaNh41RdjlDX34CgbhPQQjX1XLS5Bza9oCZEmIEyoFEZdmj+Kwx+mhW
-----END ENCRYPTED PRIVATE KEY-----
)pem";

constexpr char RsaKey[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIBPAIBAAJBANLAVsNYAwOloR0kh+ZkPXB3lVX6JLGayh8DxN0X5T14XJKqV0dy\n"
    "Rk11ypd1/tLNJ3P+pEf7JS656KNl9F1WE40CAwEAAQJBAJX8foL9cv+4c4YbBoDH\n"
    "3BXbdTNAF7cyUyBpfURgCiyI3hCnI+PSg67gM3Uj26kwlQWcxeyXfvyikVMK/fzP\n"
    "vj0CIQD3JJ03OexbpdmpVheo5bepVl2sQSb7J4F0n2A4pX1zNwIhANpN2m2kSniK\n"
    "tPQgBzg1uHOz2d4bXByGc6kOM8cw0FlbAiEAkuW2dN7++DKKP7amdYOlOQfWwc4w\n"
    "B1SiA9m/bawf9t0CIHwZlfqOUueDsqq1KmWB5IGh1FC3LP3qUQXh1P1/enHRAiEA\n"
    "m9zhglUBoQst4upwxOYfgar9ZmtI5HHu+pdzor6Lldg=\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char Pkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIBVgIBADANBgkqhkiG9w0BAQEFAASCAUAwggE8AgEAAkEA0sBWw1gDA6WhHSSH\n"
    "5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrnoo2X0\n"
    "XVYTjQIDAQABAkEAlfx+gv1y/7hzhhsGgMfcFdt1M0AXtzJTIGl9RGAKLIjeEKcj\n"
    "49KDruAzdSPbqTCVBZzF7Jd+/KKRUwr9/M++PQIhAPcknTc57Ful2alWF6jlt6lW\n"
    "XaxBJvsngXSfYDilfXM3AiEA2k3abaRKeIq09CAHODW4c7PZ3htcHIZzqQ4zxzDQ\n"
    "WVsCIQCS5bZ03v74Moo/tqZ1g6U5B9bBzjAHVKID2b9trB/23QIgfBmV+o5S54Oy\n"
    "qrUqZYHkgaHUULcs/epRBeHU/X96cdECIQCb3OGCVQGhCy3i6nDE5h+Bqv1ma0jk\n"
    "ce76l3OivouV2A==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char UnknownPkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIBVgIBADANBgkqhkiG9w0BAX8FAASCAUAwggE8AgEAAkEA0sBWw1gDA6WhHSSH\n"
    "5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrnoo2X0\n"
    "XVYTjQIDAQABAkEAlfx+gv1y/7hzhhsGgMfcFdt1M0AXtzJTIGl9RGAKLIjeEKcj\n"
    "49KDruAzdSPbqTCVBZzF7Jd+/KKRUwr9/M++PQIhAPcknTc57Ful2alWF6jlt6lW\n"
    "XaxBJvsngXSfYDilfXM3AiEA2k3abaRKeIq09CAHODW4c7PZ3htcHIZzqQ4zxzDQ\n"
    "WVsCIQCS5bZ03v74Moo/tqZ1g6U5B9bBzjAHVKID2b9trB/23QIgfBmV+o5S54Oy\n"
    "qrUqZYHkgaHUULcs/epRBeHU/X96cdECIQCb3OGCVQGhCy3i6nDE5h+Bqv1ma0jk\n"
    "ce76l3OivouV2A==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char EncryptedRsaKey[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,5E85BDFE683369EB3F468C204E928729\n"
    "\n"
    "DNQVBD7Y15Ud/Rf00RN7FoWAeH1YxcBbwUo/OT7d/bIWsp4NckdRPxyT56g5Nq5v\n"
    "f9wRj/KalM9hOAryzdtwiP0pOQRreoyam+Fo6X0YCSCPlF2lYZ7prXYLGC5h4sIP\n"
    "0fszA45NfdZupWVnG9SMGXnxhuccY/LRHH9UP2KkQft1ToLQigyRp4BVgJK2+I30\n"
    "JhB6nqGuwaZSGGtyl/f5CPRenKaDeQF6IxVuZzifGBr2p/IJXI6opDIQqmx9HuDT\n"
    "DhA6GbZCSGDsPt7n0TdqKQavHVXqv+lf9omOGi2x/y9hWVpvNX0gc9CRN1V0Yf4N\n"
    "H23qSS27nms+hzECCKmO5rilHk20sVqtxqeLmZk3ojLufhIG8/+zUKrIf7a/MlmF\n"
    "RdzgyXBmvAHz4tteqV9cWH4Ymzz86CAaEqgKN3PJ9u1yO4e2K4AvloPR31CmvLHb\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char EncryptedPkcs8Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIBxTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQlJ/brjLdMh7Wnqr7\n"
    "fgcL8AICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEAQIEEEE2FkR1Ee2mORRN\n"
    "VVhAX6sEggFg745xIBaAmv1m9Y2P2dhQsU1wvTwYxAbRQbgSKac/voNuzi7b0elY\n"
    "1h7RO6ibyNWQLqn0B8QmCt2rYczKqKSGGM1Pw+OZn5Hwgda6wL/Uuz4P3Buvlkbr\n"
    "eDoTbbURsok3MMCS5ZI0DBWu+XoJbPMFPVLTuV5ZxePgATeVjVdX2aW0fbqKSbp1\n"
    "mgKtBKGoo5+R/mnwEqMVkftAZ6m+8J8HSi/KLUERwPxTUw6zCPUc+S2raAAayK5f\n"
    "WYWEi3hVTWgD7OYTgkPp1rIfiOyRnKbAxismiKjHI/skOesIgGWWKnpRTbfw9t2+\n"
    "a4bKTlR88TdKYDNh0bGqp/ZhrumFdjkUoqPzP9GEgv34ksxALd2ffMnWAnn6avFE\n"
    "6VTAacCT0xGVvdjvU7p64pw75Kg1bGjpFNxsTu6j7ciWzJIXGbu55oAWoBHMcAIG\n"
    "TAxZByRP7+BtPAkMGrM5gR6WEOdabmVM3g==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaAes128CbcSha1Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFHzBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQI99gtv1CI9O4CAggA\n"
    "MB0GCWCGSAFlAwQBAgQQlkHroEteerAZ/bDKlEJ4uwSCBNAgkRP/KMDMer588aay\n"
    "2tQ/nK5glHqe/or5gJ8XgReXN/JiKl3c1n3ZDmCpPb71TMWGcX0D8GkRDkk7nGyB\n"
    "DFjFAUP0CRoPvnOGki6NOIGnUwBnvOVIaYrmstcsBYCscTUyPUsKvy4HX5EwZyaJ\n"
    "DGSaz4QM6XfsvsbgeQw8K/6I7wuYwKrA3WaBvvvNein2gh0hd/CFUQ41RUZdIHay\n"
    "nYu8XStKuJOMmeNVKR9K2qQ9WP37EY5YsMTcJ94IzNfnSJtATTahvBO8v9KJwvED\n"
    "vQnOpJhtxAH3CxV3EIh33Jpm6WRUNcb4r5VXJJIzXUtgx0rs+PAN54kUF8GzQwPr\n"
    "mTGhIRwY29W3ABvx/xHi5NevwUbkQgZZ+oHsb46MCSDBY8ZgYFT1MAPJNOefwzNw\n"
    "h5SHNG7nPqda3DQAxzI2ACoeH0QzUp5L+BiKWdDXKFeN6OPxIAufGOBSjUsHv1Pu\n"
    "GrhT4LtIlOepXJsrrN1R3o+IjGfu3ZBsvRZyOs29XbUDvaSIoYldRfgfmgeqhIXl\n"
    "8rUn/9TGxzTDZy+FnaMPsCzz1wrqE9yzc/f45ZV7X/2vKaaiU3zouquzEvvulwDM\n"
    "+yxA0d3fCHW5o8ycsmj0FRv8bgLsWLvolj4J3vtU3iPcRqJXEjumHs/8Z1PZK4gv\n"
    "/dLiDHyorwktoM1keCGUKxD7BurWQVhKkAn2TTbQ1IMefS9gu3V8PF3G/RGJ6gA9\n"
    "V3fvJ1EiWY0bmQ6DYSK7Cjw4JUEdN0m8/Lkuq08Oa4qldLbTKGoiwbetpc2xhtFa\n"
    "Nx1Vc2sm5QjqCCjhuctV0eeGp7+7qZxt0XpA6V3MS7e3Un/4oFN8FykJVgkWdi7O\n"
    "4/upneJ9qhCZCk+01fcjNNpZaNCyouN1oJ5h5raIOTeQjAkXQxJC9lSdpAlEf1Hi\n"
    "J9Y8NmMQ+/lPz8o3IkQhffshmStINKmyItnPwTpF8dnjYgpXCA94pGrF4tmo0M5w\n"
    "9eJVSmOX7zWXz7BkiKhzJlBKZ7PlFaXxW3Wt4ussdl9O3a94VQ32W5/UfaxZa8Ex\n"
    "mCdoTIUr+uA1vzXdLcF/JkV0SFrc9zYfV+QyEL1muIUqDfNskpQ107BuKksF9iyt\n"
    "2la0gcl5iKg+3MBb7tbJY97m384kKFJJUajx9nrfWJRs2JTunNprPfw+mTLbdAD1\n"
    "NWP1GlekWFLf8w4X+fKsBzixcHGfBqzdAgy6gdZkR8aOdtsyW63+Fv6sIgdRkaI6\n"
    "EjcwV5lbIfyMvvG6jan2xuSTRLdT10wf1e0n2XJKKxa3OvgyW7O7lyr3+zgWjJlX\n"
    "7T5x7E21ar+g3DlTCr8fxMvfQYedE1JIIiC+lGUZPu617QJJjk3ZNLOStLcjbIjv\n"
    "gjruLebCAS8Ftaxo1yDfWL24klQVh93jNUc4DRoAZYDa0qIRHBza53QJz/RcKR/4\n"
    "pSzzUd4SV3VUivTG2c2M8m38Uaf/+BKUx79DYwtiL9ZujtOx9uFzErYqJJOzGARN\n"
    "T9q2BHIpzDYM2SBMq8qEv7boaWFkvVrBlQXIndnx9OIPnk600HCpK702FiEvCuFd\n"
    "gtUkw7e6gJmEBw24JAjTRWFC6JS2pVIk6kiNotzxA/yvkp25knQ40HIIdp3j03t+\n"
    "WSr6bnl0BeAm82O0I1+SDxhfzA==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaAes256CbcSha1Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFHzBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQIgcuEOTdIlkMCAggA\n"
    "MB0GCWCGSAFlAwQBKgQQwdV7tEny6Wrl8Agvm9P0uwSCBNCKexqfXpNWdQpCEneM\n"
    "0zAQSc5tISV4rMxm32xSosIf3bc2gFftmPKU1y+wMb08HHL4Gwc8Pqzim0xxivxQ\n"
    "FKIsqN5UG4PXSrvZ49Be24FQd6lf1u0BKXlw0UXN8uzxVoTlh8+3e/TIqPne5nN0\n"
    "NX7GSDPneok2fNBP01fTojzVEYJWU592wGpoToNlNQKiG84xzuDA7mYtvQRy2IYF\n"
    "uakZV8aah922pFExNYLUqGSDbW1ouWyPj0fmEUnGQ/UZAZed9UsiaxRNzOJoa8l6\n"
    "vfQlO3DfjU23LOLMLCnKeRhVygqMoJ+yZOc1VWWzOPUAhQS7LujYHbhtICU4hIJh\n"
    "Od/B6QEcYO0qNNibk2V9vXM+Pb+RL1FKnSKSTgH8B4cgoXF7Osqtzr6WwM4i8UWd\n"
    "nHkyJEctctvMgWA9NQwtrSj0izZyOg+qx5SZJVN2LhKhUmnbv85Gp1EcMklOdZ/1\n"
    "ttqaeq2BYjGEVZVQwcegv5T6cHTugOfA5Frvt2KnifN+b3y/ZfDLTW86/Bq0nQVg\n"
    "rNBlXGLOYuG1gT6cPdAc9zUjjK0mJS8FUliQJRLHCOWyCPvNoV/BAfNsjRkK/Guz\n"
    "nuqY5hoo5frYgoXq7aQWdHcW4jAIPpCSYmNhwhzNPebl9NiemDOredKjlxDImV9a\n"
    "I8gqxEScjXqPLx+rCE/cIT8c8tfAj8i7rL4EMmgE5NY8tZpAcODsJeKzb9IfACmm\n"
    "oEO8XbpoNTv0ku1WNc7aYsBo+6MvK3LynpC3QS2pLBeyUksh+DU5Awk7C5krF+Pe\n"
    "vje1eKssnTN85DW0KrA5x+SMIzBALUqRTqkXepZx7DSCdkXBgwVYMoV8s50Bw1Hh\n"
    "WER2m7Ok/zSeKl0ygsyfo33z47syvJeZZAt/ZPzuc0mD7QfRZ5hd426gMN4IKSqS\n"
    "U8EQaA/sc+TiwgdnbVaGRLvcpD7elqEmyQguc/nmEM5ZPDpKbK8E0lf+btHj28t2\n"
    "850lHO+1d+kj3RZgqoKo4NImUbqIeVU9MMzzkZLU6AgFDBQSP7Ndl3q3P3kXe1U1\n"
    "Zk8L6EXOfP4sPb4MsG8bAeN1OO6sqKCTBqaFP15bH4rwDOCUwhn2s3parTORJBQ+\n"
    "2UyMzWBlj2zUbwM41gpmKoF7AyeHYzARYCzTe3uDUVK/Gu6WDyP7wMpTSy+o7Cik\n"
    "76XISxXk7XSEFNqHj4P8MyDZgXFKvBSiqhMJNcQXv+gWQFD1qoZ/8xmjtzaoMSSF\n"
    "Bm6VO+W6h+z1woA23iiHvYqbkS3UwBPjYTLu7fpcQIZLOWndNk5J/gtUyQoSyaq7\n"
    "59JPrSsWMk3EDHv8YHB2N0Vl2bIJk/+bd9wZ4TER1H2IAvTOwKR/ZacPODGJ4xTG\n"
    "qYn7nqmwlebh97S3satBR76E25Nsn1xedZQWS3VgOrWnPFnGg2T8psT4fLNFdr+g\n"
    "wxR5FWIMFcRrpxlPgyyfcbumwYxsqfS93TncoUw4FhQgvbg/eMoZ5YGSsuzRG9CR\n"
    "N0YL51xzjbbSmJdQFrzTg+jFxvRVKm4mPhjStcIDUHeHnA9JaDpgYp4N/YdgSaXM\n"
    "rKlgLih0JlVG3Wcuji3Zo4AiK5fjaQpSalOx3gFWnNGgx0B9DonP4YrrUfdXp0AH\n"
    "A9J+aEKrlkZz9/IAYqZXhfpcYA==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaDesEde3CbcKey[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFDjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIhsoZqxVCut4CAggA\n"
    "MBQGCCqGSIb3DQMHBAiN4DIMwVlUIQSCBMgPLVHQefJDu5oE/CfrwumHBLjVJt6i\n"
    "dSXhEfL3yt49atXi0OHfP6nyU6UYnE1eivnj8nNofiIt9CE/7cconqa6/vMUgt88\n"
    "PxHvuUaYO3zmUmZF8VpsLW44kZTAze0s2ScBUmPCXxDDR92sicdebFgdD0KbPvJ6\n"
    "ycOXOOKTVJis1uuj5b9xQESyYtEooFJMltITwIXflFsAZUx1VZU9r1/cqR4VN7+E\n"
    "rzb0YFmm77xuPqoNYNj9K5b6MpQefoqrJjrWAquGnUPg5EoLLJbo3Grm7AefZh49\n"
    "jRgwDr+avAm/yc9iswa9qsDzKVIVEkjBFp/LsvEP1e7GYlQan9EKPmnYFBgUl0Tv\n"
    "LQ6/J1illujK6GmMOJ8K0ifFFRrA22LNRoeBc6KRAl1bHHnPq3aeg+1tEI518B4M\n"
    "v9N7+YU2knxlnuSv+Gxe5sfqJKiBQ2050Z7dWT0WRn4B3S4S9eIbGIyZZXIo5Mun\n"
    "JDXpUakFtGYnI50XEcfvmrScNwoVPOczrki9GgPjs8q+orlyHxSFRnu2p3pfncFp\n"
    "eHe5DaiD1fyiOPapimHF6bUGO3jBP+ylGNVpE+czvgqptvV65QW+jxF3WzVRwrdu\n"
    "eA5kGg8Is9b3cKeVQBxctXW7jf/1jdJ0cTwJsKoqfpbgEs3KA0DfF6I+1uH7HgZj\n"
    "q07LUILQAvTdj/tEJkcPE4XEnd3mH2By+Qlkre1Ir+pZ/yl0trnIgZa41dDMLT/X\n"
    "YvPk3nBEbYq62btqpVZrm+5Ao57GDv5azZJwm3RiE2dTTMGcRaTbbDtFBzODjWnw\n"
    "/bcW1zJgpwB7GsIAEZy0eltlXZLBU5tOSLqU1MA5q+LDw4P2lSwNzHOq1/cMsXri\n"
    "LV/jmvT2twSE1QMBGsi/Dt4aonsj/OvbNcXn/mOcALq43SO3901qqChPiq1TmX7E\n"
    "cH+dqboTLR+6QKUKjH/Qi28v19JWdTdiI7H3uUDsx3phjcfwl5NWNCWvBB/DPtor\n"
    "XdZFoFSSTjlH1c+4BQd1SKx1nQL99bEVtY9GIV/XRCuGL6YNt/S/+0psOLJb+y6o\n"
    "zxaCkmgquKfa7GVNLqEg3Ou6TjgmiGrJGfWcmhdSwLfqD2uGs45AOAj4KZhGLvOt\n"
    "xpJO6zvjhk95B2ypjzi3ecK82MRLv9WsBuqVPGi7YamXhMzy9D2rN/RB5rY9+SuN\n"
    "uvF9fzH5NGKCUyMlKTtkIL+41UccdyNTTQzGFrxR4OmvpU0pUWphjmvfxaF5522S\n"
    "y+/Uah24xnofIEwCBNuq7xor4UhC7UpMZmoSzfSIOi6ka8tHCd+HgIDPu1A5RQv+\n"
    "marWaQyzM0sUkwKoKifTJnE+UnKo0O8AlCstT1yoxpSqrTt15kskSlY1I5CRFRBM\n"
    "VoLNMye1nZu3W4MNQUv9/OqA/qxoeyoco599fYrEyeCzFay6Y4w3+4YjXoGLnlhF\n"
    "IuBQZF5Ojh5XfOS+z2Vg+I5giZ3qVaGq/dA/CYbaDzt0XYiRApyd8SMiEt2X0xGv\n"
    "tToRyV7bXzD/+ctEhQbdoE3TX2AXBHFNC/I6iyzDNLoDlNBk4IJKUcjSK657rk4m\n"
    "UeZjwx2aqJHAbQRKiBwGugimbLdFv4crKhSU/bzY2fPt87+txIMxmw3/ToQ+hzev\n"
    "nW0=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaPbeSha1ThreeDesKey[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIE6jAcBgoqhkiG9w0BDAEDMA4ECPpzoDumIjAOAgIIAASCBMhquuOGqb6dgMiz\n"
    "LKYOgj9Vt495qEQxuWMpAcwX/uKI1QrYWzFoQcV0Vel2lo3WKQtqjanALirODmmw\n"
    "g0TYexwN9zgkyOq0zyzV6XX7QI7v4mo/t3etUVD4dY115fgzjnA147XS6DBtR1oM\n"
    "mzzPuTHhyjeqxvegkmEof66ZboIfdnbENFerVpBFVJPvRsehas5dzs1RPcAaK+fk\n"
    "fZ9Bu9rNJB7zm0pskI5O6S3owAk6ACjlEickxGHSwDb47n+syDNCJiMC6fOifbcQ\n"
    "G17tLwi4WmvmD2QO5qxzcTw9IxjQUj2PB2coqQnyZFG9UuQVofcDHpjjel8J8RD9\n"
    "b16kONpUYP78bBEZ+PRox0VTSrlaAVDfpjUaRxFhChhPsSFDEdwmKpy+z9YBgFiM\n"
    "Ud2lZlK1e/5XxsuWhpUzyaNNK0PMaISaws0+ng1aPeXkq5MWPrOOw9NyjFzQWodg\n"
    "SCq4740C+vIkRxZVvkwbP+Redan3vs/YaWc9wJVwFwvB/MxDIbKZTnW75M78ViHu\n"
    "JJ8DJ9ksbPkXqjUByUamZ+NeIXaDT4TfNaevvJvHOsl4o0kz/Gx2s84PdPPKXpkU\n"
    "OAE46qIAtVmQR4skn6DrmV+nhRpAVPTW7XB2HX49NAgH9uUeO2eDNNm7QTGuW024\n"
    "HIw94LMvHzlCCvKuj0yLI9OTaF86JSEbi9bauW0crP0FYPrbBCi2cFuP3yklHWTU\n"
    "ftzHYlhRWdymjV7CoMiMDF5J8zkaKLUdGyy99dYtBKo9vIn0lXdHdTmKvhCr8TN8\n"
    "8I8leQcaOavdv9IanRerwcKP/PLx9tsiCygV+4rWqyxnrASVaFkCZhQhjF1N5EzI\n"
    "NA96U5B5NgaBBZfa6CIvM1/N2+hSKV/iDVKqQJUx5ZdgjsXDR94Thrwqt3TqOL/k\n"
    "BYg385/+URZKzIIgYxSj7tkkfUY9+YfypgnQGT/Z6gQnpSMIVPu7MbvsXQF1Aal2\n"
    "nMEaFvcW30RDQXJmLD6nyukByUAhL80olgRwvthDE9RK0fwcjiptQGZo0oxcXEi0\n"
    "in2oPiqzUyLmVxitxb4a2y6TsN1GEWJTWhBbHIPCH4hVUGRN/h/NHGTGAYq3VIoc\n"
    "dhG7gtgFBL0cngkAUZhh+R+dFX0MuoFhw2oaVvHMEJfRINGaFwv907J4hoo7ojD3\n"
    "t9+00jX5drtCHhZu9/7RHfkQ0DwipffM+0DKrZKBSCPke0Tv+27j26bYXuAJbOzW\n"
    "xgAx0ITvKYXT5f6/Pw1V+foMjYLVkjjqQNGliLjZ0wFwszYrsCJS9kKs0yNhwUPv\n"
    "c4nAy4Nf3RMiTNKWV9tQDBeQEM0HEGk4wSnk6GaodUa47HPDLGoZCq6CdH1wuxey\n"
    "SIFCO2Iu33NH+YwHd2vRg9KoiOVMS8rRwOFsxJ5oPvub2wSYf/0Adw2a0AHXGLhm\n"
    "4eG99L4uidchDPVriz6Lcx3my/b2FXBHwZOPeGdG3yfw18gyFNtkIYU3DCnVs7Ma\n"
    "v/+jD6kkhCnmw0+2doZV0cXq2KjHfW6OyzEGWJ5wWCXenFXnzkVJ02Gj43GFzo2N\n"
    "2vWeOY1dMn/1mWhW6Rh3sm9CqgN6diN90bqgx45Xngy4hNheTcgRxGS7W7c9IrZX\n"
    "78bPzSIiU0WlWbp6HOU=\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char RsaPbes2Aes256HmacSha256Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "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\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcAes128CbcSha1Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAgterpQvmJgBwICCAAw\n"
    "HQYJYIZIAWUDBAECBBCnGn8ozwSrNHNmPDJARUzEBIGQUXOdEa7PffOFYX+9Q6yq\n"
    "0TBZhx9K47XbPsxPGOVx7R0fVVXUiBQeSxcg3envSPLUUlkr3Qqr+lmhrDuBQ5Eo\n"
    "LbLcurZt5BZrUGwY9U644qA5hs5mPKZ1GonBLDqktAidu3CSWkgpGkrnIrQjIj1Y\n"
    "9slBFGBkMGnPquSV+pVJBbkY0Y5817n7dLscAC5HkSTC\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcKey[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIJ0uTzDKADEDYlQfHVb/RxDD4uPfMUZnVFdYXj9YzEwXoAoGCCqGSM49\n"
    "AwEHoUQDQgAErKFNU9WIlmT6ZSGVn+9BvcxRVqTOjzIMQFJTy19InKv+QMcg3Jwk\n"
    "zYirxNgg1RH/tyZaYYHioJhiZ2Wq8YAVKQ==\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcPkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgnS5PMMoAMQNiVB8d\n"
    "Vv9HEMPi498xRmdUV1heP1jMTBehRANCAASsoU1T1YiWZPplIZWf70G9zFFWpM6P\n"
    "MgxAUlPLX0icq/5AxyDcnCTNiKvE2CDVEf+3JlphgeKgmGJnZarxgBUp\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Certificate[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBbzCCARmgAwIBAgIUCN9mscYw2Bnyw0a+JrAKTSCDHd0wDQYJKoZIhvcNAQEL\n"
    "BQAwDDEKMAgGA1UEAwwBdDAeFw0yNjExMTMxMjE1MTJaFw0yNjExMTQxMjE1MTJa\n"
    "MAwxCjAIBgNVBAMMAXQwXDANBgkqhkiG9w0BAQEFAANLADBIAkEA0sBWw1gDA6Wh\n"
    "HSSH5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrno\n"
    "o2X0XVYTjQIDAQABo1MwUTAdBgNVHQ4EFgQUow41oJsXWOPSZTKydDRUCB3KoXgw\n"
    "HwYDVR0jBBgwFoAUow41oJsXWOPSZTKydDRUCB3KoXgwDwYDVR0TAQH/BAUwAwEB\n"
    "/zANBgkqhkiG9w0BAQsFAANBAFIQmR3N3qe5yI16EzcHlE9GpiLkpHcIKqVkYXAN\n"
    "iWZjkQPvxYEluRZYTNj3YtAFoEkCOx0WACCzvNPG0YMQbbE=\n"
    "-----END CERTIFICATE-----\n";

constexpr const char* KeyFields[] {
    "D2C056C3580303A5A11D2487E6643D70779555FA24B19ACA1F03C4DD17E53D785C92AA574772464D75CA9775FED2CD2773FEA447FB252EB9E8A365F45D56138D",
    "010001",
    "95FC7E82FD72FFB873861B0680C7DC15DB75334017B7325320697D44600A2C88DE10A723E3D283AEE0337523DBA93095059CC5EC977EFCA291530AFDFCCFBE3D",
    "F7249D3739EC5BA5D9A95617A8E5B7A9565DAC4126FB2781749F6038A57D7337",
    "DA4DDA6DA44A788AB4F420073835B873B3D9DE1B5C1C8673A90E33C730D0595B",
    "92E5B674DEFEF8328A3FB6A67583A53907D6C1CE300754A203D9BF6DAC1FF6DD",
    "7C1995FA8E52E783B2AAB52A6581E481A1D450B72CFDEA5105E1D4FD7F7A71D1",
    "9BDCE1825501A10B2DE2EA70C4E61F81AAFD666B48E471EEFA9773A2BE8B95D8",
};
constexpr unsigned char Sha256Abc[] {
    0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
    0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
};
constexpr char Sha256AbcSignature[] =
    "C07F57EFC13A4CCBD6F9D7D2368573E621F731DAE336F51CC783B5A75EF7B7AFCEE214C194E40D0CF778A0D0D61C344D967DBF3BE1D1A059EC5A0EB6432D0EE9";
constexpr int DigestSha256 = 672;
constexpr int PkeyRsa = 6;

struct TestPkey {
    int type;
    int saveType;
    int references;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL PEM private key check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

std::uint64_t Error(int library, int reason) {
    return static_cast<std::uint64_t>(library) << 24 | std::uint64_t{0xfff} << 12 | static_cast<std::uint64_t>(reason);
}

void RequireErrors(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const auto code : expected) {
        const auto actual = ERR_get_error_nid_postfix();
        if (actual != code)
            std::fprintf(stderr, "error %llx, expected %llx\n", static_cast<unsigned long long>(actual), static_cast<unsigned long long>(code));
        Require(actual == code, location);
    }
    Require(ERR_get_error_nid_postfix() == 0, location);
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    for (auto position = text.find(from); position != std::string::npos; position = text.find(from, position + to.size()))
        text.replace(position, from.size(), to);
    return text;
}

std::string Relabel(const std::string& text, const std::string& from, const std::string& to) {
    return Replace(Replace(text, "BEGIN " + from + "-", "BEGIN " + to + "-"), "END " + from + "-", "END " + to + "-");
}

std::string Hex(const unsigned char* bytes, int length) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    for (int index = 0; index < length; ++index) {
        text += digits[bytes[index] >> 4];
        text += digits[bytes[index] & 15];
    }
    return text;
}

std::string Hex(const void* number) {
    unsigned char bytes[128];
    return Hex(bytes, BN_bn2bin_nid_postfix(static_cast<const Bignum*>(number), bytes));
}

void RequireKey(LibreSslRsa* rsa, std::source_location location = std::source_location::current()) {
    Require(rsa != nullptr && rsa->version == 0, location);
    const void* fields[] {rsa->n, rsa->e, rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp};
    for (int index = 0; index < 8; ++index) Require(Hex(fields[index]) == KeyFields[index], location);
    unsigned char signature[64];
    unsigned int length = 0;
    Require(RSA_sign_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, &length, rsa) == 1, location);
    Require(Hex(signature, static_cast<int>(length)) == Sha256AbcSignature, location);
    Require(RSA_verify_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, length, rsa) == 1, location);
}

void* ReadBio(const std::string& text, void** reuse = nullptr) {
    auto* bio = BIO_new_mem_buf_nid_postfix(text.data(), static_cast<int>(text.size()));
    Require(bio != nullptr);
    struct Guard {
        LibreSsl::Bio* bio;
        ~Guard() { BIO_free_nid_postfix(bio); }
    } guard{bio};
    return PEM_read_bio_PrivateKey_nid_postfix(bio, reuse, nullptr, nullptr);
}

void* ReadBioWithPass(const std::string& text, LibreSslPemPasswordCallback callback, void* userData, void** reuse = nullptr) {
    auto* bio = BIO_new_mem_buf_nid_postfix(text.data(), static_cast<int>(text.size()));
    Require(bio != nullptr);
    struct Guard {
        LibreSsl::Bio* bio;
        ~Guard() { BIO_free_nid_postfix(bio); }
    } guard{bio};
    return PEM_read_bio_PrivateKey_nid_postfix(bio, reuse, callback, userData);
}

LibreSslRsa* ReadFile(const std::string& text, LibreSslRsa** reuse = nullptr) {
    std::FILE* native = std::tmpfile();
    Require(native != nullptr);
    FileStream stream(native);
    struct Guard {
        FileStream* stream;
        ~Guard() { fclose_nid_postfix(stream); }
    } guard{&stream};
    Require(fwrite_nid_postfix(text.data(), 1, text.size(), &stream) == text.size());
    Require(fflush_nid_postfix(&stream) == 0 && fseek_nid_postfix(&stream, 0, SEEK_SET) == 0);
    return PEM_read_RSAPrivateKey_nid_postfix(&stream, reuse, nullptr, nullptr);
}

void RequireBioKey(const std::string& text, std::initializer_list<std::uint64_t> errors = {},
                   std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    void* key = ReadBio(text);
    Require(key != nullptr && EVP_PKEY_id_nid_postfix(key) == PkeyRsa, location);
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(key);
    RequireKey(rsa, location);
    RequireErrors(errors, location);
    RSA_free_nid_postfix(rsa);
    EVP_PKEY_free_nid_postfix(key);
}

void RequireBioFailure(const std::string& text, std::initializer_list<std::uint64_t> errors,
                       std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    Require(ReadBio(text) == nullptr, location);
    RequireErrors(errors, location);
}

void TestKeys() {
    for (const std::string text : {RsaKey, Pkcs8Key}) {
        RequireBioKey(text);
        ERR_clear_error_nid_postfix();
        auto* rsa = ReadFile(text);
        RequireKey(rsa);
        RequireErrors({});
        RSA_free_nid_postfix(rsa);
    }
    RequireBioKey(std::string(Certificate) + RsaKey);
    RequireBioKey(Relabel(RsaKey, "RSA PRIVATE KEY", "DH PRIVATE KEY") + Pkcs8Key);
    RequireBioKey(Relabel(RsaKey, "RSA PRIVATE KEY", "rsa PRIVATE KEY"));
    RequireBioKey(Replace(RsaKey, "KEY-----\n", "KEY-----\n\n"));
    RequireBioKey(Relabel(Pkcs8Key, "PRIVATE KEY", "RSA PRIVATE KEY"), {Error(4, 4)});
}

void TestReuse() {
    auto* old = RSA_new_nid_postfix();
    void* pkey = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, old) == 1 && old->references == 2);
    void* out = pkey;
    ERR_clear_error_nid_postfix();
    Require(ReadBio(RsaKey, &out) == pkey && out == pkey && old->references == 1);
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(pkey);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    RequireErrors({});

    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, old) == 1 && old->references == 2);
    Require(ReadBio(Replace(RsaKey, "MIIBPAIBAAJBANLA", "AAAAAAAAAAAAAAAA"), &out) == nullptr && out == pkey && old->references == 1);
    RequireErrors({Error(4, 4), Error(9, 13)});

    static_cast<TestPkey*>(pkey)->references = 2;
    Require(ReadBio(Pkcs8Key, &out) == out && out != pkey && static_cast<TestPkey*>(pkey)->references == 1);
    rsa = EVP_PKEY_get1_RSA_nid_postfix(out);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    EVP_PKEY_free_nid_postfix(out);

    static_cast<TestPkey*>(pkey)->references = 2;
    out = pkey;
    Require(ReadBio(UnknownPkcs8Key, &out) == nullptr && out == nullptr && static_cast<TestPkey*>(pkey)->references == 1);
    RequireErrors({Error(6, 156), Error(6, 118), Error(9, 13)});
    EVP_PKEY_free_nid_postfix(pkey);

    old->references = 2;
    LibreSslRsa* target = old;
    rsa = ReadFile(Pkcs8Key, &target);
    Require(rsa == target && rsa != old && old->references == 1);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    RSA_free_nid_postfix(old);
    RequireErrors({});
}

struct PasswordSource {
    const char* text;
    int calls;
};

int APS5_VABI SupplyPassword(char* buffer, int size, int, void* userData) {
    auto* source = static_cast<PasswordSource*>(userData);
    ++source->calls;
    int length = 0;
    while (length < size && source->text[length]) {
        buffer[length] = source->text[length];
        ++length;
    }
    return length;
}

int APS5_VABI RejectPassword(char*, int, int, void*) { return 0; }

int APS5_VABI NegativePassword(char*, int, int, void*) { return -1; }

int APS5_VABI OversizedPassword(char* buffer, int size, int, void*) {
    buffer[0] = 'x';
    return size + 1;
}

void RequireDecryptedRsaKey(void* key, int bits = 2048, std::source_location location = std::source_location::current()) {
    Require(key != nullptr && EVP_PKEY_id_nid_postfix(key) == PkeyRsa, location);
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(key);
    Require(rsa != nullptr && BN_num_bits_nid_postfix(static_cast<const Bignum*>(rsa->n)) == bits, location);
    unsigned char signature[256];
    unsigned int length = 0;
    Require(RSA_sign_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, &length, rsa) == 1, location);
    Require(RSA_verify_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, length, rsa) == 1, location);
    RSA_free_nid_postfix(rsa);
}

void RequireEncryptedRsa(const std::string& text, const char* pass, int bits = 2048, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    PasswordSource supplied {pass, 0};
    void* key = ReadBioWithPass(text, &SupplyPassword, &supplied);
    RequireDecryptedRsaKey(key, bits, location);
    Require(supplied.calls == 1, location);
    RequireErrors({}, location);
    EVP_PKEY_free_nid_postfix(key);

    ERR_clear_error_nid_postfix();
    key = ReadBioWithPass(text, nullptr, const_cast<char*>(pass));
    RequireDecryptedRsaKey(key, bits, location);
    RequireErrors({}, location);
    EVP_PKEY_free_nid_postfix(key);

    ERR_clear_error_nid_postfix();
    PasswordSource wrong {"wrongpass", 0};
    Require(ReadBioWithPass(text, &SupplyPassword, &wrong) == nullptr, location);
    Require(wrong.calls == 1, location);
    RequireErrors({Error(6, 100), Error(35, 116), Error(35, 117), Error(9, 13)}, location);

    ERR_clear_error_nid_postfix();
    Require(ReadBioWithPass(text, nullptr, const_cast<char*>("wrongpass")) == nullptr, location);
    RequireErrors({Error(6, 100), Error(35, 116), Error(35, 117), Error(9, 13)}, location);
}

void TestEncryptedPkcs8() {
    RequireEncryptedRsa(RsaAes128CbcSha1Key, "hunter2");
    RequireEncryptedRsa(RsaAes256CbcSha1Key, "secret");
    RequireEncryptedRsa(RsaDesEde3CbcKey, "despass");
    RequireEncryptedRsa(RsaPbeSha1ThreeDesKey, "pbe1pass");
    RequireEncryptedRsa(RsaPbes2Aes256HmacSha256Key, "sha256pass");
    for (const char* fixture : {Aes192Key, DesCbcKey, Sha224Key, Sha384Key, Sha512Key,
                                PbeMd5DesKey, PbeSha1DesKey, SmallPbeSha1ThreeDesKey})
        RequireEncryptedRsa(fixture, "fixture", 512);

    const struct { const char* fixture; std::initializer_list<std::uint64_t> errors; } failures[] {
        {UnknownPbeKey, {Error(6, 121), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {InvalidPbeParameterKey, {Error(6, 114), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {UnsupportedKdfKey, {Error(6, 124), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {UnsupportedCipherKey, {Error(6, 107), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {UnsupportedPrfKey, {Error(6, 125), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {InvalidKdfParameterKey, {Error(6, 114), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {InvalidCipherParameterKey, {Error(13, 109), Error(6, 122), Error(6, 120), Error(35, 115), Error(35, 117), Error(9, 13)}},
        {DecodeFailureKey, {Error(13, 168), Error(13, 58), Error(35, 101), Error(9, 13)}}
    };
    for (const auto& failure : failures) {
        ERR_clear_error_nid_postfix();
        Require(ReadBioWithPass(failure.fixture, nullptr, const_cast<char*>("fixture")) == nullptr);
        RequireErrors(failure.errors);
    }


    ERR_clear_error_nid_postfix();
    PasswordSource ecWrong {"wrongpass", 0};
    Require(ReadBioWithPass(EcAes128CbcSha1Key, &SupplyPassword, &ecWrong) == nullptr);
    Require(ecWrong.calls == 1);
    RequireErrors({Error(6, 100), Error(35, 116), Error(35, 117), Error(9, 13)});

    ERR_clear_error_nid_postfix();
    PasswordSource ecGood {"ecpass", 0};
    RequireThrows([&] { ReadBioWithPass(EcAes128CbcSha1Key, &SupplyPassword, &ecGood); });
    Require(ecGood.calls == 1);
    ERR_clear_error_nid_postfix();

    ERR_clear_error_nid_postfix();
    Require(ReadBioWithPass(RsaAes128CbcSha1Key, &RejectPassword, nullptr) == nullptr);
    RequireErrors({Error(9, 104)});
    ERR_clear_error_nid_postfix();
    Require(ReadBioWithPass(RsaAes128CbcSha1Key, &NegativePassword, nullptr) == nullptr);
    RequireErrors({Error(9, 104)});
    ERR_clear_error_nid_postfix();
    Require(ReadBioWithPass(RsaAes128CbcSha1Key, &OversizedPassword, nullptr) == nullptr);
    RequireErrors({Error(9, 104)});

    ERR_clear_error_nid_postfix();
    Require(ReadBio(Relabel(UnknownPkcs8Key, "PRIVATE KEY", "ENCRYPTED PRIVATE KEY")) == nullptr);
    RequireErrors({Error(9, 13)});

    ERR_clear_error_nid_postfix();
    auto* oldRsa = RSA_new_nid_postfix();
    Require(oldRsa != nullptr);
    void* old = EVP_PKEY_new_nid_postfix();
    Require(old != nullptr && EVP_PKEY_set1_RSA_nid_postfix(old, oldRsa) == 1 && oldRsa->references == 2);
    void* slot = old;
    PasswordSource supplied {"hunter2", 0};
    void* key = ReadBioWithPass(RsaAes128CbcSha1Key, &SupplyPassword, &supplied, &slot);
    Require(key != nullptr && key != old && slot == key && oldRsa->references == 1 && supplied.calls == 1);
    RequireDecryptedRsaKey(key);
    RequireErrors({});
    EVP_PKEY_free_nid_postfix(key);
    RSA_free_nid_postfix(oldRsa);

    ERR_clear_error_nid_postfix();
    auto* preservedRsa = RSA_new_nid_postfix();
    Require(preservedRsa != nullptr);
    void* preserved = EVP_PKEY_new_nid_postfix();
    Require(preserved != nullptr && EVP_PKEY_set1_RSA_nid_postfix(preserved, preservedRsa) == 1 && preservedRsa->references == 2);
    slot = preserved;
    PasswordSource wrong {"wrongpass", 0};
    Require(ReadBioWithPass(RsaAes128CbcSha1Key, &SupplyPassword, &wrong, &slot) == nullptr && slot == preserved &&
            preservedRsa->references == 2 && wrong.calls == 1);
    RequireErrors({Error(6, 100), Error(35, 116), Error(35, 117), Error(9, 13)});
    EVP_PKEY_free_nid_postfix(preserved);
    RSA_free_nid_postfix(preservedRsa);
}

void TestErrors() {
    RequireBioFailure("no PEM here\n", {Error(9, 108)});
    RequireBioFailure(Relabel(RsaKey, "RSA PRIVATE KEY", "DH PRIVATE KEY"), {Error(9, 108)});
    RequireBioFailure(Certificate, {Error(9, 108)});
    RequireBioFailure(Replace(RsaKey, "END RSA PRIVATE", "END PRIVATE"), {Error(9, 102)});
    RequireBioFailure(Replace(RsaKey, "END RSA PRIVATE", "END DSA PRIVATE"), {Error(9, 102)});
    RequireBioFailure(Replace(RsaKey, "Lldg=\n", "Lldg=AAAA\n"), {Error(9, 100)});
    RequireBioFailure(Replace(RsaKey, "m9zhglUBoQst4upwxOYfgar9ZmtI5HHu+pdzor6Lldg=\n", "AAA\n"), {Error(9, 100)});
    RequireBioFailure(Replace(RsaKey, "MIIBPAIB", "MIIB!AIB"), {});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nComment: x\n\n"), {Error(9, 107)});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nProc-Type: 4,MIC-ONLY\n\n"), {Error(9, 106)});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nProc-Type: 4,ENCRYPTED\nComment: x\n\n"), {Error(9, 105)});
    RequireBioFailure(Relabel(RsaKey, "RSA PRIVATE KEY", "ANY PRIVATE KEY"), {Error(9, 13)});
    RequireBioFailure(UnknownPkcs8Key, {Error(6, 156), Error(6, 118), Error(9, 13)});

    ERR_clear_error_nid_postfix();
    Require(ReadFile("no PEM here\n") == nullptr);
    RequireErrors({Error(9, 108)});
    Require(PEM_read_RSAPrivateKey_nid_postfix(nullptr, nullptr, nullptr, nullptr) == nullptr);
    RequireErrors({Error(9, 7)});

    RequireThrows([] { ReadBio(EncryptedRsaKey); });
    Require(ReadBio(EncryptedPkcs8Key) == nullptr);
    RequireErrors({Error(9, 104)});
    RequireThrows([] { ReadBio(EcKey); });
    RequireThrows([] { ReadBio(EcPkcs8Key); });
    RequireThrows([] { ReadFile(EcKey); });
    TestEncryptedPkcs8();
}

void TestCertificate() {
    const std::string text = std::string(RsaKey) + Certificate;
    LibreSslX509* certificate = nullptr;
    auto* bio = BIO_new_mem_buf_nid_postfix(text.data(), static_cast<int>(text.size()));
    RequireThrows([&] { PEM_read_bio_X509_nid_postfix(bio, &certificate, nullptr, nullptr); });
    BIO_free_nid_postfix(bio);
    bio = BIO_new_mem_buf_nid_postfix(RsaKey, static_cast<int>(sizeof(RsaKey) - 1));
    ERR_clear_error_nid_postfix();
    Require(PEM_read_bio_X509_nid_postfix(bio, &certificate, nullptr, nullptr) == nullptr && certificate == nullptr);
    RequireErrors({Error(9, 108)});
    BIO_free_nid_postfix(bio);
}

}

int main() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
    TestKeys();
    TestReuse();
    TestErrors();
    TestCertificate();
    std::puts("libSceLibreSsl PEM private key tests passed");
}
