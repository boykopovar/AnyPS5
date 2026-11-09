import argparse
import json
import os
import re
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

REPOSITORY = "boykopovar/AnyPS5"


def api(path, *, data=None, missing_ok=False):
    request = Request(
        f"https://api.github.com/{path}",
        data=None if data is None else json.dumps(data).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {os.environ['GH_TOKEN']}",
            "Accept": "application/vnd.github+json",
            "Content-Type": "application/json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    try:
        with urlopen(request, timeout=30) as response:
            return json.load(response)
    except HTTPError as error:
        if missing_ok and data is None and error.code == 404:
            return None
        raise


def nightly_tag(sha):
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("A full commit SHA is required")
    return f"v-nightly-{sha}"


def find_release(tag):
    result = api("graphql", data={
        "query": (
            "query($owner: String!, $name: String!, $tag: String!) {"
            " repository(owner: $owner, name: $name) {"
            " release(tagName: $tag) { databaseId } } }"
        ),
        "variables": {"owner": "boykopovar", "name": "AnyPS5", "tag": tag},
    })
    if result.get("errors") or not result.get("data", {}).get("repository"):
        raise ValueError("Cannot query the nightly release")
    found = result["data"]["repository"]["release"]
    if found is None:
        return None
    release_id = found["databaseId"]
    if type(release_id) is not int or release_id <= 0:
        raise ValueError("Invalid release ID")
    return api(f"repos/{REPOSITORY}/releases/{release_id}")


def matches_reference(ref, sha):
    return (isinstance(ref, dict) and ref.get("ref") == f"refs/tags/{nightly_tag(sha)}"
            and ref.get("object", {}).get("type") == "commit"
            and ref["object"].get("sha") == sha)


def state(sha):
    tag = nightly_tag(sha)
    release = find_release(tag)
    if release is None:
        return "missing"
    marker = f"<!-- anyps5-release-workflow:{sha} -->"
    if (release.get("tag_name") != tag or release.get("prerelease") is not True
            or release.get("target_commitish") != sha
            or marker not in (release.get("body") or "")
            or type(release.get("draft")) is not bool):
        raise ValueError(f"Release {tag} is not owned by this workflow for {sha}")
    if release["draft"]:
        return "draft"
    ref = api(f"repos/{REPOSITORY}/git/ref/tags/{tag}", missing_ok=True)
    if not matches_reference(ref, sha):
        raise ValueError(f"Published release {tag} does not point to {sha}")
    return "published"


def ensure_tag(sha):
    tag = nightly_tag(sha)
    ref = api(f"repos/{REPOSITORY}/git/ref/tags/{tag}", missing_ok=True)
    if ref is None:
        ref = api(f"repos/{REPOSITORY}/git/refs", data={"ref": f"refs/tags/{tag}", "sha": sha})
    if not matches_reference(ref, sha):
        raise ValueError(f"Refusing to move conflicting tag {tag}")


def plan():
    if os.environ["GITHUB_REPOSITORY"] != REPOSITORY:
        raise ValueError("Releases run only in the upstream repository")
    sha = os.environ["GITHUB_SHA"]
    nightly_tag(sha)
    event, ref = os.environ["GITHUB_EVENT_NAME"], os.environ["GITHUB_REF"]
    if event == "push":
        tag = ref.removeprefix("refs/tags/")
        if (not ref.startswith("refs/tags/") or tag.startswith("v-nightly-")
                or not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", tag) or tag.endswith(".")):
            raise ValueError("Stable releases require a non-nightly version tag")
        return {"tag": tag, "nightly": "false", "build": "true"}
    if event not in ("schedule", "workflow_dispatch") or ref != "refs/heads/main":
        raise ValueError("Nightly releases require a scheduled or manual run on main")
    return {"tag": nightly_tag(sha), "nightly": "true", "build": str(state(sha) != "published").lower()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("plan", "state", "ensure-tag"))
    args = parser.parse_args()
    if args.command == "plan":
        outputs = plan()
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as stream:
            for name, value in outputs.items():
                stream.write(f"{name}={value}\n")
    else:
        if (os.environ["GITHUB_REPOSITORY"] != REPOSITORY
                or os.environ["GITHUB_EVENT_NAME"] not in ("schedule", "workflow_dispatch")
                or os.environ["GITHUB_REF"] != "refs/heads/main"):
            raise ValueError("Nightly publication requires an upstream main run")
        sha = os.environ["GITHUB_SHA"]
        if args.command == "state":
            print(state(sha))
        else:
            ensure_tag(sha)


if __name__ == "__main__":
    main()
