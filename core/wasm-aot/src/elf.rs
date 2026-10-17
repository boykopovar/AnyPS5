use gimli::{BaseAddresses, EhFrame, EhFrameHdr, EhFrameOffset, LittleEndian, Pointer, UnwindSection};

use crate::image::{Fde, Image, Phdr, Relocation, Segment, Symbol, SymbolKind, Tls};
use crate::{Error, Result};

const PT_LOAD: u32 = 1;
const PT_DYNAMIC: u32 = 2;
const PT_TLS: u32 = 7;
const PT_GNU_EH_FRAME: u32 = 0x6474_e550;

const DT_NULL: u64 = 0;
const DT_NEEDED: u64 = 1;
const DT_PLTRELSZ: u64 = 2;
const DT_HASH: u64 = 4;
const DT_STRTAB: u64 = 5;
const DT_SYMTAB: u64 = 6;
const DT_RELA: u64 = 7;
const DT_RELASZ: u64 = 8;
const DT_STRSZ: u64 = 10;
const DT_INIT: u64 = 12;
const DT_FINI: u64 = 13;
const DT_JMPREL: u64 = 23;
const DT_INIT_ARRAY: u64 = 25;
const DT_FINI_ARRAY: u64 = 26;
const DT_INIT_ARRAYSZ: u64 = 27;
const DT_FINI_ARRAYSZ: u64 = 28;
const DT_GNU_HASH: u64 = 0x6fff_fef5;

const ET_EXEC: u16 = 2;
const EM_X86_64: u16 = 62;

struct Bytes<'a> {
    data: &'a [u8],
    name: &'a str,
}

impl Bytes<'_> {
    fn fail(&self, what: &str) -> Error {
        Error::new(format!("{}: {what}", self.name))
    }
    fn slice(&self, offset: u64, length: u64) -> Result<&[u8]> {
        let start = usize::try_from(offset).map_err(|_| self.fail("offset out of range"))?;
        let end = start.checked_add(length as usize).filter(|&end| end <= self.data.len());
        Ok(&self.data[start..end.ok_or_else(|| self.fail(&format!("data at {offset:#x}+{length:#x} is past the end of the file")))?])
    }
    fn u16(&self, offset: u64) -> Result<u16> {
        Ok(u16::from_le_bytes(self.slice(offset, 2)?.try_into().unwrap()))
    }
    fn u32(&self, offset: u64) -> Result<u32> {
        Ok(u32::from_le_bytes(self.slice(offset, 4)?.try_into().unwrap()))
    }
    fn u64(&self, offset: u64) -> Result<u64> {
        Ok(u64::from_le_bytes(self.slice(offset, 8)?.try_into().unwrap()))
    }
}

pub fn load(name: &str, data: &[u8]) -> Result<Image> {
    let file = Bytes { data, name };
    if file.slice(0, 4)? != b"\x7fELF" || file.slice(4, 1)?[0] != 2 || file.slice(5, 1)?[0] != 1 {
        return Err(file.fail("not a little-endian ELF64 file"));
    }
    if file.u16(18)? != EM_X86_64 {
        return Err(file.fail("not an x86-64 ELF file"));
    }
    let file_type = file.u16(16)?;
    let entry = file.u64(24)?;
    let phoff = file.u64(32)?;
    let phentsize = file.u16(54)? as u64;
    let phnum = file.u16(56)? as u64;

    let mut image = Image { name: name.to_string(), ..Image::default() };
    let mut dynamic = None;
    let mut eh_frame_hdr = None;
    for index in 0..phnum {
        let header = phoff + index * phentsize;
        let kind = file.u32(header)?;
        let flags = file.u32(header + 4)?;
        let offset = file.u64(header + 8)?;
        let vaddr = file.u64(header + 16)?;
        let filesz = file.u64(header + 32)?;
        let memsz = file.u64(header + 40)?;
        let align = file.u64(header + 48)?;
        image.phdrs.push(Phdr { kind, flags, vaddr, memsz });
        match kind {
            PT_LOAD => image.segments.push(Segment { vaddr, data: file.slice(offset, filesz)?.to_vec(), memsz, flags }),
            PT_DYNAMIC => dynamic = Some((offset, filesz)),
            PT_TLS => image.tls = Some(Tls { init_vaddr: vaddr, init_size: filesz, mem_size: memsz, align: align.max(1) }),
            PT_GNU_EH_FRAME => eh_frame_hdr = Some(vaddr),
            _ => {}
        }
    }
    if file_type == ET_EXEC || entry != 0 {
        image.entry = Some(entry);
    }
    if let Some((offset, size)) = dynamic {
        read_dynamic(&file, &mut image, offset, size)?;
    }
    if let Some(vaddr) = eh_frame_hdr {
        image.fdes = read_fdes(&image, vaddr)?;
    }
    Ok(image)
}

