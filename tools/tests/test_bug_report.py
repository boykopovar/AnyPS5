import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bug_report


class RedactTests(unittest.TestCase):
    def test_home_user_host_and_ipv4_are_replaced(self):
        text = "opened /home/ana/games/title.elf on serverfestivo from 192.168.1.20"
        out = bug_report.redact(text, home="/home/ana", user="ana", host="serverfestivo")
        self.assertEqual(out, "opened ~/games/title.elf on <host> from <ip>")

    def test_home_is_replaced_before_user(self):
        out = bug_report.redact("C:\\Users\\ana\\AppData", home="C:\\Users\\ana", user="ana", host="x1")
        self.assertEqual(out, "~\\AppData")

    def test_user_is_matched_as_a_whole_word(self):
        out = bug_report.redact("banana ana ana.txt ana-dir ANA", home="/home/ana", user="ana", host="")
        self.assertEqual(out, "banana <user> <user>.txt ana-dir ANA")

    def test_windows_home_matches_mixed_case_and_separators(self):
        out = bug_report.redact(
            r"opened c:/PROFILES\Example/games/title.elf and C:\Profiles\Example-backup\title.elf",
            home=r"C:\Profiles\Example", user="", host="")
        self.assertEqual(out, r"opened ~/games/title.elf and C:\Profiles\Example-backup\title.elf")

    def test_windows_home_before_a_period_is_redacted(self):
        out = bug_report.redact(
            r"Home is C:\Profiles\Example. Sibling is C:\Profiles\Example.old.",
            home=r"C:\Profiles\Example", user="", host="")
        self.assertEqual(out, r"Home is ~. Sibling is C:\Profiles\Example.old.")

    def test_windows_unc_home_matches_with_a_trailing_separator(self):
        out = bug_report.redact(
            r"opened //FILEHOST/Share\Example/games/title.elf",
            home="\\\\FileHost\\Share\\Example\\", user="", host="")
        self.assertEqual(out, "opened ~/games/title.elf")

    def test_posix_home_is_not_replaced_inside_a_sibling_directory(self):
        out = bug_report.redact(
            "opened /home/anabel/title.elf, /home/ana-backup/title.elf, /home/ana.old/title.elf and /home/ana/title.elf",
            home="/home/ana", user="", host="")
        self.assertEqual(out, "opened /home/anabel/title.elf, /home/ana-backup/title.elf, /home/ana.old/title.elf and ~/title.elf")

    def test_posix_home_before_a_period_is_redacted(self):
        out = bug_report.redact("Home is /home/ana. Sibling is /home/ana.old.", home="/home/ana", user="", host="")
        self.assertEqual(out, "Home is ~. Sibling is /home/ana.old.")

    def test_posix_home_matches_with_a_trailing_separator(self):
        out = bug_report.redact("opened /home/ana/title.elf", home="/home/ana/", user="", host="")
        self.assertEqual(out, "opened ~/title.elf")

    def test_short_or_empty_identities_are_not_replaced(self):
        text = "a b /x"
        self.assertEqual(bug_report.redact(text, home="/x", user="a", host="b"), "a b /x")

    def test_a_fully_qualified_host_name_is_replaced_whole(self):
        text = "resolved box.corp.example. and BOX.corp.example, then box and boxer"
        out = bug_report.redact(text, home="/home/ana", user="ana", host="box.corp.example")
        self.assertEqual(out, "resolved <host>. and <host>, then <host> and boxer")

    def test_text_without_matches_is_unchanged(self):
        text = "frames=120 averageFps=59.9 version 1.2.3"
        self.assertEqual(bug_report.redact(text, home="/home/ana", user="ana", host="box"), "frames=120 averageFps=59.9 version 1.2.3")


