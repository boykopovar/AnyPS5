pub const PF_X: u32 = 1;
pub const PF_W: u32 = 2;
pub const PF_R: u32 = 4;

#[derive(Clone, Debug, Default)]
pub struct Segment {
    pub vaddr: u64,
    pub data: Vec<u8>,
    pub memsz: u64,
    pub flags: u32,
}

impl Segment {
    pub fn end(&self) -> u64 {
        self.vaddr + self.memsz
    }

    pub fn is_code(&self) -> bool {
        self.flags & PF_X != 0 && self.flags & PF_W == 0
    }

    pub fn is_executable(&self) -> bool {
        self.flags & PF_X != 0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SymbolKind {
    Function,
    Data,
    NoType,
    Tls,
}

#[derive(Clone, Debug)]
pub struct Symbol {
    pub name: String,
    pub kind: SymbolKind,
    pub value: Option<u64>,
    pub weak: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct Relocation {
    pub offset: u64,
    pub kind: u32,
    pub symbol: Option<usize>,
    pub addend: i64,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct Tls {
    pub init_vaddr: u64,
    pub init_size: u64,
    pub mem_size: u64,
    pub align: u64,
}

#[derive(Clone, Copy, Debug)]
pub struct Phdr {
    pub kind: u32,
    pub flags: u32,
    pub vaddr: u64,
    pub memsz: u64,
}

#[derive(Clone, Copy, Debug)]
pub struct Fde {
    pub start: u64,
    pub end: u64,
    pub lsda: Option<u64>,
}

#[derive(Clone, Debug, Default)]
pub struct Image {
    pub name: String,
    pub segments: Vec<Segment>,
    pub phdrs: Vec<Phdr>,
    pub tls: Option<Tls>,
    pub entry: Option<u64>,
    pub init: Option<u64>,
    pub fini: Option<u64>,
    pub init_array: Option<(u64, u32)>,
    pub fini_array: Option<(u64, u32)>,
    pub needed: Vec<String>,
    pub symbols: Vec<Symbol>,
    pub relocations: Vec<Relocation>,
    pub fdes: Vec<Fde>,
    pub extra_entries: Vec<u64>,
}

impl Image {
    pub fn raw(code: &[u8], vaddr: u64) -> Self {
        Self {
            name: "raw".into(),
            segments: vec![Segment { vaddr, data: code.to_vec(), memsz: code.len() as u64, flags: PF_R | PF_X }],
            fdes: vec![Fde { start: vaddr, end: vaddr + code.len() as u64, lsda: None }],
            ..Self::default()
        }
    }

    pub fn segment_at(&self, vaddr: u64) -> Option<&Segment> {
        self.segments.iter().find(|segment| (segment.vaddr..segment.end()).contains(&vaddr))
    }

    pub fn is_code(&self, vaddr: u64) -> bool {
        self.segment_at(vaddr).is_some_and(Segment::is_code)
    }

    pub fn is_executable(&self, vaddr: u64) -> bool {
        self.segment_at(vaddr).is_some_and(Segment::is_executable)
    }

    pub fn bytes_at(&self, vaddr: u64) -> Option<&[u8]> {
        let segment = self.segment_at(vaddr)?;
        let start = (vaddr - segment.vaddr) as usize;
        Some(segment.data.get(start..).unwrap_or(&[]))
    }

    pub fn read_u64(&self, vaddr: u64) -> Option<u64> {
        let bytes = self.bytes_at(vaddr)?;
        Some(u64::from_le_bytes(bytes.get(..8)?.try_into().ok()?))
    }

    pub fn relocated_pointer(&self, vaddr: u64) -> Option<u64> {
        let relative = self.relocations.iter().find(|relocation| relocation.offset == vaddr && relocation.kind == crate::program::R_X86_64_RELATIVE);
        relative.map(|relocation| relocation.addend as u64).or_else(|| self.read_u64(vaddr))
    }

    pub fn read_u32(&self, vaddr: u64) -> Option<u32> {
        let bytes = self.bytes_at(vaddr)?;
        Some(u32::from_le_bytes(bytes.get(..4)?.try_into().ok()?))
    }

    pub fn span(&self) -> u64 {
        self.segments.iter().map(Segment::end).max().unwrap_or(0)
    }

    pub fn align(&self) -> u64 {
        0x4000
    }
}
