from pathlib import Path
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "launcher"))
import anyps5_launcher as launcher

ELF = b"\x7fELF" + bytes(60)
SELF = b"\x4f\x15\x3d\x1d" + bytes(60)


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def game(self, name, executable=ELF, title=None, modules=(), unity=()):
        folder = self.root / "games" / name
        write(folder / "eboot.bin", executable)
        if title is not None:
            params = {"titleId": title[0], "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": title[1]}}}
            write(folder / "sce_sys" / "param.json", json.dumps(params).encode())
        for module in modules:
            write(folder / "sce_module" / module, ELF)
        for module in unity:
            write(folder / module, ELF)
        return folder

    def test_scan_reads_param_json(self):
        self.game("a", title=("PPSA00001", "Game A"))
        self.game("nested/b")
        games = {g.path.name: g for g in launcher.scan_games(self.root / "games")}
        self.assertEqual(set(games), {"a", "b"})
        self.assertEqual((games["a"].title_id, games["a"].name, games["a"].kind), ("PPSA00001", "Game A", "elf"))
        self.assertEqual((games["b"].title_id, games["b"].name), ("", "b"))
        self.assertTrue(games["a"].key.startswith("PPSA00001-"))
        self.assertNotEqual(launcher.Game(self.root / "x", "", "PPSA00001", None, None, "elf", None).key, games["a"].key)

    def test_scan_reports_self(self):
        self.game("s", executable=SELF)
        (game,) = launcher.scan_games(self.root / "games")
        self.assertEqual(game.kind, "self")

    def test_scan_prefers_decrypted_elf_next_to_self(self):
        folder = self.game("s", executable=SELF, title="PPSA00003")
        write(folder / "decrypted" / "eboot.bin", ELF)
        (game,) = launcher.scan_games(self.root / "games")
        self.assertEqual(game.kind, "elf")
        self.assertEqual(game.path, folder)
        self.assertEqual(game.elf, folder / "decrypted" / "eboot.bin")
        write(folder / "sce_module" / "libc.prx", SELF)
        write(folder / "decrypted" / "sce_module" / "libc.prx", ELF)
        out = self.root / "out"
        launcher.stage_source(game, out, lambda message: None)
        self.assertEqual((out / "src" / "sce_module" / "libc.prx").read_bytes(), ELF)
        self.assertEqual(launcher.extra_module_dirs(game), [])

    def test_scan_skips_folders_without_executable(self):
        (self.root / "games" / "empty" / "x").mkdir(parents=True)
        self.assertEqual(launcher.scan_games(self.root / "games"), [])
        self.assertEqual(launcher.scan_games(self.root / "missing"), [])

    def test_extra_module_dirs(self):
        self.game("u", modules=("libc.prx",), unity=("Media/Modules/Il2CppUserAssemblies.prx", "Media/Plugins/PSN.prx"))
        write(self.root / "games" / "u" / "Media" / "Data" / "level.prx", b"data")
        (game,) = launcher.scan_games(self.root / "games")
        self.assertEqual(launcher.extra_module_dirs(game), ["Media/Modules", "Media/Plugins"])
        self.assertEqual(launcher.module_dirs(game), ["sce_module", "Media/Modules", "Media/Plugins"])

    def test_fingerprint_follows_inputs(self):
        folder = self.game("f", modules=("libc.prx",))
        (game,) = launcher.scan_games(self.root / "games")
        relinker = self.root / "relinker"
        write(relinker, b"r")
        cfg = {"extra_args": ""}
        first = launcher.fingerprint(game, cfg, relinker)
        self.assertEqual(first, launcher.fingerprint(game, cfg, relinker))
        self.assertNotEqual(first, launcher.fingerprint(game, {"extra_args": "unused-filter=1"}, relinker))
        module = folder / "sce_module" / "libc.prx"
        module.write_bytes(ELF + b"changed")
        os.utime(module, (1, 1))
        self.assertNotEqual(first, launcher.fingerprint(game, cfg, relinker))

    def test_stage_rejects_self(self):
        self.game("s", executable=SELF)
        (game,) = launcher.scan_games(self.root / "games")
        with self.assertRaises(RuntimeError):
            launcher.stage_source(game, self.root / "out", lambda line: None)
        self.game("m", modules=("libc.prx",))
        write(self.root / "games" / "m" / "sce_module" / "libSceFace.prx", SELF)
        game = next(g for g in launcher.scan_games(self.root / "games") if g.path.name == "m")
        with self.assertRaises(RuntimeError):
            launcher.stage_source(game, self.root / "out2", lambda line: None)

    def test_stage_links_modules(self):
        self.game("u", modules=("libc.prx",), unity=("Media/Modules/Il2CppUserAssemblies.prx",))
        write(self.root / "games" / "u" / "sce_module" / "libc.prx.esbak", b"backup")
        (game,) = launcher.scan_games(self.root / "games")
        elf = launcher.stage_source(game, self.root / "out", lambda line: None)
        src = self.root / "out" / "src"
        self.assertEqual(elf, src / "eboot.elf")
        self.assertEqual(sorted(p.relative_to(src).as_posix() for p in src.rglob("*") if p.is_file()),
                         ["Media/Modules/Il2CppUserAssemblies.prx", "eboot.elf", "sce_module/libc.prx"])

    def test_build_compiler_from_cmake_cache(self):
        write(self.root / "build" / "CMakeCache.txt", b"CMAKE_BUILD_TYPE:STRING=Release\nCMAKE_CXX_COMPILER:STRING=F:/w/mingw64/bin/g++.exe\n")
        cfg = {"anyps5_dir": str(self.root)}
        self.assertEqual(launcher.build_compiler(cfg), Path("F:/w/mingw64/bin/g++.exe"))
        self.assertEqual(launcher.mingw_dirs(cfg)[0], Path("F:/w/mingw64/bin"))
        self.assertIsNone(launcher.build_compiler({"anyps5_dir": str(self.root / "none")}))
        write(self.root / "build" / "CMakeCache.txt", b"CMAKE_CXX_COMPILER:UNINITIALIZED=aps5-missing-g++\n")
        self.assertIsNone(launcher.build_compiler(cfg))
        write(self.root / "build" / "CMakeCache.txt",
              b"CMAKE_CXX_COMPILER:UNINITIALIZED=aps5-missing-g++\nCMAKE_CXX_COMPILER_AR:FILEPATH=F:/w/mingw64/bin/gcc-ar.exe\n")
        self.assertEqual(launcher.mingw_dirs(cfg)[0], Path("F:/w/mingw64/bin"))

    def test_fingerprint_tracks_same_size_subsecond_updates(self):
        for kind in ("executable", "module", "relinker"):
            with self.subTest(kind=kind):
                folder = self.game("subsecond-" + kind, modules=("libc.prx",))
                game = next(g for g in launcher.scan_games(self.root / "games") if g.path == folder)
                relinker = self.root / ("relinker-" + kind)
                write(relinker, b"relinker")
                target = {"executable": game.elf, "module": folder / "sce_module" / "libc.prx", "relinker": relinker}[kind]
                os.utime(target, ns=(1700000000100000000, 1700000000100000000))
                original = launcher.fingerprint(game, {"extra_args": ""}, relinker)
                contents = target.read_bytes()
                target.write_bytes(contents[:-1] + bytes([contents[-1] ^ 1]))
                os.utime(target, ns=(1700000000900000000, 1700000000900000000))
                self.assertNotEqual(original, launcher.fingerprint(game, {"extra_args": ""}, relinker))

    def test_scan_handles_non_object_params(self):
        self.game("valid", title=("PPSA00001", "Game A"))
        folder = self.game("invalid")
        for value in (None, [], "title", 42, True):
            with self.subTest(value=value):
                write(folder / "sce_sys" / "param.json", json.dumps(value).encode())
                games = {g.path.name: g for g in launcher.scan_games(self.root / "games")}
                self.assertEqual(len(games), 2)
                self.assertEqual((games[folder.name].title_id, games[folder.name].name), ("", folder.name))
                self.assertEqual((games["valid"].title_id, games["valid"].name), ("PPSA00001", "Game A"))

    def test_sync_libs_refreshes_same_size_rollback(self):
        for source_time, cached_time in ((100, 200), (200, 100)):
            with self.subTest(source_time=source_time):
                out = self.root / ("cache-" + str(source_time))
                source = self.root / "built" / "libSceExample.prx"
                cached = out / "libs" / source.name
                write(source, b"selected")
                write(cached, b"previous")
                os.utime(source, (source_time, source_time))
                os.utime(cached, (cached_time, cached_time))
                with patch.object(launcher, "IS_WIN", False):
                    launcher.sync_libs(out, {"libs_dir": str(source.parent)}, lambda line: None)
                self.assertEqual(cached.read_bytes(), b"selected")

    def test_emulator_command(self):
        game = launcher.Game(Path("/g/x"), "X", "PPSA00002", None, Path("/g/x/eboot.bin"), "elf", None)
        command = launcher.emulator_command('emu --game "{dir}" --id {id} {elf}', game)
        self.assertEqual(command[0], "emu")
        self.assertIn("PPSA00002", command)
        self.assertEqual(Path(command[-1]), Path("/g/x/eboot.bin"))
        self.assertEqual(Path(command[2]), Path("/g/x"))


if __name__ == "__main__":
    unittest.main()