fn read_dynamic(file: &Bytes<'_>, image: &mut Image, offset: u64, size: u64) -> Result<()> {
    let mut tags: Vec<(u64, u64)> = Vec::new();
    for index in 0..size / 16 {
        let tag = file.u64(offset + index * 16)?;
        if tag == DT_NULL {
            break;
        }
        tags.push((tag, file.u64(offset + index * 16 + 8)?));
    }
    let value = |wanted: u64| tags.iter().find(|(tag, _)| *tag == wanted).map(|(_, value)| *value);
    let in_image = |vaddr: u64, length: u64| -> Result<Vec<u8>> {
        let bytes = image.bytes_at(vaddr).ok_or_else(|| file.fail(&format!("dynamic data at {vaddr:#x} is outside the segments")))?;
        bytes.get(..length as usize).map(<[u8]>::to_vec).ok_or_else(|| file.fail(&format!("dynamic data at {vaddr:#x} is truncated")))
    };
    let strings = match (value(DT_STRTAB), value(DT_STRSZ)) {
        (Some(vaddr), Some(size)) => in_image(vaddr, size)?,
        _ => Vec::new(),
    };
    let string = |offset: u64| -> Result<String> {
        let tail = strings.get(offset as usize..).ok_or_else(|| file.fail("string offset is outside DT_STRTAB"))?;
        let end = tail.iter().position(|&byte| byte == 0).ok_or_else(|| file.fail("unterminated dynamic string"))?;
        Ok(String::from_utf8_lossy(&tail[..end]).into_owned())
    };
    let mut needed = Vec::new();
    for (tag, value) in &tags {
        if *tag == DT_NEEDED {
            needed.push(string(*value)?);
        }
    }
    let mut symbols = Vec::new();
    if let Some(symtab) = value(DT_SYMTAB) {
        let count = symbol_count(image, value(DT_HASH), value(DT_GNU_HASH), file)?;
        let table = in_image(symtab, count * 24)?;
        for index in 0..count as usize {
            let entry = &table[index * 24..index * 24 + 24];
            let name = u32::from_le_bytes(entry[0..4].try_into().unwrap());
            let info = entry[4];
            let section = u16::from_le_bytes(entry[6..8].try_into().unwrap());
            let symbol_value = u64::from_le_bytes(entry[8..16].try_into().unwrap());
            let kind = match info & 0xf {
                2 | 10 => SymbolKind::Function,
                1 => SymbolKind::Data,
                6 => SymbolKind::Tls,
                _ => SymbolKind::NoType,
            };
            let binding = info >> 4;
            let defined = section != 0 && binding != 0;
            symbols.push(Symbol { name: string(name as u64)?, kind, value: defined.then_some(symbol_value), weak: binding == 2 });
        }
    }
    let mut relocations = Vec::new();
    for (table, size) in [(value(DT_RELA), value(DT_RELASZ)), (value(DT_JMPREL), value(DT_PLTRELSZ))] {
        let (Some(vaddr), Some(size)) = (table, size) else {
            continue;
        };
        let entries = in_image(vaddr, size)?;
        for entry in entries.chunks_exact(24) {
            let offset = u64::from_le_bytes(entry[0..8].try_into().unwrap());
            let info = u64::from_le_bytes(entry[8..16].try_into().unwrap());
            let addend = i64::from_le_bytes(entry[16..24].try_into().unwrap());
            let symbol = (info >> 32) as usize;
            if symbol >= symbols.len().max(1) {
                return Err(file.fail(&format!("relocation at {offset:#x} names symbol {symbol} past the table")));
            }
            relocations.push(Relocation { offset, kind: info as u32, symbol: (symbol != 0).then_some(symbol), addend });
        }
    }
    image.needed = needed;
    image.symbols = symbols;
    image.relocations = relocations;
    image.init = value(DT_INIT);
    image.fini = value(DT_FINI);
    if let (Some(vaddr), Some(size)) = (value(DT_INIT_ARRAY), value(DT_INIT_ARRAYSZ)) {
        image.init_array = Some((vaddr, (size / 8) as u32));
    }
    if let (Some(vaddr), Some(size)) = (value(DT_FINI_ARRAY), value(DT_FINI_ARRAYSZ)) {
        image.fini_array = Some((vaddr, (size / 8) as u32));
    }
    Ok(())
}

