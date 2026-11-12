import re
import sys


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
        "/* Generated from LibreSSL objects.txt and obj_mac.num by GenerateObjects.py */",
        "",
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
        handle.write("/* Generated from obj_mac.h by GenerateObjects.py */\n\n")
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
