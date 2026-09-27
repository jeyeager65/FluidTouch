#!/usr/bin/env python3
"""Fail if a component's packaged files changed without a version bump.

For every component declared in .github/workflows/upload_components.yml
this compares the files under the component against the target branch. If
any changed but the `version:` in its idf_component.yml was not bumped, a
new version would never reach the registry (the action skips
already-published versions) -- so fail CI here, loudly, instead of
shipping a silent no-op. The root component is scoped to its own files:
the sub-component directories and repo infrastructure are excluded.

Opt-out: include the token "[skip-version-bump]" in any commit message in
the merge request's range to mark the change as intentional (e.g. a docs
or badge tweak you don't want to publish as a release). The check then
reports what it found and passes.

The component list and name resolution are shared with
validate_components.py so both jobs agree on what a "component" is.
"""
import os
import sys
from pathlib import Path

from validate_components import (
    find_action_step,
    git,
    parse_components,
    read_version,
    root_scope,
)

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/upload_components.yml"
# Branch to compare against; origin/master for the default branch pipelines.
BASE = os.environ.get("VERSION_BUMP_BASE", "origin/master")
SKIP_TOKEN = "[skip-version-bump]"


def main():
    import yaml

    if not WORKFLOW.is_file():
        sys.exit(f"Workflow not found: {WORKFLOW}")
    step = find_action_step(yaml.safe_load(WORKFLOW.read_text()))
    if step is None:
        sys.exit(f"No upload-components-ci-action step found in {WORKFLOW}")
    components = parse_components((step.get("with") or {}).get("components") or "")

    # Best-effort refresh of the base ref; ignore failures so the check can
    # also be run locally offline against an already-fetched ref.
    branch = BASE.split("/", 1)[-1]
    git("fetch", "--quiet", "origin", branch)
    base = git("merge-base", BASE, "HEAD").stdout.strip() or BASE

    # The opt-out token may live in any commit introduced on this branch.
    commit_msgs = git("log", "--format=%B", f"{base}..HEAD").stdout
    opted_out = SKIP_TOKEN.lower() in commit_msgs.lower()

    violations = []
    for name, path, _entry in components:
        full = (ROOT / path).resolve()
        is_root = full == ROOT
        scope = root_scope(components) if is_root else [path]
        changed = git(
            "diff", "--name-only", f"{base}..HEAD", "--", *scope
        ).stdout.strip()
        if not changed:
            continue
        rel = "idf_component.yml" if is_root else f"{path}/idf_component.yml"
        base_manifest = git("show", f"{base}:{rel}").stdout
        if not base_manifest.strip():
            continue  # new component on this branch: first publish, no bump needed
        base_ver = read_version(base_manifest)
        cur_ver = read_version((full / "idf_component.yml").read_text())
        if cur_ver == base_ver:
            label = name or full.name
            violations.append((label, path, cur_ver, changed.splitlines()))

    if not violations:
        print("OK: every changed component has a version bump (or none changed).")
        return

    print("Components changed without a version bump:")
    for label, path, ver, files in violations:
        print(f"  - {label} ({path}): still at {ver}")
        for f in files:
            print(f"      {f}")

    if opted_out:
        print(
            f"\n{SKIP_TOKEN} found in the branch's commit messages -- "
            f"treating the change as intentional, passing."
        )
        return

    print(
        f"\nBump 'version' in each component's idf_component.yml, or add "
        f"{SKIP_TOKEN} to a commit message if this change is intentional and "
        f"should not be published as a new version."
    )
    sys.exit(1)


if __name__ == "__main__":
    main()
