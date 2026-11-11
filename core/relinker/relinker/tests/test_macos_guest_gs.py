"""Run guest GS accesses rewritten by `relinker --macos` against a reference without GS."""
from pathlib import Path
import os
import platform
import struct
import subprocess
import sys
import tempfile

from test_macos_executable_exports import executable
from test_macos_output import MachO

CODE = 0x4000
LIMIT = 0x4600
BUFFER = 256
OUT = 168
FLAGS = (0x8d7, 0x202)
STACK_BASE = 8

CASES = (
    ("load64-absolute", "65 48 8b 04 25 10 00 00 00", "49 8b 86 10 00 00 00", 14, {14: 0}),
    ("load32-absolute-r9d", "65 44 8b 0c 25 18 00 00 00", "45 8b 8e 18 00 00 00", 14, {14: 0}),
    ("load64-negative-disp8", "65 48 8b 43 f8", "48 8b 43 f8", 3, {3: 0x20}),
    ("load16-base-disp8", "66 65 8b 53 20", "66 8b 53 20", 3, {3: 8}),
    ("load8-sib-index", "65 44 8a 4c b3 08", "44 8a 4c b3 08", 3, {3: 0x10, 6: 3}),
    ("load8-bh", "65 8a 3c 25 30 00 00 00", "8a b9 30 00 00 00", 1, {1: 0}),
    ("load64-r13-r12", "65 4b 8b 44 a5 08", "4b 8b 44 a5 08", 13, {13: 0x10, 12: 2}),
    ("store64-rdi", "65 48 89 3c 25 40 00 00 00", "49 89 be 40 00 00 00", 14, {14: 0}),
    ("store32-immediate", "65 c7 43 44 44 33 22 11", "c7 43 44 44 33 22 11", 3, {3: 8}),
    ("store16-rbp", "66 65 44 89 55 50", "66 44 89 55 50", 5, {5: 8}),
    ("store8-sib", "65 88 84 1e 58 00 00 00", "88 84 1e 58 00 00 00", 6, {6: 8, 3: 4}),
    ("add-read-absolute", "65 4c 03 04 25 60 00 00 00", "4d 03 86 60 00 00 00", 14, {14: 0}),
    ("cmp-read-base", "65 48 3b 43 08", "48 3b 43 08", 3, {3: 0x20}),
    ("add-store-base-disp32", "65 01 8b 10 00 00 00", "01 8b 10 00 00 00", 3, {3: 8}),
    ("lock-xadd", "f0 65 48 0f c1 04 25 70 00 00 00", "f0 49 0f c1 86 70 00 00 00", 14, {14: 0}),
    ("inc-qword", "65 48 ff 04 25 78 00 00 00", "49 ff 86 78 00 00 00", 14, {14: 0}),
    ("moffs-load", "65 48 a1 80 00 00 00 00 00 00 00", "49 8b 86 80 00 00 00", 14, {14: 0}),
    ("movzx-byte", "65 0f b6 0c 25 88 00 00 00", "41 0f b6 8e 88 00 00 00", 14, {14: 0}),
    ("cmove", "65 48 0f 44 0c 25 b0 00 00 00", "49 0f 44 8e b0 00 00 00", 14, {14: 0}),
    ("mul-qword", "65 48 f7 24 25 b8 00 00 00", "49 f7 a6 b8 00 00 00", 14, {14: 0}),
    ("movdqu-load", "65 f3 0f 6f 04 25 90 00 00 00", "f3 41 0f 6f 86 90 00 00 00", 14, {14: 0}),
    ("movq-store", "65 66 0f d6 0c 25 a0 00 00 00", "66 41 0f d6 8e a0 00 00 00", 14, {14: 0}),
    ("load64-rsp", "65 48 8b 04 24", "48 8b 44 24 08", None, {}),
    ("load64-negative-disp32-index", "65 48 8b 84 8b f0 ff ff ff", "48 8b 84 8b f0 ff ff ff", 3, {3: 0x40, 1: 4}),
    ("load64-index-only", "65 48 8b 04 cd 08 00 00 00", "49 8b 84 ce 08 00 00 00", 14, {14: 0, 1: 3}),
    ("adc-read-carry", "65 48 13 04 25 58 00 00 00", "49 13 86 58 00 00 00", 14, {14: 0}),
    ("sbb-store", "65 48 19 0c 25 60 00 00 00", "49 19 8e 60 00 00 00", 14, {14: 0}),
    ("and-byte-immediate", "65 80 24 25 68 00 00 00 0f", "41 80 a6 68 00 00 00 0f", 14, {14: 0}),
    ("sub-qword-immediate8", "65 48 83 2c 25 70 00 00 00 ff", "49 83 ae 70 00 00 00 ff", 14, {14: 0}),
    ("cmp-dword-immediate32", "65 81 3c 25 78 00 00 00 44 33 22 11", "41 81 be 78 00 00 00 44 33 22 11", 14, {14: 0}),
    ("mov-byte-immediate", "65 c6 04 25 80 00 00 00 7f", "41 c6 86 80 00 00 00 7f", 14, {14: 0}),
    ("mov-word-immediate", "66 65 c7 04 25 88 00 00 00 34 12", "66 41 c7 86 88 00 00 00 34 12", 14, {14: 0}),
    ("shl-qword-immediate", "65 48 c1 24 25 c0 00 00 00 05", "49 c1 a6 c0 00 00 00 05", 14, {14: 0}),
    ("sar-dword-cl", "65 d3 3c 25 c8 00 00 00", "41 d3 be c8 00 00 00", 14, {14: 0}),
    ("shr-byte-one", "65 d0 2c 25 d0 00 00 00", "41 d0 ae d0 00 00 00", 14, {14: 0}),
    ("test-byte-immediate", "65 f6 04 25 d8 00 00 00 81", "41 f6 86 d8 00 00 00 81", 14, {14: 0}),
    ("test-ah", "65 84 24 25 e0 00 00 00", "84 a1 e0 00 00 00", 1, {1: 0}),
    ("not-qword", "65 48 f7 14 25 e0 00 00 00", "49 f7 96 e0 00 00 00", 14, {14: 0}),
    ("neg-word", "66 65 f7 1c 25 e8 00 00 00", "66 41 f7 9e e8 00 00 00", 14, {14: 0}),
    ("div-dword", "65 f7 34 25 f0 00 00 00", "41 f7 b6 f0 00 00 00", 14, {14: 0, 2: 0}),
    ("idiv-qword", "65 48 f7 3c 25 f0 00 00 00", "49 f7 be f0 00 00 00", 14, {14: 0, 2: 0}),
    ("imul-qword", "65 48 f7 2c 25 a8 00 00 00", "49 f7 ae a8 00 00 00", 14, {14: 0}),
    ("imul-register", "65 4c 0f af 14 25 d0 00 00 00", "4d 0f af 96 d0 00 00 00", 14, {14: 0}),
    ("dec-byte", "65 fe 0c 25 b0 00 00 00", "41 fe 8e b0 00 00 00", 14, {14: 0}),
    ("sete-byte", "65 0f 94 04 25 b8 00 00 00", "41 0f 94 86 b8 00 00 00", 14, {14: 0}),
    ("cmpxchg-qword", "65 48 0f b1 0c 25 c0 00 00 00", "49 0f b1 8e c0 00 00 00", 14, {14: 0}),
    ("movsx-word", "65 48 0f bf 04 25 c8 00 00 00", "49 0f bf 86 c8 00 00 00", 14, {14: 0}),
    ("xchg-rbx", "65 48 87 1c 25 d8 00 00 00", "49 87 9e d8 00 00 00", 14, {14: 0}),
    ("movups-store", "65 0f 11 0c 25 e8 00 00 00", "41 0f 11 8e e8 00 00 00", 14, {14: 0}),
    ("movaps-load", "65 0f 28 0c 25 00 00 00 00", "41 0f 28 8e 00 00 00 00", 14, {14: 0}),
    ("movss-load", "65 f3 0f 10 04 25 f8 00 00 00", "f3 41 0f 10 86 f8 00 00 00", 14, {14: 0}),
    ("movsd-store", "65 f2 0f 11 04 25 50 00 00 00", "f2 41 0f 11 86 50 00 00 00", 14, {14: 0}),
    ("movd-load", "65 66 0f 6e 0c 25 04 00 00 00", "66 41 0f 6e 8e 04 00 00 00", 14, {14: 0}),
    ("movq-store-rexw", "65 66 48 0f 7e 04 25 20 00 00 00", "66 49 0f 7e 86 20 00 00 00", 14, {14: 0}),
    ("movq-load", "65 f3 0f 7e 0c 25 28 00 00 00", "f3 41 0f 7e 8e 28 00 00 00", 14, {14: 0}),
    ("movdqa-store", "65 66 0f 7f 0c 25 30 00 00 00", "66 41 0f 7f 8e 30 00 00 00", 14, {14: 0}),
    ("movapd-store", "65 66 0f 29 04 25 40 00 00 00", "66 41 0f 29 86 40 00 00 00", 14, {14: 0}),
)


