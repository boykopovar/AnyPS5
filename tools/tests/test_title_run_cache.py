import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import title_run


MACHINE = {"os": "Fixture OS", "cpu": "Fixture CPU", "gpu": "Fixture GPU", "driver": "Fixture driver"}
DIRECTORIES = ("MESA_SHADER_CACHE_DIR", "__GL_SHADER_DISK_CACHE_PATH")


def policy(root="/previous/driver-cache", extra=None, status="runner-defaults", overrides=()):
    return {"version": 1, "platform": "linux", "status": status,
            "environment": {**{name: root for name in DIRECTORIES}, **(extra or {})},
            "overrides": list(overrides), "driver_directory": root}


def recorded(policies):
    runs = [{"run": index, "cache": "cold" if index == 1 else "warm", "outcome": "rendering", "fps": 60.0,
             "first_error": "", "machine": dict(MACHINE), "cache_policy": value} for index, value in enumerate(policies, 1)]
    return {"build": {"commit": "a" * 40, "dirty": False, "note": ""}, "machine": dict(MACHINE), "seconds": 1,
            "runs": len(runs), "driver_cache": "isolated", "titles": {"PPSA00001": {
                "name": "Fixture", "version": "01.00", "seconds": 1, "missing_content": [], "runs": runs}}}


class CachePolicyFactsTests(unittest.TestCase):
    def module(self):
        module = getattr(title_run, "title_run_cache", None)
        self.assertIsNotNone(module, "configured cache policy module is absent")
        return module

    def test_default_records_child_cache_facts_and_normalizes_owned_paths(self):
        module = self.module()
        for directory in ("/one/driver-cache", "/two/driver-cache"):
            with self.subTest(directory=directory):
                environment = {**{name: directory for name in DIRECTORIES}, "UNRELATED_SECRET": "not recorded"}
                with mock.patch.object(sys, "platform", "linux"):
                    result = module.configured_cache_policy(environment, {}, Path(directory).parent)
                self.assertEqual(result, policy(directory))
                self.assertNotIn("UNRELATED_SECRET", json.dumps(result))
        self.assertEqual(module.recorded_cache_policy(policy("/one/driver-cache")),
                         module.recorded_cache_policy(policy("/two/driver-cache")))

    def test_explicit_empty_and_custom_controls_are_retained(self):
        module = self.module()
        cases = {"ANYPS5_SHADER_CACHE_DIR": "", "MESA_SHADER_CACHE_DIR": "/external",
                 "__GL_SHADER_DISK_CACHE_PATH": "/external", "MESA_DISK_CACHE_READ_ONLY_FOZ_DBS": "seed",
                 "MESA_SHADER_CACHE_DISABLE": "false", "__GL_SHADER_DISK_CACHE": "1"}
        for name, value in cases.items():
            with self.subTest(name=name), mock.patch.object(sys, "platform", "linux"):
                environment = {**{key: "/work/driver-cache" for key in DIRECTORIES}, name: value}
                result = module.configured_cache_policy(environment, {name: value}, Path("/work"))
                self.assertEqual(result["status"], "overridden")
                self.assertEqual(result["environment"][name], value)
                self.assertEqual(result["overrides"], [name])

    def test_disable_requests_follow_project_and_documented_control_forms(self):
        module = self.module()
        for name, value in (("ANYPS5_NO_SHADER_CACHE", "1"), ("ANYPS5_NO_SHADER_CACHE", "false"),
                            ("MESA_SHADER_CACHE_DISABLE", "true"), ("MESA_SHADER_CACHE_DISABLE", "1"),
                            ("__GL_SHADER_DISK_CACHE", "0")):
            with self.subTest(name=name, value=value), mock.patch.object(sys, "platform", "linux"):
                environment = {**{key: "/work/driver-cache" for key in DIRECTORIES}, name: value}
                result = module.configured_cache_policy(environment, {}, Path("/work"))
                self.assertEqual(result["status"], "disable-requested")
                self.assertEqual(result["environment"][name], value)
        for value in ("", "0"):
            with self.subTest(anyps5=value), mock.patch.object(sys, "platform", "linux"):
                environment = {**{key: "/work/driver-cache" for key in DIRECTORIES}, "ANYPS5_NO_SHADER_CACHE": value}
                self.assertEqual(module.configured_cache_policy(environment, {}, Path("/work"))["status"], "overridden")

    def test_unknown_platforms_do_not_claim_isolation(self):
        module = self.module()
        for platform in ("win32", "darwin"):
            with self.subTest(platform=platform), mock.patch.object(sys, "platform", platform):
                result = module.configured_cache_policy({name: "/work/driver-cache" for name in DIRECTORIES}, {}, Path("/work"))
                self.assertEqual(result["status"], "unknown")

    def test_aggregate_requires_every_recorded_policy(self):
        module = self.module()
        self.assertIn("unknown", module.common_cache_policy([]))
        self.assertIn("unknown", module.common_cache_policy([policy(), None]))
        self.assertIn("mixed", module.common_cache_policy([policy(), policy(extra={"MESA_SHADER_CACHE_DISABLE": "true"}, status="disable-requested")]))
        self.assertIn("runner-owned", module.common_cache_policy([policy("/one/driver-cache"), policy("/two/driver-cache")]))

    def test_legacy_and_malformed_policy_are_unknown(self):
        module = self.module()
        cases = [None, {}, [], "isolated", dict(policy(), version=2), dict(policy(), version=True),
                 dict(policy(), environment=[]), dict(policy(), driver_directory=""), dict(policy(), overrides=["missing"]),
                 dict(policy(), environment={"MESA_SHADER_CACHE_DIR": None}), dict(policy(), status="disable-requested"),
                 dict(policy(), environment={}, status="overridden"),
                 policy(extra={"MESA_SHADER_CACHE_EXTRA": "\x00"}, status="overridden"),
                 policy(extra={"MESA_SHADER_CACHE_BAD=NAME": "value"}, status="overridden")]
        for item in cases:
            with self.subTest(item=item):
                self.assertIsNone(module.recorded_cache_policy(item))


