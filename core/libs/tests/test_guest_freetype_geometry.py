import itertools
import random
import subprocess
import sys


def signed(value, bits=64):
    return (value + (1 << (bits - 1))) % (1 << bits) - (1 << (bits - 1))


def multiply(a, b):
    product = signed(a, 32) * signed(b, 32)
    magnitude = (abs(product) + 32768) // 65536
    return signed(-magnitude if product < 0 else magnitude, 32)


def matrix_product(a, b):
    return tuple(sum(multiply(a[row * 2 + k], b[k * 2 + col]) for k in range(2))
                 for row in range(2) for col in range(2))


def invert(matrix):
    xx, xy, yx, yy = matrix
    determinant = multiply(xx, yy) - multiply(xy, yx)
    if determinant == 0:
        return (6, *matrix)
    def divide(value):
        numerator = (abs(value) * 65536 + abs(determinant) // 2) % (1 << 64)
        quotient = numerator // abs(determinant)
        return -quotient if (value < 0) != (determinant < 0) else quotient
    return (0, signed(divide(yy)), signed(-divide(xy)), signed(-divide(yx)), signed(divide(xx)))


def oracle(case):
    x, y, *components = case
    a, b = components[:4], components[4:]
    vector = tuple(multiply(x, a[row * 2]) + multiply(y, a[row * 2 + 1]) for row in range(2))
    return (*vector, *matrix_product(a, b), *matrix_product(a, a), *invert(a))


def main():
    identity = (65536, 0, 0, 65536)
    assert matrix_product((65536, 131072, 196608, 262144), (327680, 393216, 458752, 524288)) == (1245184, 1441792, 2818048, 3276800)
    assert invert((131072, 0, 0, 32768)) == (0, 32768, 0, 0, 131072)
    assert invert((0, -65536, 65536, 0)) == (0, 0, 65536, -65536, 0)
    assert invert((1, 0, 0, 1)) == (6, 1, 0, 0, 1)
    assert multiply(-1, 32768) == -1 and multiply(1, 32768) == 1
    boundary = (-(1 << 63), -(1 << 63) + 1, -(1 << 32) - 1, -(1 << 31), -65536, -32768,
                -1, 0, 1, 32768, 65536, (1 << 31) - 1, 1 << 31, (1 << 32) + 1, (1 << 63) - 1)
    cases = [(xx, yy, xx, xy, yx, yy, yy, yx, xy, xx) for xx, xy, yx, yy in itertools.product(boundary, repeat=4)]
    for a, b in itertools.product(boundary, repeat=2):
        cases.extend(((a, b, a, b, a, b, *identity), (a, b, 1, a, 0, 65536, *identity),
                      (a, b, 65536, a, 65536, b, *identity)))
    for fraction in range(65536):
        cases.append((fraction, -fraction, 32768, -32768, 65536, 65536, *identity))
    rng = random.Random(0xF7CE0133)
    for bits in (32, 64):
        cases.extend(tuple(signed(rng.getrandbits(bits), bits) for _ in range(10)) for _ in range(10000))
    for matrix in (identity, (0, -65536, 65536, 0), (0, 65536, 65536, 0), (131072, 0, 0, 32768),
                   (65536, 32768, 0, 65536), (0, 0, 0, 0), (1, 0, 0, 1), (65536, 65536, 65536, 65537)):
        cases.extend((x, y, *matrix, *identity) for x, y in itertools.product(boundary, repeat=2))
    data = ''.join(' '.join(map(str, case)) + '\n' for case in cases)
    result = subprocess.run([sys.argv[1]], input=data, text=True, capture_output=True, check=True, timeout=25)
    _, marker, payload = result.stdout.partition('FT_GEOMETRY ')
    if not marker:
        raise AssertionError(result.stdout)
    lines = payload.splitlines()
    width = int(lines.pop(0))
    if width not in (4, 8) or len(lines) != len(cases):
        raise AssertionError((width, len(lines), len(cases)))
    for case, line in zip(cases, lines):
        actual = tuple(map(int, line.split()))
        expected = oracle(case)
        if actual != expected * (2 if width == 8 else 1):
            raise AssertionError((case, actual, expected))
    print(f'{len(cases)} cases: vector transform, matrix multiply, aliased multiply and inversion match the arbitrary-precision oracle')
    if width == 8:
        print('All results also match bundled native LP64 FreeType')


if __name__ == '__main__':
    main()