def harness():
    code = bytearray()
    for register in (3, 5, 12, 13, 14, 15):
        code += (b"\x41" if register >= 8 else b"") + bytes([0x50 | (register & 7)])
    code += bytes.fromhex("56 52 51 48 b8") + struct.pack("<Q", 0x5a5a1234c3c35678) + b"\x50"
    code += bytes.fromhex("f3 0f 6f 87 80 00 00 00 f3 0f 6f 8f 90 00 00 00")
    for register in [r for r in range(16) if r not in (4, 7)] + [7]:
        code += bytes([0x48 | ((register >> 3) << 2), 0x8b, 0x47 | ((register & 7) << 3), register * 8])
    code += bytes.fromhex("ff 74 24 08 9d ff 54 24 10 9c 57 48 8b 7c 24 28")
    for register in [r for r in range(16) if r != 7]:
        code += bytes([0x48 | ((register >> 3) << 2), 0x89, 0x47 | ((register & 7) << 3), register * 8])
    code += bytes.fromhex("58 48 89 47 38 58 48 89 87 80 00 00 00")
    code += bytes.fromhex("f3 0f 7f 87 88 00 00 00 f3 0f 7f 8f 98 00 00 00 48 83 c4 20")
    for register in (15, 14, 13, 12, 5, 3):
        code += (b"\x41" if register >= 8 else b"") + bytes([0x58 | (register & 7)])
    return bytes(code + b"\xc3")


