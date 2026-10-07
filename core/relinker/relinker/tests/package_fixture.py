"""Build small plaintext PS5 finalized packages (FIH + outer PFS + NAPS inner image + CNT) for tests."""

import random
import struct

BLOCK = 0x10000
PAGE = 0x40000
HALF = 0x20000
MODULO = 0x3FFFF
MARKER = b"PPRPLAIN-NOAUTH!"
SIGNED_INODE = 0x2C8
INNER_INODE = 0xA8
INODES_PER_BLOCK = BLOCK // INNER_INODE


def fake_self(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phnum, = struct.unpack_from("<H", elf, 0x38)
    headers = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * 56) for index in range(phnum)]
    owned = [(index, header) for index, header in enumerate(headers) if header[5]]
    head = bytes(elf[:phoff + phnum * 56])
    position = (0x20 + len(owned) * 0x20 + len(head) + 15) & ~15
    entries = bytearray()
    body = bytearray()
    for index, header in owned:
        segment = bytes(elf[header[2]:header[2] + header[5]]).ljust(header[5], b"\0")
        entries += struct.pack("<QQQQ", 0x800 | 0x4 | (index << 20), position + len(body), header[5], header[5])
        body += segment + bytes(-len(segment) % 16)
    prefix = struct.pack("<IBBBBIHHQHHI", 0xEEF51454, 0, 1, 1, 0x12, 0x101, 0, 0, 0, len(owned), 0x22, 0) + entries + head
    return prefix + bytes(position - len(prefix)) + body


def dirents(entries):
    data = bytearray()
    for inode, kind, name in entries:
        encoded = name.encode()
        size = (16 + len(encoded) + 1 + 7) & ~7
        data += struct.pack("<IiiI", inode, kind, len(encoded), size) + encoded + bytes(size - 16 - len(encoded))
    return bytes(data)


