import argparse
import ctypes
import hashlib
import itertools
import json
import random
import sys
from pathlib import Path


RELEASE_URL = "https://github.com/libressl/portable/releases/download/v4.0.0/libressl-4.0.0.tar.gz"
RELEASE_SHA256 = "4d841955f0acc3dfc71d0e3dd35f283af461222350e26843fea9731c0246a1e4"
VERSION = "LibreSSL 4.0.0"


def bind(library, name, result, *arguments):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = list(arguments)
    return function


class Oracle:
    def __init__(self, path):
        self.library = ctypes.CDLL(str(path))
        self.version = bind(self.library, "OpenSSL_version", ctypes.c_char_p, ctypes.c_int)(0).decode("ascii")
        if self.version != VERSION:
            raise ValueError(f"expected {VERSION}, received {self.version}")
        self.convert = bind(self.library, "OBJ_txt2obj", ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.length = bind(self.library, "OBJ_length", ctypes.c_size_t, ctypes.c_void_p)
        self.data = bind(self.library, "OBJ_get0_data", ctypes.c_void_p, ctypes.c_void_p)
        self.free = bind(self.library, "ASN1_OBJECT_free", None, ctypes.c_void_p)
        self.error = bind(self.library, "ERR_get_error", ctypes.c_ulong)
        self.error_string = bind(self.library, "ERR_error_string_n", None,
                                 ctypes.c_ulong, ctypes.c_void_p, ctypes.c_size_t)
        self.a2d = bind(self.library, "a2d_ASN1_OBJECT", ctypes.c_int,
                        ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int)

    def errors(self):
        result = []
        while code := self.error():
            buffer = ctypes.create_string_buffer(256)
            self.error_string(code, buffer, len(buffer))
            result.append({"code": code, "hex": f"{code:08x}", "text": buffer.value.decode("ascii")})
        return result

    def observe(self, raw, no_name):
        self.errors()
        object_pointer = self.convert(raw, no_name)
        result = {"success": object_pointer is not None, "errors": self.errors()}
        if object_pointer is not None:
            try:
                length = self.length(object_pointer)
                data = self.data(object_pointer)
                result.update({"length": length, "data_is_null": data is None,
                               "der_hex": ctypes.string_at(data, length).hex() if data else None})
            finally:
                self.free(object_pointer)
        return result

    def observe_a2d(self, raw):
        raw = raw.split(b"\0", 1)[0]
        buffer = ctypes.create_string_buffer(len(raw) + 16)
        self.errors()
        length = self.a2d(buffer, len(buffer), raw, len(raw))
        errors = self.errors()
        count = self.a2d(None, 0, raw, len(raw))
        return {"length": length, "der_hex": buffer.raw[:length].hex() if length > 0 else None,
                "errors": errors, "count_only_length": count, "count_only_errors": self.errors()}

    def observe_case(self, text):
        raw = text.encode("ascii")
        result = self.observe(raw, 1)
        result = {"text": text, "no_name": 1, **result,
                  "negative_no_name_result": self.observe(raw, -1), "a2d": self.observe_a2d(raw)}
        if raw and ord("0") <= raw[0] <= ord("9"):
            result["zero_no_name_result"] = self.observe(raw, 0)
        return result


def cases():
    inputs = {"", "0", "1", "2", "3", ".", " ", "0.0", "0.39", "0.40", "1.0", "1.39", "1.40",
              "2.0", "2.40", "2.999", "3.1", "1.2.3", "1 2 3", "1.2 3", "1 2.3", "1..2",
              "1  2", "1.2.", ".1.2", "1.2 ", " 1.2", "1\t2", "1.2\n", "+1.2", "-1.2",
              "1.-2", "1.+2", "a.2", "1.a", "1.2a", "01.2", "001.2", "1.02", "1.002",
              "1.00", "1.2.0", "1.2.00", "1.2.01", "1.2.001", "0.18446744073709551615",
              "1.18446744073709551615", "2.18446744073709551535", "2.18446744073709551536",
              "2.18446744073709551615", "2.18446744073709551616", "1.2.18446744073709551615",
              "1.2.18446744073709551616", "1.2.18446744073709551619", "1.2.18446744073709551620",
              "1.2.184467440737095516159", "1.2.999999999999999999999999", "3.", "3.a", "3..",
              "3.40", "3.00", "3 00", "3 40", "00", "03", "03.", "18446744073709551620",
              "1.40a", "3a.1", "1.40.00", "2.18446744073709551536.a", "1.2.3\0ignored",
              "1\x002", "1.2\0.3", "1.2." + "9" * 1000, "1.2" + ".0" * 1000}
    for first, separator, second, tail in itertools.product(
            ["", "0", "1", "2", "3", "a", "00", "03", "18446744073709551616", "18446744073709551620"],
            [".", " "], ["", "0", "1", "39", "40", "a", "00", "01", "18446744073709551616"],
            ["", ".", ".a", " a"]):
        inputs.add(first + separator + second + tail)
    for arc in ["0", "00", "01", "09", "010", "0001", "0010", "10", "18446744073709551610",
                "18446744073709551615", "18446744073709551616", "18446744073709551617",
                "18446744073709551618", "18446744073709551619", "18446744073709551620",
                "184467440737095516160", "184467440737095516161", "1844674407370955161600"]:
        inputs.update([arc, arc + ".1", "1." + arc, "2." + arc, "1.2." + arc, "1.2." + arc + ".3"])
    for separator in [".", " ", "\t", "\n", "\r", "\v", "\f", ",", "/", ":", "+", "-", "_"]:
        inputs.update(["1" + separator + "2", "1.2" + separator + "3", "1 2" + separator + "3"])
    rng = random.Random(4000)
    for _ in range(3000):
        inputs.add("".join(rng.choice("0123456789. a+-\t") for _ in range(rng.randrange(0, 25))))
    for arc in [0, 1, 39, 40, 79, 80, 127, 128, 129, 255, 256, 16383, 16384, 2097151, 2097152,
                (1 << 32) - 1, 1 << 32, (1 << 63) - 1, 1 << 63, (1 << 64) - 1]:
        inputs.update(["1.2." + str(arc), "2." + str(arc)])
    for _ in range(1000):
        first = rng.randrange(3)
        second = rng.randrange(40 if first < 2 else (1 << 64) - 80)
        tail = [rng.randrange(1 << 64) for _ in range(rng.randrange(5))]
        inputs.add(rng.choice([".", " "]).join(map(str, [first, second, *tail])))
    return sorted(inputs)


def main():
    parser = argparse.ArgumentParser(description="Observe numeric OID parsing through official LibreSSL 4.0.0 public APIs.")
    parser.add_argument("--library", required=True, type=Path, help="path to the independently built LibreSSL 4.0.0 shared libcrypto")
    parser.add_argument("--output", type=Path, help="write JSON here instead of stdout")
    parser.add_argument("--text", action="append", help="observe this input instead of the generated corpus; may be repeated")
    args = parser.parse_args()
    try:
        library_path = args.library.resolve(strict=True)
        library_sha256 = hashlib.sha256(library_path.read_bytes()).hexdigest()
        oracle = Oracle(library_path)
        result = {"provenance": {"reference_release": {"url": RELEASE_URL, "sha256": RELEASE_SHA256},
                                 "observed_library": {"sha256": library_sha256, "reported_version": oracle.version},
                                 "source_archive_verified_by_this_tool": False,
                                 "method": "public exported functions; opaque ASN1_OBJECT; numeric inputs only"},
                  "cases": [oracle.observe_case(text) for text in (args.text if args.text is not None else cases())]}
        output = json.dumps(result, indent=2) + "\n"
        if args.output:
            args.output.write_text(output, encoding="utf-8")
        else:
            sys.stdout.write(output)
    except (OSError, AttributeError, UnicodeError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
