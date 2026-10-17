use std::collections::HashMap;

use crate::image::{Image, SymbolKind};
use crate::meta::{RelocKind, RelocMeta, SymbolRef};
use crate::{Error, Result};

pub const R_X86_64_NONE: u32 = 0;
pub const R_X86_64_64: u32 = 1;
pub const R_X86_64_GLOB_DAT: u32 = 6;
pub const R_X86_64_JUMP_SLOT: u32 = 7;
pub const R_X86_64_RELATIVE: u32 = 8;
pub const R_X86_64_DTPMOD64: u32 = 16;
pub const R_X86_64_DTPOFF64: u32 = 17;
pub const R_X86_64_TPOFF64: u32 = 18;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Target {
    Guest { image: u32, vaddr: u64 },
    Import(u32),
}

#[derive(Default)]
pub struct Program {
    pub images: Vec<Image>,
    pub imports: Vec<String>,
    pub host_data: Vec<String>,
    pub symbols: Vec<SymbolRef>,
    pub relocations: Vec<Vec<RelocMeta>>,
    pub slots: Vec<HashMap<u64, Target>>,
}

enum Resolved {
    Guest { image: u32, value: u64, kind: SymbolKind },
    Host,
}

impl Program {
    pub fn bind(images: Vec<Image>) -> Result<Self> {
        let definitions: Vec<HashMap<&str, (u64, SymbolKind)>> =
            images.iter().map(|image| image.symbols.iter().filter_map(|symbol| Some((symbol.name.as_str(), (symbol.value?, symbol.kind)))).collect()).collect();
        let resolve = |name: &str| -> Resolved {
            for (index, table) in definitions.iter().enumerate() {
                if let Some(&(value, kind)) = table.get(name) {
                    return Resolved::Guest { image: index as u32, value, kind };
                }
            }
            Resolved::Host
        };

        let mut program = Program { relocations: vec![Vec::new(); images.len()], slots: vec![HashMap::new(); images.len()], ..Program::default() };
        let mut import_index: HashMap<String, u32> = HashMap::new();
        let mut data_index: HashMap<String, u32> = HashMap::new();
        let mut symbol_index: HashMap<(u32, u64), u32> = HashMap::new();

        for (image_number, image) in images.iter().enumerate() {
            let this = image_number as u32;
            let called: std::collections::HashSet<&str> = image
                .relocations
                .iter()
                .filter(|relocation| relocation.kind == R_X86_64_JUMP_SLOT)
                .filter_map(|relocation| relocation.symbol.map(|index| image.symbols[index].name.as_str()))
                .collect();
            for relocation in &image.relocations {
                let symbol = relocation.symbol.map(|index| &image.symbols[index]);
                let fail = |reason: &str| Error::new(format!("{}: relocation type {} at {:#x}: {reason}", image.name, relocation.kind, relocation.offset));
                let mut intern_symbol = |image: u32, vaddr: u64| -> u32 {
                    *symbol_index.entry((image, vaddr)).or_insert_with(|| {
                        program.symbols.push(SymbolRef { image, vaddr });
                        program.symbols.len() as u32 - 1
                    })
                };
                let (kind, addend, target) = match relocation.kind {
                    R_X86_64_NONE => continue,
                    R_X86_64_RELATIVE => (RelocKind::Relative, relocation.addend, 0),
                    R_X86_64_64 | R_X86_64_GLOB_DAT | R_X86_64_JUMP_SLOT => {
                        let symbol = symbol.ok_or_else(|| fail("no symbol"))?;
                        let addend = if relocation.kind == R_X86_64_64 { relocation.addend } else { 0 };
                        match resolve(&symbol.name) {
                            Resolved::Guest { image, value, kind } => {
                                if kind == SymbolKind::Function && addend == 0 {
                                    program.slots[image_number].insert(relocation.offset, Target::Guest { image, vaddr: value });
                                }
                                (RelocKind::GuestSymbol, addend, intern_symbol(image, value))
                            }
                            Resolved::Host => {
                                let function = match symbol.kind {
                                    SymbolKind::Function => true,
                                    SymbolKind::Data => false,
                                    SymbolKind::NoType => called.contains(symbol.name.as_str()),
                                    SymbolKind::Tls => return Err(fail(&format!("TLS symbol {} in a non-TLS relocation", symbol.name))),
                                };
                                if function {
                                    let next = import_index.len() as u32;
                                    let index = *import_index.entry(symbol.name.clone()).or_insert(next);
                                    if index == next {
                                        program.imports.push(symbol.name.clone());
                                    }
                                    if addend == 0 {
                                        program.slots[image_number].insert(relocation.offset, Target::Import(index));
                                    }
                                    (RelocKind::ImportStub, addend, index)
                                } else {
                                    let next = data_index.len() as u32;
                                    let index = *data_index.entry(symbol.name.clone()).or_insert(next);
                                    if index == next {
                                        program.host_data.push(symbol.name.clone());
                                    }
                                    (RelocKind::HostData, addend, index)
                                }
                            }
                        }
                    }
                    R_X86_64_DTPMOD64 | R_X86_64_DTPOFF64 | R_X86_64_TPOFF64 => {
                        let (image, value) = match symbol {
                            None => (this, 0),
                            Some(symbol) => match resolve(&symbol.name) {
                                Resolved::Guest { image, value, .. } => (image, value),
                                Resolved::Host => {
                                    return Err(fail(&format!("TLS symbol {} is not defined by a guest image", symbol.name)));
                                }
                            },
                        };
                        let kind = match relocation.kind {
                            R_X86_64_DTPMOD64 => RelocKind::TlsModuleId,
                            R_X86_64_DTPOFF64 => RelocKind::DtpOffset,
                            _ => RelocKind::TlsOffset,
                        };
                        let addend = if relocation.kind == R_X86_64_DTPMOD64 { 0 } else { relocation.addend };
                        (kind, addend, intern_symbol(image, value))
                    }
                    _ => return Err(fail("unsupported relocation type")),
                };
                program.relocations[image_number].push(RelocMeta { offset: relocation.offset, kind, addend, target });
            }
        }
        program.images = images;
        Ok(program)
    }

    pub fn slot_target(&self, image: u32, slot: u64) -> Option<Target> {
        self.slots[image as usize].get(&slot).copied()
    }
}
