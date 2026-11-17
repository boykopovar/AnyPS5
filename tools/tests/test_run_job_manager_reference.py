from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_job_manager_reference as ref


class CasesTests(unittest.TestCase):
    def test_parity_excludes_only_stalling_cases(self):
        self.assertEqual(set(ref.CASES) - set(ref.PARITY_CASES), {"node_pool", "worker_wait"})
        self.assertEqual(len(set(ref.CASES)), len(ref.CASES))


class ResolveTests(unittest.TestCase):
    def test_missing_path_raises_file_not_found(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(FileNotFoundError):
                ref.resolve_existing_cli_path(Path(root) / "absent")

    def test_existing_path_resolves(self):
        with tempfile.TemporaryDirectory() as root:
            target = Path(root) / "present"
            target.write_bytes(b"")
            self.assertEqual(ref.resolve_existing_cli_path(target), target.resolve())


class StageLibrariesTests(unittest.TestCase):
    def test_missing_prx_is_an_error_and_unknown_dll_is_skipped(self):
        with tempfile.TemporaryDirectory() as root:
            build = Path(root) / "build"
            (build / "core/libs/libs").mkdir(parents=True)
            out = Path(root) / "out"
            out.mkdir()
            ref.stage_libraries(out, build, None, ["kernel32.dll"])
            self.assertEqual(list(out.iterdir()), [])
            with self.assertRaisesRegex(RuntimeError, "Missing patched runtime libfoo.prx"):
                ref.stage_libraries(out, build, None, ["libFoo.prx"])

    def test_stages_provider_case_insensitively(self):
        with tempfile.TemporaryDirectory() as root:
            build = Path(root) / "build"
            libs = build / "core/libs/libs"
            libs.mkdir(parents=True)
            (libs / "Libc.prx").write_bytes(b"x")
            out = Path(root) / "out"
            out.mkdir()
            original = ref.read_host_imports
            ref.read_host_imports = lambda path: []
            try:
                ref.stage_libraries(out, build, None, ["LIBC.PRX"])
            finally:
                ref.read_host_imports = original
            self.assertEqual((out / "Libc.prx").read_bytes(), b"x")


if __name__ == "__main__":
    unittest.main()