def build_inner(files, sparse_paths=(), zero_paths=()):
    """files maps 'dir/name' -> bytes. Returns (mount bytes, block plan, fidx offsets, metadata offset)."""
    tree = {"": []}
    for path in sorted(files):
        parts = path.split("/")
        for depth in range(1, len(parts)):
            parent, name = "/".join(parts[:depth - 1]), parts[depth - 1]
            directory = "/".join(parts[:depth])
            if directory not in tree:
                tree[directory] = []
                tree[parent].append((name, directory, True))
        tree["/".join(parts[:-1])].append((parts[-1], path, False))

    mount = bytearray()
    regions = []
    offsets = {}
    for path in sorted(files):
        mount += bytes(-len(mount) % (PAGE if path in sparse_paths else BLOCK))
        offsets[path] = len(mount)
        data = files[path]
        if path in sparse_paths:
            regions.append((len(mount), len(data) // PAGE * PAGE, "sparse"))
            regions.append((len(mount) + len(data) // PAGE * PAGE, len(data) % PAGE, "stored"))
        else:
            regions.append((len(mount), len(data), "zero" if path in zero_paths else "stored"))
        mount += data

    directories = sorted(tree)
    inode_of = {"#super": 0}
    for directory in directories:
        inode_of[directory] = len(inode_of)
    for path in sorted(files):
        inode_of[path] = len(inode_of)
    inode_count = len(inode_of)
    mount += bytes(-len(mount) % PAGE)
    metadata = len(mount)
    inode_blocks = (inode_count + INODES_PER_BLOCK - 1) // INODES_PER_BLOCK
    directory_base = metadata + (1 + inode_blocks) * BLOCK
    directory_offset = {"#super": directory_base}
    for index, directory in enumerate(directories):
        directory_offset[directory] = directory_base + (index + 1) * BLOCK
    blobs = {"#super": dirents([(0, 4, "."), (0, 5, ".."), (inode_of[""], 3, "uroot")])}
    for directory in directories:
        own = inode_of[directory]
        parent = inode_of["/".join(directory.split("/")[:-1])] if directory else 0
        children = [(inode_of[target], 3 if is_dir else 2, name) for name, target, is_dir in tree[directory]]
        blobs[directory] = dirents([(own, 4, "."), (parent, 5, "..")] + children)
    end = directory_base + (len(directories) + 1) * BLOCK
    meta = bytearray(end - metadata)
    struct.pack_into("<qq", meta, 0, 2, 20130315)
    struct.pack_into("<I", meta, 0x20, BLOCK)
    struct.pack_into("<q", meta, 0x30, inode_count)
    for key, inode in inode_of.items():
        record = BLOCK + (inode // INODES_PER_BLOCK) * BLOCK + (inode % INODES_PER_BLOCK) * INNER_INODE
        if key in files:
            struct.pack_into("<HHIq", meta, record, 0x816D, 1, 0, len(files[key]))
            struct.pack_into("<Q", meta, record + 0x60, offsets[key])
        else:
            struct.pack_into("<HHIq", meta, record, 0x416D, 2, 0, len(blobs[key]))
            struct.pack_into("<Q", meta, record + 0x60, directory_offset[key])
    for key, blob in blobs.items():
        meta[directory_offset[key] - metadata:directory_offset[key] - metadata + len(blob)] = blob
    mount += meta
    regions.append((metadata, len(meta), "stored"))
    boundaries = sorted(set(offsets.values()) | {metadata})
    return bytes(mount), regions, boundaries, offsets


def build_naps(mount, regions, boundaries, kraken_offset=None):
    """Lay the mount out as native-span blocks. Returns (naps bytes, pfs_image bytes)."""
    blocks = []
    covered = 0

    def append(start, length, kind):
        position = start
        while position < start + length:
            size = min(start + length - position, PAGE - position % PAGE)
            blocks.append((position, size, kind))
            position += size

    for start, length, kind in regions:
        append(covered, start - covered, "zero")
        if kind != "sparse":
            append(start, length, kind)
        covered = start + length
    append(covered, len(mount) - covered, "zero")

    source = bytearray()
    records = [("run", 0, 0)]
    stds = []
    for position, size, kind in blocks:
        cmod = len(source) & MODULO
        if kind == "zero":
            stds.append((cmod, position & MODULO, 1, 0, 0))
        elif position == kraken_offset:
            assert 64 < size <= HALF, size
            source += bytes(mount[position:position + 64])
            stds.append((cmod, position & MODULO, 64, 5, 0))
        else:
            source += mount[position:position + size]
            stds.append((cmod, position & MODULO, min(size, HALF), 1, 1 if size > HALF else 0))
        records.append(("std", len(stds) - 1, position))
    stds.append((len(source) & MODULO, len(mount) & MODULO, 1, 0, 0))
    records.append(("std", len(stds) - 1, len(mount)))

    pages = (len(mount) + PAGE - 1) // PAGE
    std_record = {index: record for record, (kind, index, _) in enumerate(records) if kind == "std"}
    positions = [(position, std_record[index]) for kind, index, position in records if kind == "std"]
    anchors = []
    for page in range(pages + 8):
        anchors.append(next((record for position, record in positions if position >= page * PAGE), positions[-1][1]))
    fidx = boundaries + [len(mount)]
    outer_digests = 1
    header = struct.pack("<QQ", (len(fidx) - 1) | (2 << 24) | (pages << 32), outer_digests | ((len(records) - 2) << 24))
    naps = bytearray(header + bytes(outer_digests * 8))
    for index, offset in enumerate(fidx):
        naps += offset.to_bytes(5, "little") + bytes([0x40 if index == len(fidx) - 1 else 0])
    for group in range((pages + 8) >> 3):
        base = anchors[group * 8]
        naps += base.to_bytes(3, "little") + bytes(anchors[group * 8 + delta] - base for delta in range(1, 8))
    naps += bytes(-len(naps) % 8)
    for kind, index, _ in records:
        if kind == "run":
            naps += (1 << 18).to_bytes(8, "little") + b"\0"
        else:
            cmod, uoff, even_length, even, odd = stds[index]
            low = cmod | (uoff << 20) | ((even_length - 1) << 38) | (even << 55) | (odd << 58)
            naps += low.to_bytes(8, "little") + b"\0"
    return bytes(naps), bytes(source)


def build_outer(files):
    """files maps outer names under uroot -> (bytes, logical size). Returns (image bytes, superblock block)."""
    image = bytearray()
    placed = {}
    for name, (data, logical) in files.items():
        placed[name] = (len(image) // BLOCK, len(data), logical)
        image += data + bytes(-len(data) % BLOCK)
    inode_number = {"#super": 0, "uroot": 1}
    for name in files:
        inode_number[name] = len(inode_number)
    superblock = len(image) // BLOCK
    image += bytes(BLOCK * 4)
    inode_table, super_dir, uroot_dir = superblock + 1, superblock + 2, superblock + 3
    super_blob = dirents([(0, 4, "."), (0, 5, ".."), (1, 3, "uroot")])
    uroot_blob = dirents([(1, 4, "."), (0, 5, "..")] + [(inode_number[name], 2, name) for name in files])
    image[super_dir * BLOCK:super_dir * BLOCK + len(super_blob)] = super_blob
    image[uroot_dir * BLOCK:uroot_dir * BLOCK + len(uroot_blob)] = uroot_blob

    def inode(index, mode, size, logical, blocks):
        record = bytearray(SIGNED_INODE)
        struct.pack_into("<HHIqq", record, 0, mode, 1, 0, size, logical)
        struct.pack_into("<I", record, 0x60, len(blocks))
        for slot, block in enumerate(blocks[:12]):
            struct.pack_into("<I", record, 0x64 + slot * 36 + 32, block)
        if len(blocks) > 12:
            indirect = len(image) // BLOCK
            image.extend(bytes(BLOCK))
            for slot, block in enumerate(blocks[12:]):
                struct.pack_into("<I", image, indirect * BLOCK + slot * 36 + 32, block)
            struct.pack_into("<I", record, 0x64 + 12 * 36 + 32, indirect)
        position = inode_table * BLOCK + index * SIGNED_INODE
        image[position:position + SIGNED_INODE] = record

    inode(0, 0x416D, len(super_blob), len(super_blob), [super_dir])
    inode(1, 0x416D, len(uroot_blob), len(uroot_blob), [uroot_dir])
    for name, (first, size, logical) in placed.items():
        inode(inode_number[name], 0x816D, size, logical, list(range(first, first + max(1, (size + BLOCK - 1) // BLOCK))))
    sb = bytearray(BLOCK)
    struct.pack_into("<qq", sb, 0, 2, 20130315)
    struct.pack_into("<H", sb, 0x1C, 0x0D)
    struct.pack_into("<I", sb, 0x20, BLOCK)
    struct.pack_into("<qqqq", sb, 0x30, len(inode_number), len(image) // BLOCK, 1, 0)
    struct.pack_into("<Q", sb, 0xD8, inode_table)
    sb[0x370:0x380] = MARKER
    image[superblock * BLOCK:(superblock + 1) * BLOCK] = sb
    return bytes(image), superblock


def build_cnt(named):
    names = bytearray(b"\0")
    name_offsets = {}
    for name in named:
        name_offsets[name] = len(names)
        names += name.encode() + b"\0"
    ids = [0x200] + [0x1000 + index for index in range(len(named))]
    table = 0x100
    data_start = table + len(ids) * 0x20
    blobs = [bytes(names)] + list(named.values())
    cnt = bytearray(data_start)
    cnt[:4] = b"\x7fCNT"
    struct.pack_into(">I", cnt, 0x10, len(ids))
    struct.pack_into(">I", cnt, 0x18, table)
    cnt[0x40:0x64] = b"UP0000-TEST00000_00-0000000000000000"
    for index, blob in enumerate(blobs):
        offset = len(cnt)
        name_offset = 0 if index == 0 else name_offsets[list(named)[index - 1]]
        struct.pack_into(">IIIIII", cnt, table + index * 0x20, ids[index], name_offset, 0, 0, offset, len(blob))
        cnt += blob + bytes(-len(blob) % 16)
    return bytes(cnt)


def build_package(files, content=None, sparse_paths=(), zero_paths=(), kraken_path=None, marker=True):
    mount, regions, boundaries, offsets = build_inner(files, sparse_paths, zero_paths)
    naps, source = build_naps(mount, regions, boundaries, offsets[kraken_path] if kraken_path is not None else None)
    image, superblock = build_outer({"pfs_image.dat": (source, len(mount)), "naps_pkg_layout.dat": (naps, len(naps))})
    if not marker:
        position = superblock * BLOCK + 0x370
        image = image[:position] + bytes(16) + image[position + 16:]
    cnt = build_cnt(content or {})
    fih = bytearray(BLOCK)
    fih[:4] = b"\x7fFIH"
    struct.pack_into("<QQQ", fih, 0x10, BLOCK, len(image), BLOCK + superblock * BLOCK)
    struct.pack_into("<Q", fih, 0x58, BLOCK + len(image))
    return bytes(fih) + image + cnt


def random_bytes(size, seed):
    generator = random.Random(seed)
    return bytes(generator.getrandbits(8) for _ in range(size))
