use anyps5_wasm_aot::lsda;
use gimli::{BaseAddresses, CfaRule, EhFrame, EhFrameHdr, LittleEndian, Pointer, Register, RegisterRule, UnwindContext, UnwindSection};

#[derive(Clone, Copy, Debug)]
pub struct UnwindImage {
    pub base: u64,
    pub span: u64,
    pub eh_frame_hdr: Option<u64>,
}

pub struct Resume {
    pub gpr: [u64; 16],
    pub rip: u64,
}

const DWARF_TO_GPR: [usize; 16] = [0, 2, 1, 3, 6, 7, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15];
const RETURN_ADDRESS: u16 = 16;

pub fn find_landing_pad(images: &[UnwindImage], mut gpr: [u64; 16], exception: u64, read: &dyn Fn(u64, usize) -> Vec<u8>) -> Result<Resume, String> {
    let read_u64 = |address: u64| u64::from_le_bytes(read(address, 8).try_into().unwrap());
    let mut pc = read_u64(gpr[4]);
    gpr[4] += 8;
    for _ in 0..256 {
        let image = images.iter().find(|image| (image.base..image.base + image.span).contains(&(pc - 1))).ok_or(format!("no image holds {pc:#x}"))?;
        let hdr = image.eh_frame_hdr.ok_or("image without unwind data")?;
        let bytes = read(image.base, image.span as usize);
        let vaddr = pc - 1 - image.base;
        let bases = BaseAddresses::default().set_eh_frame_hdr(hdr);
        let parsed = EhFrameHdr::new(&bytes[hdr as usize..], LittleEndian).parse(&bases, 8).map_err(|error| error.to_string())?;
        let Pointer::Direct(eh_frame_address) = parsed.eh_frame_ptr() else { return Err("indirect eh_frame pointer".into()) };
        let eh_frame = EhFrame::new(&bytes[eh_frame_address as usize..], LittleEndian);
        let bases = bases.set_eh_frame(eh_frame_address);
        let table = parsed.table().ok_or("eh_frame_hdr without a table")?;
        let fde = table.fde_for_address(&eh_frame, &bases, vaddr, EhFrame::cie_from_offset).map_err(|error| format!("{pc:#x}: {error}"))?;

        if let Some(Pointer::Direct(lsda_address)) = fde.lsda() {
            let parsed_lsda = lsda::parse(&bytes[lsda_address as usize..], lsda_address, fde.initial_address()).ok_or("malformed LSDA")?;
            let site = parsed_lsda.call_sites.iter().find(|site| (site.start..site.start + site.length).contains(&vaddr));
            if let Some(site) = site.filter(|site| site.landing_pad != 0) {
                let selector = if site.action == 0 {
                    0
                } else {
                    let offset = (parsed_lsda.actions + site.action - 1) as usize;
                    lsda::action_filter(&bytes, offset).ok_or("malformed action record")? as u64
                };
                gpr[0] = exception;
                gpr[2] = selector;
                return Ok(Resume { gpr, rip: image.base + site.landing_pad });
            }
        }

        let mut context = UnwindContext::new();
        let row = fde.unwind_info_for_address(&eh_frame, &bases, &mut context, vaddr).map_err(|error| error.to_string())?;
        let cfa = match row.cfa() {
            CfaRule::RegisterAndOffset { register, offset } => gpr[DWARF_TO_GPR[register.0 as usize]].wrapping_add(*offset as u64),
            CfaRule::Expression(_) => return Err("CFA expressions are not supported".into()),
        };
        let mut restored = gpr;
        for dwarf in [3u16, 6, 12, 13, 14, 15] {
            if let Some(RegisterRule::Offset(offset)) = row.register(Register(dwarf)) {
                restored[DWARF_TO_GPR[dwarf as usize]] = read_u64(cfa.wrapping_add(offset as u64));
            }
        }
        let Some(RegisterRule::Offset(offset)) = row.register(Register(RETURN_ADDRESS)) else { return Err("no return address rule".into()) };
        pc = read_u64(cfa.wrapping_add(offset as u64));
        restored[4] = cfa;
        gpr = restored;
    }
    Err("too many frames".into())
}