class CachePolicyContextTests(unittest.TestCase):
    def compared(self, before, now):
        return json.loads(title_run.compare_json(now, before))["titles"]["PPSA00001"]

    def test_default_paths_from_different_output_roots_match(self):
        compared = self.compared(recorded([policy("/one/driver-cache")]), recorded([policy("/two/driver-cache")]))
        self.assertIn("runCachePolicies", compared["context"]["before"])
        self.assertEqual(compared["context"]["status"], "matched")
        self.assertEqual(compared["context"]["before"]["runCachePolicies"], compared["context"]["now"]["runCachePolicies"])

    def test_each_changed_cache_control_prevents_a_matched_claim(self):
        cases = [("ANYPS5_SHADER_CACHE_DIR", "/external"), ("MESA_SHADER_CACHE_DIR", "/external"),
                 ("__GL_SHADER_DISK_CACHE_PATH", "/external"), ("ANYPS5_SHADER_CACHE_DIR", ""),
                 ("ANYPS5_NO_SHADER_CACHE", "1"), ("MESA_SHADER_CACHE_DISABLE", "true"),
                 ("__GL_SHADER_DISK_CACHE", "0"), ("MESA_DISK_CACHE_READ_ONLY_FOZ_DBS", "seed")]
        for name, value in cases:
            with self.subTest(name=name, value=value):
                changed = policy(extra={name: value}, status="disable-requested" if name.endswith("DISABLE") or name in
                                 ("ANYPS5_NO_SHADER_CACHE", "__GL_SHADER_DISK_CACHE") else "overridden", overrides=[name])
                before, now = recorded([policy()]), recorded([changed])
                now["titles"]["PPSA00001"]["runs"][0]["outcome"] = "blank"
                self.assertEqual(title_run.compare(now, before), ([], []))
                compared = self.compared(before, now)
                self.assertEqual(compared["context"]["status"], "differing")
                self.assertIn("runCachePolicies", compared["context"]["differences"])
                self.assertEqual(compared["fps"], {"before": 60.0, "now": 60.0})

    def test_policies_compare_every_run_and_do_not_equate_custom_paths(self):
        before = recorded([policy(), policy()])
        changed = policy(extra={"MESA_SHADER_CACHE_DIR": "/external-a"}, status="overridden", overrides=["MESA_SHADER_CACHE_DIR"])
        now = recorded([changed, policy()])
        self.assertIn("runCachePolicies", self.compared(before, now)["context"]["differences"])
        other = copy.deepcopy(now)
        other["titles"]["PPSA00001"]["runs"][0]["cache_policy"]["environment"]["MESA_SHADER_CACHE_DIR"] = "/external-b"
        self.assertIn("runCachePolicies", self.compared(now, other)["context"]["differences"])

    def test_legacy_and_malformed_records_do_not_borrow_global_isolated(self):
        before = recorded([policy()])
        for missing in (True, False):
            with self.subTest(missing=missing):
                now = recorded([policy()])
                if missing:
                    del now["titles"]["PPSA00001"]["runs"][0]["cache_policy"]
                else:
                    now["titles"]["PPSA00001"]["runs"][0]["cache_policy"] = {}
                compared = self.compared(before, now)
                self.assertEqual(compared["context"]["status"], "unknown")
                self.assertIn("runCachePolicies", compared["context"]["unknowns"]["now"])
                self.assertIsNone(compared["context"]["now"]["driverCache"])
                self.assertEqual(compared["fps"], {"before": 60.0, "now": 60.0})

    def test_selected_title_policy_does_not_borrow_mixed_batch_aggregate(self):
        before, now = recorded([policy()]), recorded([policy()])
        now["driver_cache"] = "mixed or unknown"
        now["titles"]["PPSA00002"] = copy.deepcopy(now["titles"]["PPSA00001"])
        now["titles"]["PPSA00002"]["runs"][0]["cache_policy"] = policy(extra={"MESA_SHADER_CACHE_DISABLE": "true"}, status="disable-requested")
        compared = self.compared(before, now)
        self.assertEqual(compared["context"]["status"], "matched")


