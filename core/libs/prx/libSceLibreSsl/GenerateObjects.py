import re
import sys

DERIVED = (
    "Derived from LibreSSL 3.4.1 (openbsd 078b3d1df8ca2011c1605cd40418c58fa0f5b143) "
    "src/lib/libcrypto/objects/objects.pl and src/lib/libcrypto/objects/obj_dat.pl"
)

NOTICE = """\
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
"""


def parse_numbers(path):
    nid = {}
    nidn = {}
    max_nid = 0
    with open(path, encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            line = re.sub(r"#.*$", "", line.rstrip("\r\n"))
            if not line.strip():
                continue
            name, number = ("X" + line).split()[:2]
            name = name[1:]
            number = int(number)
            if number in nidn:
                raise SystemExit(f"{path}:{line_number}: duplicate NID {number}")
            if name in nid:
                raise SystemExit(f"{path}:{line_number}: duplicate name {name}")
            nid[name] = number
            nidn[number] = name
            max_nid = max(max_nid, number)
    return nid, max_nid


def process_oid(oid, obj, path, line_number):
    parts = oid.split()
    prefix = ""
    if parts and not re.fullmatch(r"[0-9]+", parts[0]):
        base = parts.pop(0).replace("-", "_")
        if base not in obj:
            raise SystemExit(f"{path}:{line_number}: undefined identifier {base}")
        prefix = "OBJ_" + base
    arcs = ",".join(f"{part}L" for part in parts)
    if not arcs:
        return prefix
    return f"{prefix},{arcs}" if prefix else arcs


def parse_objects(path, nid, max_nid):
    order = []
    sn = {}
    ln = {}
    obj = {}
    module = ""
    cname = ""
    with open(path, encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            line = line.rstrip("\r\n")
            match = re.match(r"^!module\s+(.*)$", line)
            if match:
                module = (match.group(1) + "-").replace(".", "_").replace("-", "_")
            if re.match(r"^!global$", line):
                module = ""
            match = re.match(r"^!Cname\s+(.*)$", line)
            if match:
                cname = match.group(1)
            match = re.match(r"^!Alias\s+(.+?)\s+(.*)$", line)
            if match:
                alias = (module + match.group(1)).replace("-", "_")
                obj[alias] = process_oid(match.group(2), obj, path, line_number)
                order.append(alias)
                line = ""
                cname = ""
            line = re.sub(r"!.*$", "", line)
            line = re.sub(r"#.*$", "", line)
            if not line.strip():
                continue
            fields = (line.split(":") + ["", "", ""])[:3]
            oid, short, long = (field.strip() for field in fields)
            if oid:
                oid = process_oid(oid, obj, path, line_number)
            if not cname and " " not in long:
                cname = long.replace(".", "_").replace("-", "_")
                if cname and module + cname in ln:
                    raise SystemExit(f"{path}:{line_number}: duplicate long name {long}")
            if not cname:
                cname = short.replace("-", "_")
                if cname and module + cname in sn:
                    raise SystemExit(f"{path}:{line_number}: duplicate short name {short}")
            if not cname:
                cname = long.replace("-", "_").replace(".", "_").replace(" ", "_")
                if cname and module + cname in ln:
                    raise SystemExit(f"{path}:{line_number}: duplicate long name {long}")
            cname = module + cname.replace(".", "_").replace("-", "_")
            order.append(cname)
            sn[cname] = short
            ln[cname] = long
            obj[cname] = oid
            if cname not in nid:
                max_nid += 1
                nid[cname] = max_nid
            cname = ""
    return order, sn, ln, obj


def write_obj_mac(path, order, sn, ln, nid, obj):
    lines = [
        "/* crypto/objects/obj_mac.h */",
        "",
        "/* THIS FILE IS GENERATED FROM objects.txt and obj_mac.num by GenerateObjects.py.",
        f" * {DERIVED}",
        " */",
        "",
        NOTICE,
        "#define SN_undef\t\t\t\"UNDEF\"",
        "#define LN_undef\t\t\t\"undefined\"",
        "#define NID_undef\t\t\t0",
        "#define OBJ_undef\t\t\t0L",
        "",
    ]
    for cname in order:
        if sn.get(cname, ""):
            lines.append(f"#define SN_{cname}\t\t\"{sn[cname]}\"")
        if ln.get(cname, ""):
            lines.append(f"#define LN_{cname}\t\t\"{ln[cname]}\"")
        if cname in nid:
            lines.append(f"#define NID_{cname}\t\t{nid[cname]}")
        if obj.get(cname, ""):
            lines.append(f"#define OBJ_{cname}\t\t{obj[cname]}")
        lines.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def der_encode(arcs):
    out = [arcs[0] * 40 + arcs[1]]
    for arc in arcs[2:]:
        chunk = [arc & 0x7F]
        arc >>= 7
        while arc:
            chunk.append(0x80 | (arc & 0x7F))
            arc >>= 7
        out.extend(reversed(chunk))
    return out


def read_obj_mac(path):
    sn = {}
    ln = {}
    nid = {}
    obj = {}
    objd = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            match = re.match(r"^#define\s+(\S+)\s+(.*)$", line.rstrip("\n"))
            if not match:
                continue
            name, value = match.groups()
            value = re.sub(r'^"', "", value)
            value = re.sub(r'"$', "", value)
            if name.startswith("SN_"):
                sn[name[3:]] = value
            elif name.startswith("LN_"):
                ln[name[3:]] = value
            elif name.startswith("NID_"):
                nid[int(value)] = name[4:]
            elif name.startswith("OBJ_"):
                obj[name[4:]] = name
                objd[name] = value
    changed = True
    while changed:
        changed = False
        for key, value in objd.items():
            match = re.search(r"(OBJ_[^,]+),", value)
            if match and match.group(1) in objd:
                objd[key] = value[:match.start()] + objd[match.group(1)] + "," + value[match.end():]
                changed = True
    return sn, ln, nid, obj, objd


def wrap_entry(entry):
    if len(entry) <= 75:
        return entry
    fields = entry.split(",")
    while fields and fields[-1] == "":
        fields.pop()
    out = ""
    result = ""
    for field in fields:
        candidate = out + field + ","
        if len(candidate) > 70:
            result += out + "\n"
            candidate = "\t" + field + ","
        out = candidate
    return result + out[:-1]


def write_obj_dat(source, path):
    sn, ln, nid, obj, objd = read_obj_mac(source)
    count = max(nid) + 1
    entries = []
    lvalues = []
    lvalue_count = 0
    der = {}
    for index in range(count):
        if index not in nid:
            entries.append("{NULL,NULL,NID_undef,0,NULL,0},\n")
            continue
        name = nid[index]
        short = sn.get(name)
        long = ln.get(name)
        if short is None:
            short = long if long is not None else "NULL"
            sn[name] = short
        if long is None:
            long = short
            ln[name] = long
        entry = f'{{"{short}","{long}",NID_{name},'
        macro = obj.get(name)
        if macro is not None and "," in objd[macro]:
            arcs = [int(arc) for arc in objd[macro].replace("L", "").split(",")]
            data = der_encode(arcs)
            text = "".join(f"0x{byte:02X}," for byte in data)
            der[macro] = (len(data), text)
            lvalues.append(f"{text:<45s}/* [{lvalue_count:3d}] {macro} */\n")
            entry += f"{len(data)},&(lvalues[{lvalue_count}]),0"
            lvalue_count += len(data)
        else:
            entry += "0,NULL,0"
        entry += "},\n"
        entries.append(entry)

    present = [index for index in range(count) if index in nid]
    short_order = sorted((index for index in present if nid[index] in sn), key=lambda index: sn[nid[index]].encode())
    long_order = sorted((index for index in present if nid[index] in ln), key=lambda index: ln[nid[index]].encode())
    with_object = [index for index in present if nid[index] in obj]

    def object_key(index):
        length, text = der.get(obj[nid[index]], (0, ""))
        return (length, text.encode())

    object_order = sorted(with_object, key=object_key)

    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("/* crypto/objects/obj_dat.h */\n\n")
        handle.write(f"/* THIS FILE IS GENERATED FROM obj_mac.h by GenerateObjects.py.\n * {DERIVED}\n */\n\n")
        handle.write(NOTICE + "\n")
        handle.write(f"#define NUM_NID {count}\n")
        handle.write(f"#define NUM_SN {len(short_order)}\n")
        handle.write(f"#define NUM_LN {len(long_order)}\n")
        handle.write(f"#define NUM_OBJ {len(object_order)}\n\n")
        handle.write(f"static const unsigned char lvalues[{lvalue_count + 1}]={{\n")
        handle.writelines(lvalues)
        handle.write("};\n\n")
        handle.write("static const ASN1_OBJECT nid_objs[NUM_NID]={\n")
        handle.writelines(wrap_entry(entry) for entry in entries)
        handle.write("};\n\n")
        handle.write("static const unsigned int sn_objs[NUM_SN]={\n")
        handle.writelines(f'{index:2d},\t/* "{sn[nid[index]]}" */\n' for index in short_order)
        handle.write("};\n\n")
        handle.write("static const unsigned int ln_objs[NUM_LN]={\n")
        handle.writelines(f'{index:2d},\t/* "{ln[nid[index]]}" */\n' for index in long_order)
        handle.write("};\n\n")
        handle.write("static const unsigned int obj_objs[NUM_OBJ]={\n")
        for index in object_order:
            macro = obj[nid[index]]
            arcs = objd[macro].replace("L", "").replace(",", " ")
            handle.write(f"{index:2d},\t/* {macro:<32s} {arcs} */\n")
        handle.write("};\n\n")


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: GenerateObjects.py objects.txt obj_mac.num obj_mac.h obj_dat.h")
    objects, numbers, mac, dat = sys.argv[1:]
    nid, max_nid = parse_numbers(numbers)
    order, sn, ln, obj = parse_objects(objects, nid, max_nid)
    write_obj_mac(mac, order, sn, ln, nid, obj)
    write_obj_dat(mac, dat)


if __name__ == "__main__":
    main()
