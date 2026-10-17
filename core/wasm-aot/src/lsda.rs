#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CallSite {
    pub start: u64,
    pub length: u64,
    pub landing_pad: u64,
    pub action: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Lsda {
    pub call_sites: Vec<CallSite>,
    pub actions: u64,
}

pub fn parse(bytes: &[u8], address: u64, function_start: u64) -> Option<Lsda> {
    let mut reader = EncodedReader { bytes, position: 0, base: address };
    let lpstart_encoding = reader.u8()?;
    let lpstart = if lpstart_encoding == 0xff { function_start } else { reader.encoded(lpstart_encoding)? };
    let ttype_encoding = reader.u8()?;
    if ttype_encoding != 0xff {
        reader.uleb()?;
    }
    let call_site_encoding = reader.u8()?;
    let table_length = reader.uleb()? as usize;
    let table_end = reader.position + table_length;
    let mut call_sites = Vec::new();
    while reader.position < table_end {
        let start = reader.encoded(call_site_encoding)?;
        let length = reader.encoded(call_site_encoding)?;
        let pad = reader.encoded(call_site_encoding)?;
        let action = reader.uleb()?;
        call_sites.push(CallSite { start: lpstart.wrapping_add(start), length, landing_pad: if pad == 0 { 0 } else { lpstart.wrapping_add(pad) }, action });
    }
    Some(Lsda { call_sites, actions: address + table_end as u64 })
}

pub fn action_filter(bytes: &[u8], offset: usize) -> Option<i64> {
    let mut reader = EncodedReader { bytes, position: offset, base: 0 };
    reader.sleb()
}

pub(crate) struct EncodedReader<'a> {
    bytes: &'a [u8],
    position: usize,
    base: u64,
}

impl EncodedReader<'_> {
    fn u8(&mut self) -> Option<u8> {
        let value = *self.bytes.get(self.position)?;
        self.position += 1;
        Some(value)
    }

    fn fixed(&mut self, size: usize) -> Option<u64> {
        let slice = self.bytes.get(self.position..self.position + size)?;
        self.position += size;
        let mut buffer = [0u8; 8];
        buffer[..size].copy_from_slice(slice);
        Some(u64::from_le_bytes(buffer))
    }

    fn uleb(&mut self) -> Option<u64> {
        let mut result = 0u64;
        let mut shift = 0;
        loop {
            let byte = self.u8()?;
            result |= ((byte & 0x7f) as u64) << shift;
            if byte & 0x80 == 0 {
                return Some(result);
            }
            shift += 7;
        }
    }

    fn sleb(&mut self) -> Option<i64> {
        let mut result = 0i64;
        let mut shift = 0;
        loop {
            let byte = self.u8()?;
            result |= ((byte & 0x7f) as i64) << shift;
            shift += 7;
            if byte & 0x80 == 0 {
                if shift < 64 && byte & 0x40 != 0 {
                    result |= -1i64 << shift;
                }
                return Some(result);
            }
        }
    }

    fn encoded(&mut self, encoding: u8) -> Option<u64> {
        let place = self.base + self.position as u64;
        let value = match encoding & 0x0f {
            0x00 | 0x04 => self.fixed(8)?,
            0x01 => self.uleb()?,
            0x02 => self.fixed(2)?,
            0x03 => self.fixed(4)?,
            0x09 => self.sleb()? as u64,
            0x0a => self.fixed(2)? as i16 as i64 as u64,
            0x0b => self.fixed(4)? as i32 as i64 as u64,
            0x0c => self.fixed(8)?,
            _ => return None,
        };
        match encoding & 0x70 {
            0x00 => Some(value),
            0x10 => Some(place.wrapping_add(value)),
            _ => None,
        }
    }
}
