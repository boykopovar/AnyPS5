import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOW = REPOSITORY / ".github/workflows/progress-comment.yml"
KEY = re.compile(r"^( *)([A-Za-z0-9_-]+):(?:\s+(.*))?$")
SUCCESS = "github.event.workflow_run.conclusion == 'success'"
HEAD_GROUP = "progress-comment-${{ github.event.workflow_run.head_sha }}-"
PR_GROUP = "${{ github.event.workflow_run.pull_requests[0].number || github.event.workflow_run.id }}"


class Node:
    def __init__(self, value=""):
        self.value = value
        self.children = {}


def parse_mapping(text):
    root = Node()
    stack = [(-1, root)]
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("#") or line.lstrip().startswith("-"):
            continue
        match = KEY.match(line)
        if match is None:
            continue
        indent = len(match.group(1))
        while stack[-1][0] >= indent:
            stack.pop()
        node = Node(match.group(3) or "")
        stack[-1][1].children[match.group(2)] = node
        stack.append((indent, node))
    return root


def problems(text):
    root = parse_mapping(text)
    found = []
    if "concurrency" in root.children:
        found.append("concurrency is set at workflow level")
    jobs = root.children.get("jobs", Node()).children
    grouped = [name for name, job in jobs.items() if "concurrency" in job.children]
    if not grouped:
        found.append("no job sets concurrency")
    for name in grouped:
        job = jobs[name]
        condition = job.children.get("if")
        if condition is None or SUCCESS not in condition.value:
            found.append(f"job {name} sets concurrency without the success condition")
        concurrency = job.children["concurrency"].children
        group = concurrency.get("group")
        if group is None or not group.value.startswith(HEAD_GROUP):
            found.append(f"job {name} group is not keyed by head_sha first")
        elif not group.value.endswith(PR_GROUP):
            found.append(f"job {name} group does not identify the PR")
        cancel = concurrency.get("cancel-in-progress")
        if cancel is None or cancel.value != "true":
            found.append(f"job {name} does not cancel superseded runs")
    return found


OLD_WORKFLOW = """\
name: Progress comment
on:
  workflow_run:
    workflows: [Progress report]
    types: [completed]
concurrency:
  group: progress-comment-${{ github.event.workflow_run.head_sha }}
  cancel-in-progress: true
jobs:
  comment:
    if: github.event.workflow_run.conclusion == 'success'
    runs-on: ubuntu-latest
    steps:
      - run: echo
"""

WRONG_JOB = """\
jobs:
  comment:
    if: github.event.workflow_run.conclusion == 'success'
    runs-on: ubuntu-latest
    steps:
      - run: echo
  other:
    runs-on: ubuntu-latest
    concurrency:
      group: progress-comment-${{ github.event.workflow_run.head_sha }}-${{ github.event.workflow_run.pull_requests[0].number || github.event.workflow_run.id }}
      cancel-in-progress: true
    steps:
      - run: echo
"""


class ProgressCommentConcurrencyTests(unittest.TestCase):
    def test_group_sits_on_the_job_that_carries_the_success_condition(self):
        self.assertEqual(problems(WORKFLOW.read_text(encoding="utf-8")), [])

    def test_workflow_level_group_fails(self):
        self.assertIn("concurrency is set at workflow level", problems(OLD_WORKFLOW))

    def test_group_on_a_job_without_the_success_condition_fails(self):
        self.assertIn("job other sets concurrency without the success condition", problems(WRONG_JOB))


if __name__ == "__main__":
    unittest.main()
