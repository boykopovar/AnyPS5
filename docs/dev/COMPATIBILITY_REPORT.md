# Compatibility report validation

`tools/compatibility_report.py` validates a minimal version 1 report contract for
issue #1671. It uses the Python 3 standard library. It does not launch a title,
collect metrics, generate summaries, or verify gameplay.

```
python3 tools/compatibility_report.py /path/to/session/report.json
python3 -m unittest discover -s tools/tests -p 'test_compatibility_report.py'
```

The command reads one UTF-8 JSON file, with an optional UTF-8 BOM. It does not
modify the file or open evidence references. Exit status `0` means the structure
is valid; `2` means invalid input, an unreadable file, or incorrect command-line
arguments. Errors identify the file and, for contract errors, the field. Duplicate
JSON keys and non-finite numbers are rejected.

## Version 1 contract

All fields in this table are required. Unknown additional fields are preserved
by the Python API but are outside this minimal validator's contract. Producers
must not interpret unvalidated extension fields as validated compatibility claims.
`validate_report(report)` returns `None` on success and raises `ReportError` on
failure without changing the input object.

| Field | Value |
|---|---|
| `schema_version` | Integer `1`; booleans and floating-point values are invalid |
| `report_id` | Nonempty session identifier; uniqueness across reports is the producer's responsibility |
| `title.id`, `title.version`, `host.os` | Nonempty string, `null`, or the literal `"unknown"` |
| `build.commit` | Full 40-character hexadecimal Git commit ID, `null`, or `"unknown"` |
| `session.exit_reason` | `not_run`, `conversion_failure`, `launch_failure`, `normal_exit`, `nonzero_exit`, `crash`, `user_stop`, `timeout`, or `unknown` |
| `session.exit_code` | Runtime process exit code as an integer, or `null`; booleans are invalid |
| `progress.level` | `not_tested`, `boots`, `reaches_gameplay`, `gameplay_tested`, or `completed` |
| `progress.objective` | Nonempty description of the tested objective, or `null`; required as text for `completed` |
| `progress.milestones` | List of observed progress milestones |
| `checks` | List of named check results; names must be unique within a report |
| `unavailable_data` | List of objects containing `field` and a nonempty `reason` |

Each milestone requires a nonempty `name`, an observation `source`, and a
nonempty `evidence` list. Each check requires a nonempty `name`, `result`,
`source`, nonempty `notes` explaining the result, and an `evidence` list.

Allowed sources are `collector`, `instrumentation`, `tester`, `replay`, and
`external_reference`. Allowed check results are `pass`, `fail`, `untested`, and
`not_applicable`. `pass` and `fail` require evidence references. `untested` and
`not_applicable` may have no references, but still require explanatory notes.

Evidence references are nonempty strings, preferably paths relative to the
report, or external links. The validator does not check existence, download
links, evaluate artifacts, or establish that a reference supports the claim.
Missing or truncated artifacts must remain explicit in notes or extension data;
a syntactically valid reference is not verification of an artifact bundle.

For `title.id`, `title.version`, `build.commit`, `host.os`, and
`session.exit_code`, each `null` value needs a matching `unavailable_data` entry.
The literal `"unknown"` is also allowed for identity fields and
`session.exit_reason`, with a matching reason. Unavailable entries must name one
of these six fields, must be unique, and cannot refer to a known value.
`progress.objective: null` for a non-completed report means no completion
objective is claimed and needs no unavailable-data entry.

`collector_errors` is optional. When present, it is a list of nonempty strings.
Collector errors are distinct from the runtime process exit reason. No errors,
unknown values, or check results are rewritten or promoted to success.

## Consistency rules

- Every observed level above `not_tested` requires at least one milestone with
  evidence references. `not_tested` has an empty progress milestone list; static
  analysis may be recorded in checks.
- `completed` also requires an objective. Completion does not imply that all
  quality checks passed. Failed and untested checks remain valid report data.
- `not_run`, `conversion_failure`, and `launch_failure` require `not_tested`
  progress and a `null` runtime exit code. Conversion-tool codes belong in
  separate extension data rather than the runtime code.
- `normal_exit` requires code zero or an explicitly unavailable code.
  `nonzero_exit` cannot have code zero. A normal exit never promotes progress.
- A crash, user stop, or timeout can occur after an observed milestone, including
  completion. These outcomes are preserved. A timeout is not classified as a
  guest deadlock.

This contract covers one session. Full reproducibility metadata, required-check
policies for each scenario, metrics, artifact integrity and linked-session
completion claims require further agreement. A valid report alone does not
establish that a title works on Windows, Linux, or another configuration.

## Synthetic partial report

This example and the unit-test fixtures are synthetic and assert no actual title
result. Store real reports and artifacts outside the source tree and link them
from issues or pull requests, following [CONTRIBUTING.md](../../CONTRIBUTING.md).

```json
{
  "schema_version": 1,
  "report_id": "synthetic-launch-failure",
  "title": {"id": null, "version": null},
  "build": {"commit": null},
  "host": {"os": null},
  "session": {"exit_reason": "launch_failure", "exit_code": null},
  "progress": {"level": "not_tested", "objective": null, "milestones": []},
  "checks": [
    {
      "name": "interactive_gameplay",
      "result": "untested",
      "source": "tester",
      "notes": "Synthetic launch failure; gameplay was not observed.",
      "evidence": []
    }
  ],
  "unavailable_data": [
    {"field": "title.id", "reason": "Synthetic fixture; no title was used."},
    {"field": "title.version", "reason": "Synthetic fixture; no title was used."},
    {"field": "build.commit", "reason": "Synthetic fixture; no build was run."},
    {"field": "host.os", "reason": "Synthetic fixture; no host run occurred."},
    {"field": "session.exit_code", "reason": "No process was started."}
  ]
}
```

Invalid examples can be reproduced by changing this fixture:

- Set `checks[0].result` to `"success"`: not an allowed check result.
- Remove the `build.commit` unavailable-data entry: a null commit lacks a reason.
- Set `progress.level` to `"completed"`: the objective and milestone evidence
  are absent, and the session outcome also contradicts observed completion.

The tests isolate missing objectives and missing evidence in otherwise launched
synthetic sessions. CTest registers the suite as `compatibility_report` when
`BUILD_TESTING` is enabled and Python is found, with a 30-second timeout.