class SettingsTests(unittest.TestCase):
    identity = {"home": "/home/ana", "user": "ana", "host": "serverfestivo"}

    def test_only_anyps5_variables_are_listed_in_name_order(self):
        environ = {"PATH": "/usr/bin", "ANYPS5_ZETA": "1", "APS5_NO_LABEL_READ_TRAP": "1", "ANYPS5_ALPHA": "on", "HOME": "/home/ana", "XAPS5_OTHER": "x"}
        self.assertEqual(bug_report.settings_text(environ, self.identity), "ANYPS5_ALPHA=on\nANYPS5_ZETA=1\nAPS5_NO_LABEL_READ_TRAP=1\n")

    def test_values_are_redacted(self):
        environ = {"ANYPS5_INPUT_CONFIG": "/home/ana/cfg/serverfestivo.ini", "ANYPS5_DNS_OVERRIDE": "store.example=192.168.1.20"}
        text = bug_report.settings_text(environ, self.identity)
        self.assertIn("ANYPS5_INPUT_CONFIG=~/cfg/<host>.ini", text)
        self.assertIn("ANYPS5_DNS_OVERRIDE=store.example=<ip>", text)

    def test_no_variables_says_so(self):
        self.assertEqual(bug_report.settings_text({"PATH": "/x"}, self.identity), "No ANYPS5_ or APS5_ variables were set.\n")


