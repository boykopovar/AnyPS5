import itertools
import random
import subprocess
import sys


MODULUS = 1 << 64
SCALE = 1 << 16
NAMES = ("FT_MulDiv", "FT_MulFix", "FT_DivFix", "FT_RoundFix", "FT_CeilFix", "FT_FloorFix")


def signed(value, bits=64):
    return (value + (1 << (bits - 1))) % (1 << bits) - (1 << (bits - 1))


def rounded_ratio(numerator, denominator):
    quotient, remainder = divmod(abs(numerator), denominator)
    if 2 * remainder >= denominator:
        quotient += 1
    return -quotient if numerator < 0 else quotient


def oracle(a, b, c):
    if c:
        numerator = (abs(a) * abs(b) + abs(c) // 2) % MODULUS
        muldiv = numerator // abs(c)
    else:
        muldiv = (1 << 31) - 1
    if (a < 0) + (b < 0) + (c < 0) in (1, 3):
        muldiv = -muldiv
    mulfix = signed(rounded_ratio(signed(a, 32) * signed(b, 32), SCALE), 32)
    if b:
        numerator = (abs(a) * SCALE + abs(b) // 2) % MODULUS
        divfix = numerator // abs(b)
    else:
        divfix = (1 << 31) - 1
    if (a < 0) != (b < 0):
        divfix = -divfix
    rounded = rounded_ratio(a, SCALE) * SCALE
    ceiling = -((-a) // SCALE) * SCALE
    floor = (a // SCALE) * SCALE
    return tuple(signed(value) for value in (muldiv, mulfix, divfix, rounded, ceiling, floor))


def main():
    boundaries = sorted({
        -(1 << 63), -(1 << 63) + 1, -(1 << 48), -(1 << 32) - 1, -(1 << 32),
        -(1 << 31) - 1, -(1 << 31), -(1 << 31) + 1, -SCALE - 1, -SCALE,
        -SCALE + 1, -32769, -32768, -32767, -3, -2, -1, 0, 1, 2, 3,
        32767, 32768, 32769, SCALE - 1, SCALE, SCALE + 1, (1 << 31) - 1,
        1 << 31, (1 << 31) + 1, (1 << 32) - 1, 1 << 32, (1 << 32) + 1,
        1 << 48, (1 << 63) - SCALE, (1 << 63) - 32769, (1 << 63) - 32768,
        (1 << 63) - 1,
    })
    cases = list(itertools.product(boundaries, repeat=3))
    for anchor in (-(1 << 63), -2 * SCALE, -SCALE, 0, SCALE, (1 << 63) - SCALE):
        cases.extend((anchor + fraction, 1, 2) for fraction in range(SCALE))
    rng = random.Random(0xF7A2133)
    for bits in (32, 64):
        cases.extend(tuple(signed(rng.getrandbits(bits), bits) for _ in range(3)) for _ in range(10000))
    for a, b in itertools.product(boundaries, repeat=2):
        cases.extend(((a, b, 0), (a, 0, b)))
    known = {
        (1, 32768, 2): (16384, 1, 2, 0, SCALE, 0),
        (-32768, SCALE, 1): (-2147483648, -32768, -32768, -SCALE, 0, -SCALE),
        (0, 0, 0): (2147483647, 0, 2147483647, 0, 0, 0),
        (-(1 << 63), -1, 1): (-(1 << 63), 0, 0, -(1 << 63), -(1 << 63), -(1 << 63)),
        ((1 << 63) - 1, 1, 1): ((1 << 63) - 1, 0, -SCALE, -(1 << 63), -(1 << 63), (1 << 63) - SCALE),
        ((1 << 32) + SCALE, SCALE, 1): ((1 << 48) + (1 << 32), SCALE, (1 << 32) + SCALE,
                                                  (1 << 32) + SCALE, (1 << 32) + SCALE, (1 << 32) + SCALE),
    }
    for args, expected in known.items():
        if oracle(*args) != expected:
            raise AssertionError((args, oracle(*args), expected))
    cases.extend(known)
    data = "".join(f"{a} {b} {c}\n" for a, b, c in cases)
    result = subprocess.run([sys.argv[1]], input=data, text=True, capture_output=True, check=True, timeout=25)
    preamble, marker, payload = result.stdout.partition("FT_ARITHMETIC ")
    if not marker:
        raise AssertionError(result.stdout)
    lines = payload.splitlines()
    host_width = int(lines.pop(0))
    if host_width not in (4, 8) or len(lines) != len(cases):
        raise AssertionError((host_width, len(lines), len(cases)))
    for args, line in zip(cases, lines):
        actual = tuple(map(int, line.split()))
        expected = oracle(*args)
        if len(actual) != (12 if host_width == 8 else 6):
            raise AssertionError((args, actual))
        for index, value in enumerate(actual):
            if value != expected[index % 6]:
                source = "guest" if index < 6 else "native LP64"
                raise AssertionError((source, NAMES[index % 6], args, value, expected[index % 6]))
    print(f"{len(cases)} input triples; {len(cases) * 6} guest results match the arbitrary-precision oracle")
    if host_width == 8:
        print("All results also match bundled native LP64 FreeType")


if __name__ == "__main__":
    main()