def set_base():
    return bytes.fromhex("56 48 89 f8 bf 83 00 00 00 48 89 e6 ff d0 59 c3")


def layout():
    cases, offset = bytearray(), []
    for _, gs, reference, _, _ in CASES:
        offset.append(len(cases))
        cases += bytes.fromhex(gs) + b"\xc3"
        offset.append(len(cases))
        cases += bytes.fromhex(reference) + b"\xc3"
    return bytes(cases), offset


def fixture():
    cases, _ = layout()
    pieces = [("GsSt", set_base()), ("GsRun", harness()), ("GsCasA", cases)]
    code, symbols, offset = bytearray(), [], CODE
    for name, body in pieces:
        symbols.append((name + "#A#B", 0x12, 0, 1, offset - CODE, len(body)))
        code += body
        offset += len(body)
    assert offset <= LIMIT, hex(offset)
    image = executable(tuple(symbols))
    image[CODE:LIMIT] = bytes(code) + b"\xcc" * (LIMIT - offset)
    struct.pack_into("<Q", image, 24, 0)
    return image


def registers(overrides):
    values = [0x1111111111111111 * ((index % 15) + 1) ^ (index << 56) for index in range(16)]
    for register, value in overrides.items():
        values[register] = value
    return values + [0x0123456789abcdef, 0xfedcba9876543210, 0x0f1e2d3c4b5a6978, 0x8796a5b4c3d2e1f0]


