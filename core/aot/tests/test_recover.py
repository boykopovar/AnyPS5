import hashlib
import json
import subprocess
import struct
import tempfile
import unittest
from pathlib import Path

from recover import (RecoveryError, load_elf, masked_matches, materialize_contracts,
                     object_mappings, recover_image, verify_evidence)
from test_compiler import fixture


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='anyps5-aot-recovery-')
        self.directory = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def image(self, code, **kwargs):
        raw, _ = fixture(code, **kwargs)
        path = self.directory / 'image.elf'
        path.write_bytes(raw)
        return path

    def companion(self):
        source = self.directory / 'companion.S'
        source.write_text('.text\n.globl entry\n.type entry,@function\nentry:\n.cfi_startproc\n'
                          'mov $42,%eax\ncall imported\nret\n.cfi_endproc\n.size entry,.-entry\n')
        result = self.directory / 'companion.o'
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-c', str(source), '-o', str(result)],
                       check=True, capture_output=True)
        return result

    def evidence(self):
        source = self.directory / 'declaration.h'
        source.write_text('unsigned long entry(void);\n')
        return [{'path': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                 'excerpt': 'unsigned long entry(void);'}]

    def annotated_recovery(self):
        companion = self.companion()
        _, obj = load_elf(companion)
        code = obj.get_section_by_name('.text').data()
        recovered = recover_image(self.image(code), [companion])
        annotations = {'schema': 1, 'sha256': recovered['sha256'],
                       'functions': [{'address': 0x1000, 'size': len(code), 'name': 'entry',
                                      'result': 'i64', 'parameters': [], 'evidence': self.evidence()}], 'imports': {}}
        return recovered, annotations

    def test_mask_only_relocation_fields(self):
        self.assertEqual(masked_matches(b'ABC0000XYZ', [True] * 3 + [False] * 4 + [True] * 3,
                                        b'prefixABC1234XYZsuffix'), [6])
        self.assertEqual(masked_matches(b'ABC0000XYZ', [True] * 3 + [False] * 4 + [True] * 3,
                                        b'prefixABD1234XYZsuffix'), [])

    def test_fully_relocated_pattern_rejected(self):
        with self.assertRaisesRegex(RecoveryError, 'no invariant bytes'):
            masked_matches(b'1234', [False] * 4, b'1234')

    def test_linked_relocation_does_not_destroy_mapping(self):
        companion = self.companion()
        _, obj = load_elf(companion)
        code = bytearray(obj.get_section_by_name('.text').data())
        code[6:10] = bytes.fromhex('01020304')
        recovered = recover_image(self.image(code), [companion])
        candidate = recovered['functions'][0]
        self.assertEqual((candidate['symbol'], candidate['address'], candidate['size']), ('entry', 0x1000, 11))
        self.assertIsNone(candidate['abi'])
        self.assertIsNone(candidate['analysis']['stack_objects'])
        self.assertEqual(candidate['analysis']['instructions'], 3)
        self.assertEqual(recovered['companions'][0]['sections'][0]['relocated_bytes'], 4)

    def test_changed_nonrelocation_code_rejected(self):
        companion = self.companion()
        _, obj = load_elf(companion)
        code = bytearray(obj.get_section_by_name('.text').data())
        code[1] ^= 1
        with self.assertRaisesRegex(RecoveryError, '0 image matches'):
            recover_image(self.image(code), [companion])

    def test_ambiguous_companion_mapping_rejected(self):
        companion = self.companion()
        _, obj = load_elf(companion)
        code = obj.get_section_by_name('.text').data()
        with self.assertRaisesRegex(RecoveryError, '2 image matches'):
            recover_image(self.image(code + code), [companion])

    def test_import_and_data_relocations_are_facts_without_abi(self):
        recovered = recover_image(self.image(b'\xc3', imports={'nid#A#B': {}}, relocations=[(0x2800, 0x2900)]))
        self.assertEqual(recovered['imports'], [{'symbol': 'nid#A#B', 'slots': [0x2000], 'abi': None}])
        self.assertEqual(recovered['pointer_relocations'], [[0x2800, 0x2900]])
        self.assertEqual(recovered['functions'], [])

    def test_materialized_contract_requires_verified_evidence(self):
        recovery, annotations = self.annotated_recovery()
        result = materialize_contracts(recovery, annotations, self.directory)
        self.assertEqual(result['functions'][0]['result'], 'i64')
        self.assertNotIn('evidence', result['functions'][0])
        self.assertIn('annotations', result['provenance'])
        annotations['functions'][0]['evidence'] = []
        with self.assertRaisesRegex(RecoveryError, 'needs source evidence'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_modified_source_annotation_rejected(self):
        evidence = self.evidence()
        (self.directory / 'declaration.h').write_text('float entry(void);\n')
        with self.assertRaisesRegex(RecoveryError, 'stale source evidence'):
            verify_evidence(evidence, self.directory)

    def test_wrong_input_hash_rejected(self):
        recovery, annotations = self.annotated_recovery()
        annotations['sha256'] = '0' * 64
        with self.assertRaisesRegex(RecoveryError, 'input hash mismatch'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_missing_and_invented_imports_rejected(self):
        recovery, annotations = self.annotated_recovery()
        annotations['imports']['invented'] = {}
        with self.assertRaisesRegex(RecoveryError, 'do not match image imports'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_unsupported_image_obligation_is_not_silently_dropped(self):
        recovery, annotations = self.annotated_recovery()
        recovery['unknowns'] = [{'kind': 'tls', 'size': 64}]
        with self.assertRaisesRegex(RecoveryError, 'obligations remain unresolved'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_annotation_does_not_override_verified_function_size(self):
        recovery, annotations = self.annotated_recovery()
        annotations['functions'][0]['size'] += 1
        with self.assertRaisesRegex(RecoveryError, 'range differs'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_eh_frame_recovers_range_without_inventing_signature(self):
        companion = self.companion()
        _, obj = load_elf(companion)
        code = obj.get_section_by_name('.text').data()
        unwind = bytearray(obj.get_section_by_name('.eh_frame').data())
        for relocation in obj.get_section_by_name('.rela.eh_frame').iter_relocations():
            self.assertEqual(relocation['r_info_type'], 2)
            offset = relocation['r_offset']
            struct.pack_into('<i', unwind, offset, 0x1000 + relocation['r_addend'] - (0x2700 + offset))
        raw, _ = fixture(code)
        raw = bytearray(raw)
        raw[0x2700:0x2700 + len(unwind)] = unwind
        name_size = struct.unpack_from('<Q', raw, 0x5100 + 32)[0]
        raw[0x2500 + name_size:0x2500 + name_size + 10] = b'.eh_frame\0'
        struct.pack_into('<Q', raw, 0x5100 + 32, name_size + 10)
        struct.pack_into('<IIQQQQIIQQ', raw, 0x5140, name_size, 1, 2, 0x2700, 0x2700, len(unwind), 0, 0, 8, 0)
        struct.pack_into('<H', raw, 60, 6)
        path = self.directory / 'unwind.elf'
        path.write_bytes(raw)
        recovery = recover_image(path)
        self.assertEqual(len(recovery['functions']), 1)
        candidate = recovery['functions'][0]
        self.assertEqual((candidate['address'], candidate['size']), (0x1000, len(code)))
        self.assertIsNone(candidate['abi'])
        self.assertIsNone(candidate['symbol'])
        self.assertEqual(candidate['evidence'][0]['kind'], 'eh-frame-range')

    def test_conflicting_boundary_evidence_rejected(self):
        recovery, annotations = self.annotated_recovery()
        conflict = dict(recovery['functions'][0])
        conflict['size'] += 1
        recovery['functions'].append(conflict)
        with self.assertRaisesRegex(RecoveryError, 'conflicting function range evidence'):
            materialize_contracts(recovery, annotations, self.directory)

    def test_duplicate_annotation_rejected(self):
        recovery, annotations = self.annotated_recovery()
        annotations['functions'].append(dict(annotations['functions'][0]))
        with self.assertRaisesRegex(RecoveryError, 'duplicate function annotation'):
            materialize_contracts(recovery, annotations, self.directory)


if __name__ == '__main__':
    unittest.main()