class BundleTests(unittest.TestCase):
    identity = {"home": "/home/ana", "user": "ana", "host": "serverfestivo"}

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.report = self.root / "anyps5-report.json"
        self.report.write_text('{"game": {"name": "title"}, "run": {"path": "/home/ana/games/title.elf", "from": "10.0.0.7"}}', encoding="utf-8")
        self.log = self.root / "run.log"
        self.log.write_text("[sce] opened /home/ana/games/title.elf\nserverfestivo: 192.168.1.20 connected\n", encoding="utf-8")
        self.ini = self.root / "anyps5-input.ini"
        self.ini.write_text("Cross = KEY:F\n", encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def bundle(self, **kwargs):
        out = self.root / "bundle.zip"
        names = bug_report.make_bundle(self.report, out, log=kwargs.pop("log", self.log), input_config=kwargs.pop("input_config", self.ini), **self.identity)
        return out, names

    def test_ambiguous_or_nonfinite_report_does_not_create_bundle(self):
        cases = (
            ('{"run":{},"run":{"frames":5}}', "duplicate JSON"),
            ('{"run":{"frames":0,"frames":5}}', "duplicate JSON"),
            ('{"run":{"averageFps":NaN}}', "Out of range float"),
            ('{"run":{"averageFps":1e999}}', "Out of range float"),
        )
        for index, (text, error) in enumerate(cases):
            with self.subTest(text=text):
                out = self.root / f"invalid-{index}.zip"
                self.report.write_text(text, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, error):
                    bug_report.make_bundle(self.report, out, **self.identity)
                self.assertFalse(out.exists())

    def test_members_are_redacted_and_no_identity_text_remains(self):
        out, names = self.bundle()
        self.assertEqual(names, ["README.txt", "anyps5-input.ini", "anyps5-report.json", "anyps5.log", "host.txt"])
        with zipfile.ZipFile(out) as bundle:
            contents = {name: bundle.read(name).decode("utf-8") for name in bundle.namelist()}
        self.assertIn('"path": "~/games/title.elf"', contents["anyps5-report.json"])
        self.assertIn('"from": "<ip>"', contents["anyps5-report.json"])
        self.assertIn("[sce] opened ~/games/title.elf", contents["anyps5.log"])
        self.assertIn("<host>: <ip> connected", contents["anyps5.log"])
        for name, text in contents.items():
            for secret in ("/home/ana", "serverfestivo", "192.168.1.20", "10.0.0.7"):
                self.assertNotIn(secret, text, f"{secret} leaked into {name}")

    def test_bundle_keeps_an_unpaired_character_in_runtime_settings(self):
        out = self.root / "runtime-settings.zip"
        names = bug_report.make_bundle(self.report, out, environ={"APS5_TRACE_DIR": "capture\udcff"},
                                       **self.identity)
        with zipfile.ZipFile(out) as bundle:
            self.assertEqual(sorted(bundle.namelist()), names)
            self.assertEqual(bundle.read("anyps5-settings.txt").decode("utf-8"),
                             "APS5_TRACE_DIR=capture\\udcff\n")

    def test_escaped_home_paths_are_redacted_in_nested_json(self):
        cases = (
            ("windows", r"C:\Profiles\example", r"\games\title.elf"),
            ("unicode-linux", "/home/профиль", "/games/title.elf"),
        )
        for label, home, suffix in cases:
            with self.subTest(label=label):
                details = {"path": f"opened {home}{suffix}", "recovered": False, "frames": 3, "extra": None}
                report = {"run": {"details": [details]}}
                self.report.write_text(json.dumps(report, ensure_ascii=True), encoding="utf-8")
                out = self.root / f"{label}.zip"
                bug_report.make_bundle(self.report, out, **{**self.identity, "home": home})
                with zipfile.ZipFile(out) as bundle:
                    archived = json.loads(bundle.read("anyps5-report.json"))
                details["path"] = f"opened ~{suffix}"
                self.assertEqual(archived, report)

    def test_windows_username_case_is_redacted_in_report_and_log(self):
        identity = {"home": r"C:\Profiles\Example", "user": "Example", "host": ""}
        self.report.write_text(json.dumps({"run": {"note": "opened by EXAMPLE"}}), encoding="utf-8")
        self.log.write_text("account eXaMpLe; sibling EXAMPLE-backup\n", encoding="utf-8")
        out = self.root / "bundle.zip"
        bug_report.make_bundle(self.report, out, log=self.log, **identity)
        with zipfile.ZipFile(out) as bundle:
            self.assertEqual(json.loads(bundle.read("anyps5-report.json")), {"run": {"note": "opened by <user>"}})
            self.assertEqual(bundle.read("anyps5.log").decode("utf-8"), "account <user>; sibling EXAMPLE-backup\n")

    def test_json_object_keys_are_redacted(self):
        report = {
            "/home/ana/games/title.elf": {
                "ana": "opened by ana",
                "serverfestivo": {"10.0.0.7": "/home/ana/save.bin"},
            }
        }
        self.report.write_text(json.dumps(report), encoding="utf-8")
        out, _ = self.bundle()
        with zipfile.ZipFile(out) as bundle:
            raw = bundle.read("anyps5-report.json").decode("utf-8")
            archived = json.loads(raw)
        self.assertEqual(
            archived,
            {
                "~/games/title.elf": {
                    "<user>": "opened by <user>",
                    "<host>": {"<ip>": "~/save.bin"},
                }
            },
        )
        for secret in ("/home/ana", "ana", "serverfestivo", "10.0.0.7"):
            self.assertNotIn(secret, raw)

    def test_utf8_bom_report_is_redacted(self):
        self.report.write_text('{"path": "/home/ana/title.elf"}', encoding="utf-8-sig")
        out, _ = self.bundle()
        with zipfile.ZipFile(out) as bundle:
            self.assertEqual(json.loads(bundle.read("anyps5-report.json")), {"path": "~/title.elf"})

    def test_missing_log_and_input_are_left_out(self):
        _, names = self.bundle(log=self.root / "absent.log", input_config=None)
        self.assertEqual(names, ["README.txt", "anyps5-report.json", "host.txt"])

    def test_log_keeps_driver_versions_and_redacts_ipv4_endpoints(self):
        self.log.write_text(
            "[gpu]\ndriver_version: 621.14.0.0\ndriver_version: 580.95.5.1\n"
            "peer http://192.168.1.20:8080/ from ::ffff:127.000.0.001\n"
            "route 0.0.0.0 to 255.255.255.255\n"
            "host 999.192.168.1.20\n", encoding="utf-8")
        out, _ = self.bundle()
        with zipfile.ZipFile(out) as bundle:
            self.assertEqual(bundle.read("anyps5.log"),
                             b"[gpu]\ndriver_version: 621.14.0.0\ndriver_version: 580.95.5.1\n"
                             b"peer http://<ip>:8080/ from ::ffff:<ip>\nroute <ip> to <ip>\nhost 999.<ip>\n")

    def test_report_that_is_not_json_is_refused(self):
        self.report.write_text("not json", encoding="utf-8")
        with self.assertRaises(ValueError):
            self.bundle()
        self.assertFalse((self.root / "bundle.zip").exists())

    def test_existing_bundle_is_not_overwritten(self):
        (self.root / "bundle.zip").write_bytes(b"keep")
        with self.assertRaises(FileExistsError):
            self.bundle()
        self.assertEqual((self.root / "bundle.zip").read_bytes(), b"keep")

    def test_bundle_created_after_absence_check_is_not_overwritten(self):
        target = self.root / "bundle.zip"
        target.write_bytes(b"keep")
        with patch.object(Path, "exists", return_value=False):
            with self.assertRaises(FileExistsError):
                self.bundle()
        self.assertEqual(target.read_bytes(), b"keep")


class MainTests(unittest.TestCase):
    identity = {"home": "/home/ana", "user": "ana", "host": "serverfestivo"}

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.report = self.root / "anyps5-report.json"
        self.report.write_text('{"game": {"name": "title"}}', encoding="utf-8")
        self.log = self.root / "run.log"
        self.log.write_text("opened /home/ana/title.elf on serverfestivo\n", encoding="utf-8")
        self.patch = patch.object(bug_report, "identities", return_value=self.identity)
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.directory.cleanup()

    def run_main(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = bug_report.main(argv)
        return code, out.getvalue(), err.getvalue()

    def test_hostname_case_is_redacted_in_network_paths(self):
        self.log.write_text(
            "opened //SERVERFESTIVO/share/title.elf via serverfestivo.local\n"
            "opened //serverfestivo-backup/share/title.elf\n", encoding="utf-8")
        target = self.root / "bundle.zip"
        code, _, error = self.run_main(["--report", str(self.report), "--log", str(self.log),
                                        "--input-config", "", "--out", str(target)])
        self.assertEqual(code, 0, error)
        with zipfile.ZipFile(target) as bundle:
            self.assertEqual(bundle.read("anyps5.log").decode("utf-8"),
                             "opened //<host>/share/title.elf via <host>.local\n"
                             "opened //serverfestivo-backup/share/title.elf\n")

    def test_writes_the_bundle_with_identities_redacted(self):
        target = self.root / "bundle.zip"
        code, out, _ = self.run_main(["--report", str(self.report), "--log", str(self.log), "--input-config", str(self.root / "none.ini"), "--out", str(target)])
        self.assertEqual(code, 0)
        self.assertIn("wrote", out)
        with zipfile.ZipFile(target) as bundle:
            self.assertEqual(sorted(bundle.namelist()), ["README.txt", "anyps5-report.json", "anyps5-settings.txt", "anyps5.log", "host.txt"])
            self.assertEqual(bundle.read("anyps5.log").decode("utf-8"), "opened ~/title.elf on <host>\n")

    def test_bundle_redacts_environment_settings_once(self):
        target = self.root / "bundle.zip"
        identity = {"home": "C:/Users/user", "user": "user", "host": "host"}
        environ = {"ANYPS5_INPUT_CONFIG": "C:/Users/user/host.ini", "APS5_TEST": "user@192.0.2.1"}
        with patch.object(bug_report, "identities", return_value=identity), patch.dict(os.environ, environ, clear=True):
            code, _, err = self.run_main(["--report", str(self.report), "--log", "", "--input-config", "", "--out", str(target)])
        self.assertEqual(code, 0, err)
        with zipfile.ZipFile(target) as bundle:
            self.assertEqual(bundle.read("anyps5-settings.txt"), b"ANYPS5_INPUT_CONFIG=~/<host>.ini\nAPS5_TEST=<user>@<ip>\n")

    def test_colliding_redacted_keys_refuse_the_bundle(self):
        first, second = "192.0.2.1", "192.0.2.2"
        self.report.write_text(json.dumps({"addresses": [{first: 1, second: 2}]}), encoding="utf-8")
        target = self.root / "bundle.zip"
        code, out, err = self.run_main(["--report", str(self.report), "--out", str(target)])
        self.assertEqual(code, 1)
        self.assertEqual(out, "")
        self.assertIn("duplicate JSON object keys after redaction", err)
        self.assertNotIn(first, err)
        self.assertNotIn(second, err)
        self.assertFalse(target.exists())

    def test_missing_report_is_an_error(self):
        code, _, err = self.run_main(["--report", str(self.root / "absent.json"), "--out", str(self.root / "b.zip")])
        self.assertEqual(code, 1)
        self.assertIn("no run report", err)
        self.assertFalse((self.root / "b.zip").exists())

    def test_existing_bundle_is_refused(self):
        target = self.root / "bundle.zip"
        target.write_bytes(b"keep")
        code, _, err = self.run_main(["--report", str(self.report), "--log", "", "--input-config", "", "--out", str(target)])
        self.assertEqual(code, 1)
        self.assertIn("already exists", err)
        self.assertEqual(target.read_bytes(), b"keep")

    def test_log_defaults_to_the_environment_variable(self):
        target = self.root / "bundle.zip"
        with patch.dict(os.environ, {"ANYPS5_LOG_FILE": str(self.log)}):
            code, _, _ = self.run_main(["--report", str(self.report), "--input-config", "", "--out", str(target)])
        self.assertEqual(code, 0)
        with zipfile.ZipFile(target) as bundle:
            self.assertIn("anyps5.log", bundle.namelist())

    def test_input_config_defaults_to_the_environment_variable(self):
        ini = self.root / "map.ini"
        ini.write_text("Cross = KEY:/home/ana/F\n", encoding="utf-8")
        target = self.root / "bundle.zip"
        with patch.dict(os.environ, {"ANYPS5_INPUT_CONFIG": str(ini)}):
            code, _, _ = self.run_main(["--report", str(self.report), "--log", "", "--out", str(target)])
        self.assertEqual(code, 0)
        with zipfile.ZipFile(target) as bundle:
            self.assertEqual(bundle.read("anyps5-input.ini").decode("utf-8"), "Cross = KEY:~/F\n")

    def test_bundle_includes_the_profile_next_to_the_configured_input_mapping(self):
        self.report.write_text('{"game": {"titleId": "PPSA00001"}}', encoding="utf-8")
        config = self.root / "config"
        config.mkdir()
        ini = config / "controls.ini"
        ini.write_text("Cross = KEY:F\n", encoding="utf-8")
        (config / "anyps5-input.PPSA00001.ini").write_text(
            "# /home/ana/controls on serverfestivo\nCross = KEY:G\n", encoding="utf-8")
        (config / "anyps5-input.PPSA00002.ini").write_text("Cross = KEY:H\n", encoding="utf-8")
        target = self.root / "bundle.zip"
        with patch.dict(os.environ, {"ANYPS5_INPUT_CONFIG": str(ini)}):
            code, _, err = self.run_main(["--report", str(self.report), "--log", "", "--out", str(target)])
        self.assertEqual(code, 0, err)
        with zipfile.ZipFile(target) as bundle:
            self.assertEqual(sorted(name for name in bundle.namelist() if name.endswith(".ini")),
                             ["anyps5-input-profile.ini", "anyps5-input.ini"])
            self.assertEqual(bundle.read("anyps5-input.ini").decode("utf-8"), "Cross = KEY:F\n")
            self.assertEqual(bundle.read("anyps5-input-profile.ini").decode("utf-8"),
                             "# ~/controls on <host>\nCross = KEY:G\n")

    def test_bundle_includes_the_title_profile_without_a_default_shared_mapping(self):
        self.report.write_text('{"game": {"titleId": "PPSA00001"}}', encoding="utf-8")
        (self.root / "anyps5-input.PPSA00001.ini").write_text("Cross = KEY:G\n", encoding="utf-8")
        env = {name: value for name, value in os.environ.items() if name != "ANYPS5_INPUT_CONFIG"}
        result = subprocess.run([sys.executable, bug_report.__file__, "--log", ""], cwd=self.root, env=env,
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        with zipfile.ZipFile(self.root / "bug-report.zip") as bundle:
            self.assertNotIn("anyps5-input.ini", bundle.namelist())
            self.assertIn("anyps5-input-profile.ini", bundle.namelist())
            self.assertEqual(bundle.read("anyps5-input-profile.ini").decode("utf-8"), "Cross = KEY:G\n")

    def test_invalid_report_json_is_an_error(self):
        self.report.write_text("not json", encoding="utf-8")
        code, _, err = self.run_main(["--report", str(self.report), "--log", "", "--input-config", "", "--out", str(self.root / "b.zip")])
        self.assertEqual(code, 1)
        self.assertIn("bug_report:", err)
        self.assertFalse((self.root / "b.zip").exists())

    def test_non_ascii_paths_with_cp1252_stdio_are_reported_as_utf8(self):
        env = dict(os.environ, PYTHONIOENCODING="cp1252", PYTHONUTF8="0")
        out = self.root / "bundle-\u2603.zip"
        success = subprocess.run(
            [sys.executable, bug_report.__file__, "--report", str(self.report), "--log", "", "--input-config", "", "--out", str(out)],
            env=env, capture_output=True, timeout=5)
        self.assertEqual(success.returncode, 0, success.stderr.decode("utf-8", errors="replace"))
        self.assertTrue(out.is_file())
        self.assertIn(out.name, success.stdout.decode("utf-8"))

        missing = self.root / "missing-\u2603.json"
        failure = subprocess.run(
            [sys.executable, bug_report.__file__, "--report", str(missing), "--log", "", "--input-config", "", "--out", str(self.root / "unused.zip")],
            env=env, capture_output=True, timeout=5)
        self.assertEqual(failure.returncode, 1)
        self.assertIn(missing.name, failure.stderr.decode("utf-8"))


class IdentityTests(unittest.TestCase):
    def test_host_keeps_its_domain_so_both_forms_are_redacted(self):
        with patch.object(bug_report.getpass, "getuser", return_value="ana"), patch.object(bug_report.socket, "gethostname", return_value="box.example.org"):
            identity = bug_report.identities()
        self.assertEqual(identity["user"], "ana")
        self.assertEqual(identity["host"], "box.example.org")
        self.assertEqual(identity["home"], str(Path.home()))
        self.assertEqual(bug_report.redact("box.example.org box", **identity), "<host> <host>")

    def test_user_is_empty_when_it_cannot_be_determined(self):
        with patch.object(bug_report.getpass, "getuser", side_effect=OSError("no user")), patch.object(bug_report.socket, "gethostname", return_value="box"):
            self.assertEqual(bug_report.identities()["user"], "")


class RedactReportContainerTests(unittest.TestCase):
    def test_non_string_scalars_pass_through_and_containers_are_walked(self):
        identity = {"home": "/home/ana", "user": "ana", "host": "box1"}
        value = {"n": 3, "ok": True, "none": None, "list": ["/home/ana/x", 1.5, {"h": "box1"}]}
        self.assertEqual(bug_report.redact_report(value, identity), {"n": 3, "ok": True, "none": None, "list": ["~/x", 1.5, {"h": "<host>"}]})

class RedactReportTests(unittest.TestCase):
    IDENTITY = {"home": "/home/alice", "user": "alice", "host": "box"}

    def test_redacts_strings_in_lists_dict_keys_and_values(self):
        report = {"/home/alice/a": ["/home/alice/b", {"k": "alice"}], "n": 3, "flag": True, "none": None}
        expected = {
            bug_report.redact("/home/alice/a", **self.IDENTITY): [
                bug_report.redact("/home/alice/b", **self.IDENTITY),
                {"k": bug_report.redact("alice", **self.IDENTITY)},
            ],
            "n": 3,
            "flag": True,
            "none": None,
        }
        self.assertEqual(bug_report.redact_report(report, self.IDENTITY), expected)

    def test_leaves_non_string_scalars_and_does_not_mutate_input(self):
        report = [1, 2.5, None, ["/home/alice"]]
        self.assertEqual(bug_report.redact_report(report, self.IDENTITY)[:3], [1, 2.5, None])
        self.assertEqual(report[3], ["/home/alice"])


class HostTextTests(unittest.TestCase):
    def test_host_text_names_the_system_processor_cpus_and_python(self):
        with patch.object(bug_report.platform, "system", return_value="Linux"), \
                patch.object(bug_report.platform, "release", return_value="6.1.0"), \
                patch.object(bug_report.platform, "machine", return_value="x86_64"), \
                patch.object(bug_report.platform, "processor", return_value=""), \
                patch.object(bug_report.os, "cpu_count", return_value=8):
            text = bug_report.host_text()
        self.assertEqual(text.splitlines(), ["os: Linux 6.1.0", "machine: x86_64", "processor: unknown", "logical cpus: 8", f"python: {bug_report.platform.python_version()}"])


if __name__ == "__main__":
    unittest.main()