fn symbol_count(image: &Image, hash: Option<u64>, gnu_hash: Option<u64>, file: &Bytes<'_>) -> Result<u64> {
    if let Some(vaddr) = hash {
        return image.read_u32(vaddr + 4).map(u64::from).ok_or_else(|| file.fail("DT_HASH is outside the segments"));
    }
    let Some(vaddr) = gnu_hash else {
        return Err(file.fail("dynamic symbols without DT_HASH or DT_GNU_HASH"));
    };
    let read = |address: u64| image.read_u32(address).ok_or_else(|| file.fail("DT_GNU_HASH is outside the segments"));
    let buckets = read(vaddr)? as u64;
    let symbol_offset = read(vaddr + 4)? as u64;
    let bloom = read(vaddr + 8)? as u64;
    let bucket_start = vaddr + 16 + bloom * 8;
    let chain_start = bucket_start + buckets * 4;
    let mut last = 0u64;
    for bucket in 0..buckets {
        last = last.max(read(bucket_start + bucket * 4)? as u64);
    }
    if last < symbol_offset {
        return Ok(symbol_offset);
    }
    while read(chain_start + (last - symbol_offset) * 4)? & 1 == 0 {
        last += 1;
    }
    Ok(last + 1)
}

fn read_fdes(image: &Image, hdr_vaddr: u64) -> Result<Vec<Fde>> {
    let fail = |what: String| Error::new(format!("{}: {what}", image.name));
    let hdr_bytes = image.bytes_at(hdr_vaddr).ok_or_else(|| fail("eh_frame_hdr is outside the segments".into()))?;
    let bases = BaseAddresses::default().set_eh_frame_hdr(hdr_vaddr);
    let hdr = EhFrameHdr::new(hdr_bytes, LittleEndian).parse(&bases, 8).map_err(|error| fail(format!("eh_frame_hdr: {error}")))?;
    let Pointer::Direct(eh_frame_vaddr) = hdr.eh_frame_ptr() else {
        return Err(fail("indirect eh_frame pointer".into()));
    };
    let eh_frame_bytes = image.bytes_at(eh_frame_vaddr).ok_or_else(|| fail("eh_frame is outside the segments".into()))?;
    let eh_frame = EhFrame::new(eh_frame_bytes, LittleEndian);
    let bases = bases.set_eh_frame(eh_frame_vaddr);
    let Some(table) = hdr.table() else {
        return Ok(Vec::new());
    };
    let mut iterator = table.iter(&bases);
    let mut fdes = Vec::new();
    while let Some((_, fde_pointer)) = iterator.next().map_err(|error| fail(format!("eh_frame_hdr table: {error}")))? {
        let Pointer::Direct(fde_vaddr) = fde_pointer else {
            return Err(fail("indirect FDE pointer".into()));
        };
        let offset = EhFrameOffset((fde_vaddr - eh_frame_vaddr) as usize);
        let fde = eh_frame
            .fde_from_offset(&bases, offset, |section, bases, offset| section.cie_from_offset(bases, offset))
            .map_err(|error| fail(format!("FDE at {fde_vaddr:#x}: {error}")))?;
        let lsda = match fde.lsda() {
            None => None,
            Some(Pointer::Direct(address)) => Some(address),
            Some(Pointer::Indirect(slot)) => image.relocated_pointer(slot),
        };
        fdes.push(Fde { start: fde.initial_address(), end: fde.initial_address() + fde.len(), lsda });
    }
    Ok(fdes)
}
