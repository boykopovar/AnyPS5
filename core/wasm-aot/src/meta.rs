use crate::{Error, Result};

const MAGIC: &[u8; 8] = b"APS5IMG\0";
const VERSION: u32 = 1;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Metadata {
    pub images: Vec<ImageMeta>,
    pub stubs: Vec<String>,
    pub host_data: Vec<String>,
    pub symbols: Vec<SymbolRef>,
    pub addr_map: Vec<u8>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ImageMeta {
    pub name: String,
    pub span: u64,
    pub align: u64,
    pub segments: Vec<SegmentMeta>,
    pub phdrs: Vec<PhdrMeta>,
    pub tls: Option<TlsMeta>,
    pub entry: Option<u64>,
    pub init: Option<u64>,
    pub fini: Option<u64>,
    pub init_array: Option<(u64, u32)>,
    pub fini_array: Option<(u64, u32)>,
    pub needed: Vec<String>,
    pub exports: Vec<ExportMeta>,
    pub relocations: Vec<RelocMeta>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SegmentMeta {
    pub vaddr: u64,
    pub memsz: u64,
    pub flags: u32,
    pub data: Vec<u8>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PhdrMeta {
    pub kind: u32,
    pub flags: u32,
    pub vaddr: u64,
    pub memsz: u64,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TlsMeta {
    pub init_vaddr: u64,
    pub init_size: u64,
    pub mem_size: u64,
    pub align: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ExportKind {
    Function = 0,
    Data = 1,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ExportMeta {
    pub nid: String,
    pub vaddr: u64,
    pub kind: ExportKind,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RelocKind {
    Relative = 1,
    GuestSymbol = 2,
    ImportStub = 3,
    HostData = 4,
    TlsModuleId = 5,
    TlsOffset = 6,
    DtpOffset = 7,
}

impl RelocKind {
    fn from_byte(value: u8) -> Result<Self> {
        Ok(match value {
            1 => Self::Relative,
            2 => Self::GuestSymbol,
            3 => Self::ImportStub,
            4 => Self::HostData,
            5 => Self::TlsModuleId,
            6 => Self::TlsOffset,
            7 => Self::DtpOffset,
            _ => return Err(Error::new(format!("unknown relocation kind {value}"))),
        })
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RelocMeta {
    pub offset: u64,
    pub kind: RelocKind,
    pub addend: i64,
    pub target: u32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SymbolRef {
    pub image: u32,
    pub vaddr: u64,
}

const NONE: u64 = u64::MAX;

struct Writer {
    head: Vec<u8>,
    blob: Vec<u8>,
}

impl Writer {
    fn u8(&mut self, value: u8) {
        self.head.push(value);
    }
    fn u32(&mut self, value: u32) {
        self.head.extend_from_slice(&value.to_le_bytes());
    }
    fn u64(&mut self, value: u64) {
        self.head.extend_from_slice(&value.to_le_bytes());
    }
    fn len(&mut self, value: usize) {
        self.u32(u32::try_from(value).expect("metadata table larger than 4 GiB"));
    }
    fn string(&mut self, value: &str) {
        self.len(value.len());
        self.head.extend_from_slice(value.as_bytes());
    }
    fn option(&mut self, value: Option<u64>) {
        self.u64(value.unwrap_or(NONE));
    }
    fn array(&mut self, value: Option<(u64, u32)>) {
        let (vaddr, count) = value.unwrap_or((NONE, 0));
        self.u64(vaddr);
        self.u32(count);
    }
    fn blob(&mut self, bytes: &[u8]) -> u64 {
        let offset = self.blob.len() as u64;
        self.blob.extend_from_slice(bytes);
        offset
    }
}

pub fn encode(metadata: &Metadata) -> Vec<u8> {
    let mut writer = Writer { head: Vec::new(), blob: Vec::new() };
    writer.head.extend_from_slice(MAGIC);
    writer.u32(VERSION);
    writer.len(metadata.images.len());
    writer.len(metadata.stubs.len());
    let map_offset = writer.blob(&metadata.addr_map);
    writer.u64(map_offset);
    writer.u64(metadata.addr_map.len() as u64);
    for image in &metadata.images {
        writer.string(&image.name);
        writer.u64(image.span);
        writer.u64(image.align);
        writer.len(image.segments.len());
        for segment in &image.segments {
            let offset = writer.blob(&segment.data);
            writer.u64(segment.vaddr);
            writer.u64(segment.data.len() as u64);
            writer.u64(segment.memsz);
            writer.u32(segment.flags);
            writer.u64(offset);
        }
        writer.len(image.phdrs.len());
        for phdr in &image.phdrs {
            writer.u32(phdr.kind);
            writer.u32(phdr.flags);
            writer.u64(phdr.vaddr);
            writer.u64(phdr.memsz);
        }
        match &image.tls {
            Some(tls) => {
                writer.u8(1);
                writer.u64(tls.init_vaddr);
                writer.u64(tls.init_size);
                writer.u64(tls.mem_size);
                writer.u64(tls.align);
            }
            None => writer.u8(0),
        }
        writer.option(image.entry);
        writer.option(image.init);
        writer.option(image.fini);
        writer.array(image.init_array);
        writer.array(image.fini_array);
        writer.len(image.needed.len());
        for needed in &image.needed {
            writer.string(needed);
        }
        writer.len(image.exports.len());
        for export in &image.exports {
            writer.string(&export.nid);
            writer.u64(export.vaddr);
            writer.u8(export.kind as u8);
        }
        writer.len(image.relocations.len());
        for relocation in &image.relocations {
            writer.u64(relocation.offset);
            writer.u8(relocation.kind as u8);
            writer.u64(relocation.addend as u64);
            writer.u32(relocation.target);
        }
    }
    writer.len(metadata.symbols.len());
    for symbol in &metadata.symbols {
        writer.u32(symbol.image);
        writer.u64(symbol.vaddr);
    }
    for stub in &metadata.stubs {
        writer.string(stub);
    }
    writer.len(metadata.host_data.len());
    for data in &metadata.host_data {
        writer.string(data);
    }
    let mut bytes = writer.head;
    bytes.extend_from_slice(&(bytes.len() as u64 + 8).to_le_bytes());
    bytes.extend_from_slice(&writer.blob);
    bytes
}

struct Reader<'a> {
    bytes: &'a [u8],
    position: usize,
}

impl<'a> Reader<'a> {
    fn take(&mut self, length: usize) -> Result<&'a [u8]> {
        let end = self.position.checked_add(length).filter(|&end| end <= self.bytes.len());
        let end = end.ok_or_else(|| Error::new("image metadata is truncated"))?;
        let slice = &self.bytes[self.position..end];
        self.position = end;
        Ok(slice)
    }
    fn u8(&mut self) -> Result<u8> {
        Ok(self.take(1)?[0])
    }
    fn u32(&mut self) -> Result<u32> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }
    fn u64(&mut self) -> Result<u64> {
        Ok(u64::from_le_bytes(self.take(8)?.try_into().unwrap()))
    }
    fn string(&mut self) -> Result<String> {
        let length = self.u32()? as usize;
        String::from_utf8(self.take(length)?.to_vec()).map_err(|_| Error::new("image metadata string is not UTF-8"))
    }
    fn option(&mut self) -> Result<Option<u64>> {
        Ok(Some(self.u64()?).filter(|&value| value != NONE))
    }
    fn array(&mut self) -> Result<Option<(u64, u32)>> {
        let vaddr = self.u64()?;
        let count = self.u32()?;
        Ok((vaddr != NONE).then_some((vaddr, count)))
    }
}

pub fn decode(bytes: &[u8]) -> Result<Metadata> {
    let mut reader = Reader { bytes, position: 0 };
    if reader.take(8)? != MAGIC {
        return Err(Error::new("image metadata has a wrong magic"));
    }
    let version = reader.u32()?;
    if version != VERSION {
        return Err(Error::new(format!("image metadata version {version} is not {VERSION}")));
    }
    let image_count = reader.u32()?;
    let stub_count = reader.u32()?;
    let map_offset = reader.u64()?;
    let map_size = reader.u64()?;
    let mut images = Vec::new();
    let mut pending_segments = Vec::new();
    for _ in 0..image_count {
        let mut image = ImageMeta { name: reader.string()?, span: reader.u64()?, align: reader.u64()?, ..ImageMeta::default() };
        for _ in 0..reader.u32()? {
            let vaddr = reader.u64()?;
            let filesz = reader.u64()?;
            let memsz = reader.u64()?;
            let flags = reader.u32()?;
            let offset = reader.u64()?;
            pending_segments.push((images.len(), image.segments.len(), offset, filesz));
            image.segments.push(SegmentMeta { vaddr, memsz, flags, data: Vec::new() });
        }
        for _ in 0..reader.u32()? {
            image.phdrs.push(PhdrMeta { kind: reader.u32()?, flags: reader.u32()?, vaddr: reader.u64()?, memsz: reader.u64()? });
        }
        if reader.u8()? != 0 {
            image.tls = Some(TlsMeta { init_vaddr: reader.u64()?, init_size: reader.u64()?, mem_size: reader.u64()?, align: reader.u64()? });
        }
        image.entry = reader.option()?;
        image.init = reader.option()?;
        image.fini = reader.option()?;
        image.init_array = reader.array()?;
        image.fini_array = reader.array()?;
        for _ in 0..reader.u32()? {
            image.needed.push(reader.string()?);
        }
        for _ in 0..reader.u32()? {
            let nid = reader.string()?;
            let vaddr = reader.u64()?;
            let kind = if reader.u8()? == 0 { ExportKind::Function } else { ExportKind::Data };
            image.exports.push(ExportMeta { nid, vaddr, kind });
        }
        for _ in 0..reader.u32()? {
            let offset = reader.u64()?;
            let kind = RelocKind::from_byte(reader.u8()?)?;
            image.relocations.push(RelocMeta { offset, kind, addend: reader.u64()? as i64, target: reader.u32()? });
        }
        images.push(image);
    }
    let mut symbols = Vec::new();
    for _ in 0..reader.u32()? {
        symbols.push(SymbolRef { image: reader.u32()?, vaddr: reader.u64()? });
    }
    let mut stubs = Vec::new();
    for _ in 0..stub_count {
        stubs.push(reader.string()?);
    }
    let mut host_data = Vec::new();
    for _ in 0..reader.u32()? {
        host_data.push(reader.string()?);
    }
    let blob_start = reader.u64()? as usize;
    if blob_start != reader.position {
        return Err(Error::new("image metadata blob offset does not follow the tables"));
    }
    let blob = &bytes[blob_start..];
    let slice = |offset: u64, length: u64| -> Result<Vec<u8>> {
        let start = offset as usize;
        let end = start.checked_add(length as usize).filter(|&end| end <= blob.len());
        Ok(blob[start..end.ok_or_else(|| Error::new("image metadata blob is truncated"))?].to_vec())
    };
    for (image, segment, offset, length) in pending_segments {
        images[image].segments[segment].data = slice(offset, length)?;
    }
    let addr_map = slice(map_offset, map_size)?;
    Ok(Metadata { images, stubs, host_data, symbols, addr_map })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_keeps_every_field() {
        let metadata = Metadata {
            images: vec![ImageMeta {
                name: "app.elf".into(),
                span: 0x8000,
                align: 0x4000,
                segments: vec![SegmentMeta { vaddr: 0, memsz: 0x20, flags: 5, data: vec![1, 2, 3] }],
                phdrs: vec![PhdrMeta { kind: 0x6474_e550, flags: 4, vaddr: 0x100, memsz: 0x10 }],
                tls: Some(TlsMeta { init_vaddr: 0x200, init_size: 8, mem_size: 16, align: 16 }),
                entry: Some(0x10),
                init: None,
                fini: Some(0x30),
                init_array: Some((0x400, 2)),
                fini_array: None,
                needed: vec!["libkernel.prx".into()],
                exports: vec![ExportMeta { nid: "AAAAAAAAAAA".into(), vaddr: 0x40, kind: ExportKind::Data }],
                relocations: vec![RelocMeta { offset: 0x400, kind: RelocKind::TlsOffset, addend: -8, target: 0 }],
            }],
            stubs: vec!["BBBBBBBBBBB".into()],
            host_data: vec!["CCCCCCCCCCC".into()],
            symbols: vec![SymbolRef { image: 0, vaddr: 0x10 }],
            addr_map: vec![9; 32],
        };
        assert_eq!(decode(&encode(&metadata)).unwrap(), metadata);
    }

    #[test]
    fn truncated_metadata_is_rejected() {
        let bytes = encode(&Metadata::default());
        assert!(decode(&bytes[..bytes.len() - 9]).is_err());
    }
}
