from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture as optional_plt_fixture


TAG_PAIRS = ((5, 0x61000035), (10, 0x61000037), (6, 0x61000039),
             (11, 0x6100003b), (7, 0x6100002f), (8, 0x61000031),
             (9, 0x61000033), (3, 0x61000027), (2, 0x6100002d),
             (20, 0x6100002b), (23, 0x61000029))


def tags_for(os_tags):
    values = (0x600, 1, 0x620, 24, 0x700, 24, 24, 0x300, 0, 7, 0x720)
    return [(pair[1 if os_tags else 0], value) for pair, value in zip(TAG_PAIRS, values)]


def fixture(tags, after_null=()):
    image = optional_plt_fixture()
    struct.pack_into("<H", image, 0x38, 5)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     0x61000000, 0, 0, 0, 0, len(image), len(image), 1)
    for index in (3, 4):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56,
                         0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    entries = [*tags, (0, 0), *after_null]
    struct.pack_into("<QQ", image, 120 + 32, len(entries) * 16, len(entries) * 16)
    for index, entry in enumerate(entries):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *entry)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix="anyps5-dynamic-tags-") as directory:
        work = Path(directory)

        def convert(name, image, error=None, needed=None):
            source = work / (name + ".elf")
            source.write_bytes(image)
            for mode in ([], ["--windows"]):
                output = work / (name + (".exe" if mode else ".out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                if error is not None:
                    valid = (result.returncode == 2 and error in result.stderr and not output.exists()
                             and not output.with_suffix(".registry.json").exists())
                else:
                    magic = b"MZ" if mode else b"\x7fELF"
                    valid = (result.returncode == 0 and output.exists() and output.read_bytes().startswith(magic)
                             and output.with_suffix(".registry.json").exists())
                    if valid and needed is not None and not mode:
                        data = output.read_bytes()
                        phoff = struct.unpack_from("<Q", data, 32)[0]
                        phsize, phcount = struct.unpack_from("<HH", data, 54)
                        dynamic = next(struct.unpack_from("<IIQQQQQQ", data, phoff + index * phsize)
                                       for index in range(phcount)
                                       if struct.unpack_from("<I", data, phoff + index * phsize)[0] == 2)
                        entries = [struct.unpack_from("<qQ", data, offset)
                                   for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
                        strings = next(value for tag, value in entries if tag == 5)
                        string_offset = next(header[2] + strings - header[3]
                                             for index in range(phcount)
                                             for header in [struct.unpack_from("<IIQQQQQQ", data, phoff + index * phsize)]
                                             if header[0] == 1 and header[3] <= strings < header[3] + header[5])
                        names = [data[string_offset + value:data.index(0, string_offset + value)].decode()
                                 for tag, value in entries if tag == 1]
                        valid = names == needed
                if not valid:
                    failures.append((name, mode, result.returncode, result.stdout, result.stderr))

        for os_tags in (False, True):
            family = "os" if os_tags else "sysv"
            tags = tags_for(os_tags)
            convert(family + "-valid", fixture(tags))
            for tag, value in tags:
                error = "Duplicate dynamic tag " + str(tag)
                convert(f"{family}-{tag:x}-same", fixture([*tags, (tag, value)]), error)
                convert(f"{family}-{tag:x}-different-last", fixture([*tags, (tag, value + 1)]), error)
                convert(f"{family}-{tag:x}-different-first", fixture([(tag, value + 1), *tags]), error)
            convert(family + "-after-null", fixture(tags, [(tag, value + 1) for tag, value in tags]))

            strings = b"\0libA.so\0libB.so\0"
            repeated = [(tag, len(strings) if tag == (0x61000037 if os_tags else 10) else value)
                        for tag, value in tags]
            repeated.extend([(1, 1), (1, 9), (0x61000045, (1 << 48) | 1),
                             (0x61000045, (2 << 48) | 9), (0x70000001, 1), (0x70000001, 2)])
            image = fixture(repeated)
            image[0x600:0x600 + len(strings)] = strings
            convert(family + "-repeatable", image, needed=["libA.so", "libB.so"])

            reverse = list(repeated)
            reverse[len(tags):len(tags) + 2] = [(1, 9), (1, 1)]
            image = fixture(reverse)
            image[0x600:0x600 + len(strings)] = strings
            convert(family + "-repeatable-reverse", image, needed=["libB.so", "libA.so"])

    if failures:
        raise AssertionError(failures)
    print("Dynamic tag duplicate tests passed")


if __name__ == "__main__":
    main()