def host_source():
    rows = []
    _, offsets = layout()
    for index, (name, _, _, base, overrides) in enumerate(CASES):
        init = ", ".join(f"0x{value:x}ULL" for value in registers(overrides))
        rows.append(f'    {{"{name}", {offsets[2 * index]}, {offsets[2 * index + 1]}, {-1 if base is None else base}, {1 if base is None else 0}, {{{init}}}}},')
    return r'''#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint64_t Aps5GuestGsKey_nid_no_patch;

__attribute__((constructor)) static void CreateKey(void) {
    pthread_key_t key;
    if (pthread_key_create(&key, 0) != 0) abort();
    Aps5GuestGsKey_nid_no_patch = key;
}

int sysarch(int operation, void *parameters) {
    uint64_t base;
    if (operation == 131) {
        memcpy(&base, parameters, sizeof(base));
        if (base >= 0x0000800000000000ULL) { errno = EINVAL; return -1; }
        if (pthread_setspecific((pthread_key_t)Aps5GuestGsKey_nid_no_patch, (void *)(uintptr_t)base) != 0) abort();
        return 0;
    }
    if (operation == 130) {
        base = (uint64_t)(uintptr_t)pthread_getspecific((pthread_key_t)Aps5GuestGsKey_nid_no_patch);
        memcpy(parameters, &base, sizeof(base));
        return 0;
    }
    errno = EINVAL;
    return -1;
}

extern int GsSt(int (*)(int, void *), uint64_t);
extern void GsRun(const uint64_t *, uint64_t *, void *, uint64_t);
extern void GsCasA(void);

struct Case { const char *name; unsigned gs, reference; int base, stack; uint64_t init[20]; };
static const struct Case cases[] = {
''' + "\n".join(rows) + r'''
};

static void Fill(unsigned char *buffer, unsigned seed) {
    for (unsigned index = 0; index < ''' + str(BUFFER) + r'''; ++index) buffer[index] = (unsigned char)(index * 37 + seed);
}

static void *Find(unsigned offset) {
    return (unsigned char *)GsCasA + offset;
}

static uint64_t Load(void) {
    uint64_t out[''' + str(OUT // 8) + r'''];
    GsRun(cases[0].init, out, Find(cases[0].gs), 0x202);
    return out[0];
}

static unsigned char threadBuffer[''' + str(BUFFER) + r'''] __attribute__((aligned(16)));
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int stage;

static void Await(int value) {
    pthread_mutex_lock(&lock);
    while (stage < value) pthread_cond_wait(&changed, &lock);
    pthread_mutex_unlock(&lock);
}

static void Advance(int value) {
    pthread_mutex_lock(&lock);
    stage = value;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
}

static void *Other(void *argument) {
    (void)argument;
    if (GsSt(sysarch, (uint64_t)(uintptr_t)threadBuffer) != 0) _exit(20);
    Advance(1);
    Await(2);
    uint64_t expected;
    memcpy(&expected, threadBuffer + 0x10, sizeof(expected));
    if (Load() != expected) _exit(21);
    return 0;
}

static void *Unset(void *argument) {
    (void)argument;
    Load();
    _exit(30);
}

