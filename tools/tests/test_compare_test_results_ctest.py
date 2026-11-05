import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compare_test_results


def run_ctest(ctest, directory, tests):
    directory.mkdir()
    lines = []
    for name, kind in tests.items():
        command = {
            "passed": "pass", "failed": "raise SystemExit(1)", "skipped": "raise SystemExit(77)",
            "regex": "print('skip this test')", "disabled": "raise SystemExit(1)", "missing": "pass",
        }[kind]
        executable = directory / "no_such_test_executable" if kind == "missing" else Path(sys.executable)
        lines.append(f'add_test({name} "{executable.as_posix()}" "-c" "{command}")')
        if kind == "skipped":
            lines.append(f"set_tests_properties({name} PROPERTIES SKIP_RETURN_CODE 77)")
        elif kind == "regex":
            lines.append(f'set_tests_properties({name} PROPERTIES SKIP_REGULAR_EXPRESSION "skip this test")')
        elif kind == "disabled":
            lines.append(f"set_tests_properties({name} PROPERTIES DISABLED TRUE)")
    (directory / "CTestTestfile.cmake").write_text("\n".join(lines) + "\n", encoding="utf-8")
    report = directory / "results.xml"
    result = subprocess.run([str(ctest), "--test-dir", str(directory), "--output-junit", str(report)],
                            capture_output=True, encoding="utf-8", errors="replace", timeout=10)
    assert result.returncode != 0, (result.stdout, result.stderr)
    assert report.is_file(), (result.stdout, result.stderr)
    return report


def main():
    ctest = Path(sys.argv[1]).resolve()
    tool = Path(compare_test_results.__file__)
    with tempfile.TemporaryDirectory(prefix="ctest comparison 界 ") as temporary:
        root = Path(temporary)
        baseline = run_ctest(ctest, root / "baseline", {
            "unchanged": "passed", "regression": "passed", "known": "failed", "repaired": "failed",
            "removed": "passed", "skip_code": "passed", "skip_regex": "passed", "disabled": "passed",
            "not_started": "passed", "resumed": "skipped",
        })
        candidate = run_ctest(ctest, root / "candidate", {
            "unchanged": "passed", "regression": "failed", "known": "failed", "repaired": "passed",
            "skip_code": "skipped", "skip_regex": "regex", "disabled": "disabled", "not_started": "missing",
            "resumed": "passed", "added": "passed", "added_failure": "failed",
        })
        before, after = baseline.read_bytes(), candidate.read_bytes()
        result = subprocess.run([sys.executable, str(tool), str(baseline), str(candidate), "--json"],
                                capture_output=True, encoding="utf-8", timeout=5)
        assert result.returncode == 1, (result.stdout, result.stderr)
        report = json.loads(result.stdout)
        assert report["baseline"] == {"total": 10, "passed": 7, "failed": 2, "skipped": 1}, report
        assert report["candidate"] == {"total": 11, "passed": 4, "failed": 4, "skipped": 3}, report
        assert report["changes"] == {
            "new_failure": 3, "existing_failure": 1, "fixed": 1, "newly_skipped": 3,
            "resumed": 1, "added": 1, "missing": 1, "unchanged": 1,
        }, report
        tests = {test["name"]: test for test in report["tests"]}
        assert tests["not_started"]["candidate"] == "failed", tests
        assert tests["removed"]["candidate"] is None, tests
        assert tests["added_failure"]["baseline"] is None, tests
        result = subprocess.run([sys.executable, str(tool), str(baseline), str(candidate)],
                                capture_output=True, encoding="utf-8", timeout=5)
        assert result.returncode == 1, (result.stdout, result.stderr)
        assert "| <code>known</code> | failed | failed | Existing failure |" in result.stdout, result.stdout
        result = subprocess.run([sys.executable, str(tool), str(baseline), str(baseline), "--json"],
                                capture_output=True, encoding="utf-8", timeout=5)
        assert result.returncode == 0, (result.stdout, result.stderr)
        control = json.loads(result.stdout)
        assert not control["has_regressions"] and control["changes"]["existing_failure"] == 2, control
        assert baseline.read_bytes() == before and candidate.read_bytes() == after
    print("Real CTest reports: changes, skips, missing executables and unchanged-baseline control passed")


if __name__ == "__main__":
    main()
