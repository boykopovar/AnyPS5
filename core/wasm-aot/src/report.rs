use std::collections::BTreeMap;
use std::fmt::Write;

use crate::discover::Discovery;
use crate::program::Program;

#[derive(Clone, Debug)]
pub struct Unsupported {
    pub image: u32,
    pub address: u64,
    pub code: String,
    pub text: String,
}

#[derive(Debug, Default)]
pub struct Report {
    pub images: Vec<String>,
    pub functions: usize,
    pub instructions: usize,
    pub imports: usize,
    pub wasm_bytes: usize,
    pub invalid: Vec<(u32, u64)>,
    pub unresolved_jumps: Vec<(u32, u64)>,
    pub pointer_jumps: usize,
    pub guessed_tables: Vec<(u32, u64)>,
    pub unsupported: Vec<Unsupported>,
}

impl Report {
    pub fn new(program: &Program, discovery: &Discovery, unsupported: Vec<Unsupported>, wasm_bytes: usize) -> Self {
        Self {
            images: program.images.iter().map(|image| image.name.clone()).collect(),
            functions: discovery.functions.len(),
            instructions: discovery.functions.iter().flat_map(|function| &function.blocks).map(|block| block.instructions.len()).sum(),
            imports: program.imports.len(),
            wasm_bytes,
            invalid: discovery.invalid.clone(),
            unresolved_jumps: discovery.unresolved_jumps.clone(),
            pointer_jumps: discovery.pointer_jumps.len(),
            guessed_tables: discovery.guessed_tables.clone(),
            unsupported,
        }
    }

    pub fn unsupported_by_code(&self) -> Vec<(String, usize, String)> {
        let mut groups: BTreeMap<&str, (usize, &str)> = BTreeMap::new();
        for entry in &self.unsupported {
            let group = groups.entry(&entry.code).or_insert((0, &entry.text));
            group.0 += 1;
        }
        let mut list: Vec<(String, usize, String)> =
            groups.into_iter().map(|(code, (count, example))| (code.to_string(), count, example.to_string())).collect();
        list.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
        list
    }

    pub fn to_json(&self) -> String {
        let mut json = String::from("{\n");
        let images: Vec<String> = self.images.iter().map(|name| format!("\"{}\"", escape(name))).collect();
        let _ = writeln!(json, "  \"images\": [{}],", images.join(", "));
        let _ = writeln!(json, "  \"functions\": {},", self.functions);
        let _ = writeln!(json, "  \"instructions\": {},", self.instructions);
        let _ = writeln!(json, "  \"imports\": {},", self.imports);
        let _ = writeln!(json, "  \"wasm_bytes\": {},", self.wasm_bytes);
        let addresses = |list: &[(u32, u64)]| list.iter().map(|(image, address)| format!("[{image}, \"{address:#x}\"]")).collect::<Vec<_>>().join(", ");
        let _ = writeln!(json, "  \"invalid\": [{}],", addresses(&self.invalid));
        let _ = writeln!(json, "  \"unresolved_jumps\": [{}],", addresses(&self.unresolved_jumps));
        let _ = writeln!(json, "  \"pointer_jumps\": {},", self.pointer_jumps);
        let _ = writeln!(json, "  \"guessed_tables\": [{}],", addresses(&self.guessed_tables));
        let groups: Vec<String> = self
            .unsupported_by_code()
            .into_iter()
            .map(|(code, count, example)| format!("    {{\"code\": \"{code}\", \"count\": {count}, \"example\": \"{}\"}}", escape(&example)))
            .collect();
        let _ = writeln!(json, "  \"unsupported\": [\n{}\n  ]", groups.join(",\n"));
        json.push('}');
        json
    }
}

fn escape(text: &str) -> String {
    text.replace('\\', "\\\\").replace('"', "\\\"")
}
