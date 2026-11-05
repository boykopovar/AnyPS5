import hashlib
SUFFIX = bytes([0x51,0x8D,0x64,0xA6,0x35,0xDE,0xD8,0xC1,0xE6,0xB0,0x39,0xB1,0xC3,0xE5,0x52,0x30])
B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"
def nid(name):
    d = hashlib.sha1(name.encode() + SUFFIX).digest()
    r = d[:8][::-1]
    out = ""
    for i in (0, 3):
        t = (r[i] << 16) | (r[i+1] << 8) | r[i+2]
        out += B64[(t >> 18) & 63] + B64[(t >> 12) & 63] + B64[(t >> 6) & 63] + B64[t & 63]
    t = (r[6] << 16) | (r[7] << 8)
    out += B64[(t >> 18) & 63] + B64[(t >> 12) & 63] + B64[(t >> 6) & 63]
    return out
if __name__ == "__main__":
    import sys
    for n in sys.argv[1:]: print(n, nid(n))