class CacheCliFixture(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        subprocess.run(["git", "init", "-q", str(self.source)], check=True, capture_output=True)
        subprocess.run(["git", "-C", str(self.source), "-c", "user.name=Fixture", "-c", "user.email=fixture@invalid",
                        "-c", "commit.gpgsign=false", "-c", f"core.hooksPath={os.devnull}", "commit", "-q", "--allow-empty", "-m", "test: cache policy fixture"],
                       check=True, capture_output=True)
        self.commit = subprocess.check_output(["git", "-C", str(self.source), "rev-parse", "HEAD"], text=True).strip()
        self.build = self.root / "build"
        converter = self.build / "core/relinker/relinker"
        converter.parent.mkdir(parents=True)
        (self.build / "core/libs/libs").mkdir(parents=True)
        (self.build / "core/libs/libs/libmimalloc.so").write_bytes(b"runtime fixture, not executed")
        (self.build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={self.source}\nTRACY_ENABLE:BOOL=OFF\n")
        native = '''import json,os,struct,time
from pathlib import Path
root=Path(os.environ['FIXTURE_ROOT']).resolve()
keys=('MESA_SHADER_CACHE_DIR','MESA_SHADER_CACHE_DISABLE','__GL_SHADER_DISK_CACHE_PATH','__GL_SHADER_DISK_CACHE','ANYPS5_SHADER_CACHE_DIR','ANYPS5_NO_SHADER_CACHE')
settings={key:os.environ.get(key) for key in keys}
markers={}
for name in ('MESA_SHADER_CACHE_DIR','__GL_SHADER_DISK_CACHE_PATH','ANYPS5_SHADER_CACHE_DIR'):
    value=settings[name]
    if value:
        directory=Path(value).resolve()
        if root not in directory.parents:raise RuntimeError('fixture refuses unowned cache path')
        directory.mkdir(parents=True,exist_ok=True)
        markers[name]={'path':str(directory),'present_before_write':(directory/'marker').exists()}
for item in markers.values():Path(item['path'],'marker').write_text('protocol-only fixture')
trace=Path(os.environ['APS5_TRACE_DIR'])
(trace/'cache-observed.json').write_text(json.dumps({'settings':settings,'markers':markers}))
row={'commit':os.environ['FIXTURE_COMMIT'][:12],'dirty':False,'gpu':'Fixture GPU','driver':'Fixture driver','os':'Fixture OS','interval_start_s':0,'interval_end_s':1,'presents':60,'window':{'ms':1000}}
(trace/'aps5-summary.jsonl').write_text(json.dumps(row)+'\\n')
Path('anyps5-report.json').write_text(json.dumps({'format':'anyps5-report','version':1,'anyps5':{'commit':os.environ['FIXTURE_COMMIT'],'modified':False},'run':{'averageFps':60}}))
pixels=bytes([0,0,0,255,255,255,255,255])
Path('frame_1.bmp').write_bytes(struct.pack('<2sIHHI',b'BM',62,0,0,54)+struct.pack('<IiiHHIIiiII',40,2,-1,1,32,0,8,0,0,0,0)+pixels)
print('host CPU Fixture CPU: has fixture features',flush=True)
time.sleep(60)
'''
        converter.with_name("native.py").write_text("#!" + sys.executable + "\n" + native)
        converter.write_text("#!" + sys.executable + "\nimport pathlib,shutil,sys\np=pathlib.Path(sys.argv[-1]);shutil.copyfile(pathlib.Path(__file__).with_name('native.py'),p);p.chmod(0o700)\n")
        converter.chmod(0o700)
        for title_id in ("PPSA00001", "PPSA00002"):
            package = self.root / "games" / title_id
            (package / "sce_sys").mkdir(parents=True)
            (package / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
            (package / "sce_sys/param.json").write_text(json.dumps({"titleId": title_id, "contentVersion": "01.00"}))

    def run_cli(self, name, extra=None, inherited=None, titles=None, previous=None):
        root = self.root / name
        root.mkdir()
        external = root / "external"
        external.mkdir()
        (external / "marker").write_text("pre-existing owned test cache marker")
        entries = titles or {"PPSA00001": extra or {}}
        catalog = root / "titles.json"
        catalog.write_text(json.dumps({title_id: {"env": {"FIXTURE_ROOT": str(self.root), "FIXTURE_COMMIT": self.commit, **values}}
                                       for title_id, values in entries.items()}))
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(("MESA_SHADER_CACHE_", "MESA_DISK_CACHE_", "__GL_SHADER_DISK_CACHE", "ANYPS5_SHADER_CACHE"))
                       and key != "ANYPS5_NO_SHADER_CACHE"}
        environment.update(inherited or {})
        command = [sys.executable, title_run.__file__, "--games", str(self.root / "games"), "--build", str(self.build),
                   "--titles", str(catalog), "--out", str(root / "out"), "--runs", "2", "--seconds", "1", *entries]
        if previous:
            command += ["--compare", str(previous), "--json"]
        result = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        path = next((root / "out").glob("*/summary.json"))
        summary = json.loads(path.read_text())
        return summary, path, result, external

    def previous(self):
        data = recorded([policy(), policy()])
        data["build"]["commit"] = self.commit
        path = self.root / "previous.json"
        path.write_text(json.dumps(data))
        return path


@unittest.skipUnless(os.name == "posix" and sys.platform == "linux", "executable protocol fixture and Linux cache configuration")
class CachePolicyCliDefaultsTests(CacheCliFixture):
    def test_requested_sequence_is_annotated_with_recorded_default_policy(self):
        summary, path, result, external = self.run_cli("default", previous=self.previous())
        runs = summary["titles"]["PPSA00001"]["runs"]
        self.assertEqual([run["cache"] for run in runs], ["cold", "warm"])
        self.assertIn("cache_policy", runs[0])
        self.assertEqual([run["cache_policy"]["status"] for run in runs], ["runner-defaults", "runner-defaults"])
        self.assertNotEqual(summary["driver_cache"], "isolated")
        observations = [json.loads((path.parent / "PPSA00001" / f"run{index}" / "cache-observed.json").read_text()) for index in (1, 2)]
        self.assertTrue(all(not item["present_before_write"] for item in observations[0]["markers"].values()))
        self.assertTrue(all(item["present_before_write"] for item in observations[1]["markers"].values()))
        compared = json.loads(result.stdout)["titles"]["PPSA00001"]
        self.assertEqual(compared["context"]["status"], "matched")
        self.assertEqual(compared["provenance"]["now"]["status"], "matched")
        report = path.with_suffix(".md").read_text()
        self.assertIn("| Cold (requested) | Warm (requested) |", report)
        self.assertIn("requested", report.lower())
        self.assertIn("cache policy", report.lower())
        self.assertIn("written to", result.stderr)
        self.assertEqual((external / "marker").read_text(), "pre-existing owned test cache marker")


@unittest.skipUnless(os.name == "posix" and sys.platform == "linux", "executable protocol fixture and Linux cache configuration")
class CachePolicyCliOverrideTests(CacheCliFixture):
    def test_each_external_directory_remains_user_controlled_and_is_reported(self):
        for index, name in enumerate(("MESA_SHADER_CACHE_DIR", "__GL_SHADER_DISK_CACHE_PATH", "ANYPS5_SHADER_CACHE_DIR")):
            with self.subTest(name=name):
                external = self.root / f"cache-{index}"
                external.mkdir()
                (external / "marker").write_text("existing external marker")
                (external / "sentinel").write_bytes(b"private cache data retained")
                summary, path, result, unused = self.run_cli(str(index), {name: str(external)}, previous=self.previous())
                run = summary["titles"]["PPSA00001"]["runs"][0]
                observed = json.loads((path.parent / "PPSA00001/run1/cache-observed.json").read_text())
                self.assertEqual(observed["settings"][name], str(external))
                self.assertTrue(observed["markers"][name]["present_before_write"])
                self.assertIn("cache_policy", run)
                self.assertEqual(run["cache_policy"]["status"], "overridden")
                self.assertEqual(run["cache_policy"]["environment"][name], str(external))
                self.assertNotEqual(summary["driver_cache"], "isolated")
                self.assertNotEqual(json.loads(result.stdout)["titles"]["PPSA00001"]["context"]["status"], "matched")
                self.assertIn(str(external), path.with_suffix(".md").read_text())
                self.assertEqual((external / "sentinel").read_bytes(), b"private cache data retained")


@unittest.skipUnless(os.name == "posix" and sys.platform == "linux", "executable protocol fixture and Linux cache configuration")
class CachePolicyCliFlagsTests(CacheCliFixture):
    def test_explicit_and_inherited_disable_requests_are_preserved(self):
        controls = {"MESA_SHADER_CACHE_DISABLE": "true", "__GL_SHADER_DISK_CACHE": "0", "ANYPS5_NO_SHADER_CACHE": "1"}
        for name, extra, inherited in (("explicit", controls, {}), ("inherited", {}, {key: value for key, value in controls.items() if key != "ANYPS5_NO_SHADER_CACHE"})):
            with self.subTest(name=name):
                summary, path, result, unused = self.run_cli(name, extra, inherited, previous=self.previous())
                run = summary["titles"]["PPSA00001"]["runs"][0]
                observed = json.loads((path.parent / "PPSA00001/run1/cache-observed.json").read_text())
                for key, value in (extra or inherited).items():
                    self.assertEqual(observed["settings"][key], value)
                self.assertIn("cache_policy", run)
                self.assertEqual(run["cache_policy"]["status"], "disable-requested")
                self.assertNotEqual(json.loads(result.stdout)["titles"]["PPSA00001"]["context"]["status"], "matched")
                self.assertIn("disable", path.with_suffix(".md").read_text().lower())


@unittest.skipUnless(os.name == "posix" and sys.platform == "linux", "executable protocol fixture and Linux cache configuration")
class CachePolicyAggregateTests(CacheCliFixture):
    def test_failed_conversion_does_not_claim_known_cache_configuration(self):
        converter = self.build / "core/relinker/relinker"
        converter.write_text("#!" + sys.executable + "\nraise SystemExit(1)\n")
        summary, path, result, external = self.run_cli("failed", previous=self.previous())
        self.assertIn("unknown", summary["driver_cache"])
        self.assertTrue(all("cache_policy" not in run for run in summary["titles"]["PPSA00001"]["runs"]))
        compared = json.loads(result.stdout)["titles"]["PPSA00001"]
        self.assertIn("runCachePolicies", compared["context"]["unknowns"]["now"])
        self.assertEqual((external / "marker").read_text(), "pre-existing owned test cache marker")

    def test_mixed_titles_aggregate_without_borrowing_one_title_policy(self):
        summary, path, result, unused = self.run_cli("mixed", titles={"PPSA00001": {}, "PPSA00002": {"MESA_SHADER_CACHE_DISABLE": "true"}})
        self.assertNotEqual(summary["driver_cache"], "isolated")
        self.assertIn("mixed", summary["driver_cache"].lower())
        self.assertEqual(summary["titles"]["PPSA00001"]["runs"][0]["cache_policy"]["status"], "runner-defaults")
        self.assertEqual(summary["titles"]["PPSA00002"]["runs"][0]["cache_policy"]["status"], "disable-requested")
        report = path.with_suffix(".md").read_text()
        self.assertIn("PPSA00002", report)
        self.assertIn("disable", report.lower())


if __name__ == "__main__":
    unittest.main()