void Aps5StartGuest_nid_no_patch(void) {
    static unsigned char pools[2][3 * ''' + str(BUFFER) + r'''] __attribute__((aligned(16)));
    unsigned char *first = pools[0] + ''' + str(BUFFER) + r''';
    unsigned char *second = pools[1] + ''' + str(BUFFER) + r''';
    const char *mode = getenv("GS_PROBE_MODE");
    if (mode && strcmp(mode, "unset-main") == 0) {
        Load();
        _exit(30);
    }
    if (mode && strcmp(mode, "unset-thread") == 0) {
        Fill(first, 1);
        if (GsSt(sysarch, (uint64_t)(uintptr_t)first) != 0) _exit(4);
        pthread_t thread;
        if (pthread_create(&thread, 0, Unset, 0) != 0) _exit(5);
        pthread_join(thread, 0);
        _exit(31);
    }
    const uint64_t flags[] = {''' + ", ".join(f"0x{value:x}" for value in FLAGS) + r'''};
    for (unsigned index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const struct Case *item = &cases[index];
        for (unsigned pattern = 0; pattern < 2; ++pattern) {
            uint64_t gsOut[''' + str(OUT // 8) + r'''], referenceOut[''' + str(OUT // 8) + r'''], init[20];
            memset(pools[0], 0xa5, sizeof(pools[0]));
            memset(pools[1], 0x5a, sizeof(pools[1]));
            Fill(first, index);
            Fill(second, index);
            if (GsSt(sysarch, item->stack ? ''' + str(STACK_BASE) + r''' : (uint64_t)(uintptr_t)first) != 0) _exit(6);
            GsRun(item->init, gsOut, Find(item->gs), flags[pattern]);
            memcpy(init, item->init, sizeof(init));
            if (item->base >= 0) init[item->base] += (uint64_t)(uintptr_t)second;
            GsRun(init, referenceOut, Find(item->reference), flags[pattern]);
            if (item->base >= 0) referenceOut[item->base] -= (uint64_t)(uintptr_t)second;
            for (unsigned slot = 0; slot < ''' + str(OUT // 8) + r'''; ++slot) {
                if (gsOut[slot] != referenceOut[slot]) {
                    fprintf(stderr, "%s flags %llx slot %u: gs %llx reference %llx\n", item->name, (unsigned long long)flags[pattern], slot,
                            (unsigned long long)gsOut[slot], (unsigned long long)referenceOut[slot]);
                    _exit(10);
                }
            }
            int outside = 0;
            for (unsigned at = 0; at < ''' + str(BUFFER) + r'''; ++at)
                outside |= pools[0][at] != 0xa5 || pools[1][at] != 0x5a || first[''' + str(BUFFER) + r''' + at] != 0xa5 || second[''' + str(BUFFER) + r''' + at] != 0x5a;
            if (outside || memcmp(first, second, ''' + str(BUFFER) + r''') != 0) {
                fprintf(stderr, "%s flags %llx: memory differs\n", item->name, (unsigned long long)flags[pattern]);
                _exit(11);
            }
        }
    }
    Fill(first, 7);
    Fill(threadBuffer, 99);
    if (GsSt(sysarch, (uint64_t)(uintptr_t)first) != 0) _exit(7);
    pthread_t thread;
    if (pthread_create(&thread, 0, Other, 0) != 0) _exit(8);
    Await(1);
    uint64_t expected;
    memcpy(&expected, first + 0x10, sizeof(expected));
    if (Load() != expected) _exit(12);
    Advance(2);
    pthread_join(thread, 0);
    if (Load() != expected) _exit(13);
    printf("%u cases\n", (unsigned)(sizeof(cases) / sizeof(cases[0])));
    fflush(stdout);
    _exit(42);
}
'''


def convert(relinker, work):
    source, output = work / "gs.elf", work / "gs"
    source.write_bytes(fixture())
    run = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                         capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, (run.stdout, run.stderr)
    return output


def check_rewritten(output):
    data = output.read_bytes()
    code = b"".join(data[lo:hi] for lo, hi in MachO(data).executable_ranges())
    for name, gs, _, _, _ in CASES:
        assert bytes.fromhex(gs) + b"\xc3" not in code, name
    assert b"_Aps5GuestGsKey_nid_no_patch\0" in data


def native(work, output):
    if sys.platform != "darwin" or platform.machine() not in ("arm64", "x86_64"):
        print("macOS guest GS: native execution skipped")
        return
    libraries = work / "libs"
    libraries.mkdir()
    source = work / "kernel.c"
    source.write_text(host_source())
    for name in ("libkernel.prx", "libc.prx"):
        subprocess.run(["xcrun", "clang", "-arch", "x86_64", "-dynamiclib", str(source),
                        "-undefined", "dynamic_lookup", "-Wl,-install_name,@rpath/" + name,
                        "-o", str(libraries / name)], check=True, capture_output=True, timeout=30)
        source.write_text("int FixtureHostLibc(void) { return 0; }\n")
    output.chmod(0o755)
    subprocess.run(["codesign", "-s", "-", "-f", str(output)], capture_output=True, timeout=30)
    command = (["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []) + [str(output)]
    run = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if run.returncode != 42 and "Bad CPU type in executable" in run.stderr:
        assert os.environ.get("GITHUB_ACTIONS") != "true", "macOS CI must execute the x86_64 guest GS probe"
        print("macOS guest GS: x86_64 execution unavailable")
        return
    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
    print(f"macOS guest GS: {run.stdout.strip()} matched the reference under two flag patterns; two threads kept their own bases")
    for mode in ("unset-main", "unset-thread"):
        run = subprocess.run(command, capture_output=True, text=True, timeout=30,
                             env=dict(os.environ, GS_PROBE_MODE=mode))
        assert run.returncode == -11, (mode, run.returncode, run.stdout, run.stderr)
    print("macOS guest GS: a thread without a GS base faults with SIGSEGV")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-guest-gs-") as directory:
        work = Path(directory)
        output = convert(relinker, work)
        native(work, output)
        check_rewritten(output)
    print("macOS guest GS tests passed")


if __name__ == "__main__":
    main()
