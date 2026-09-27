#!/usr/bin/env python3
"""Dry-run validate the components this push would publish.

Parses .github/workflows/upload_components.yml and resolves component
names exactly like espressif/upload-components-ci-action (split on
`[;\\n]`, then on the first `:`; an omitted name defaults to the path's
basename, except the repo root which requires an explicit name). Only
components whose version changed against the base ref are packed and
validated with `compote ... --dry-run`, so a malformed `components:` list
fails CI here instead of on push. A component still at its published
version cannot be uploaded at all, so there is nothing to validate.

If the base ref cannot be resolved (shallow clone, no history), every
component is validated.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ACTION = "espressif/upload-components-ci-action"
# Repo root is this script's parent directory's parent (ci/ -> repo root);
# component paths in the workflow are resolved relative to it.
ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/upload_components.yml"
# Repo infrastructure and build output: part of the root tarball, but not
# content a release would be cut for.
NON_SHIPPING = (
    ".github",
    ".gitignore",
    ".gitlab-ci.yml",
    ".pre-commit",
    ".pre-commit-config.yaml",
    "ci",
    "dist",
)


def git(*args):
    """Run a git command from the repo root."""
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=ROOT)


def base_ref():
    """Commit to diff against, or None if it can't be determined."""
    base = os.environ.get("VERSION_BUMP_BASE")
    if not base:
        target = (
            os.environ.get("CI_MERGE_REQUEST_TARGET_BRANCH_NAME")
            or os.environ.get("CI_DEFAULT_BRANCH")
            or "master"
        )
        base = f"origin/{target}"
    # Best-effort refresh; ignore failures so this also works offline.
    git("fetch", "--quiet", "origin", base.split("/", 1)[-1])

    merge_base = git("merge-base", base, "HEAD").stdout.strip()
    if not merge_base:
        return None
    if merge_base != git("rev-parse", "HEAD").stdout.strip():
        return merge_base

    # On the base branch itself: diff against what was there before the push.
    before = os.environ.get("CI_COMMIT_BEFORE_SHA", "").strip()
    if before and set(before) != {"0"}:
        if git("cat-file", "-e", f"{before}^{{commit}}").returncode == 0:
            return before
    return git("rev-parse", "HEAD~1").stdout.strip() or None


def read_version(text):
    for line in text.splitlines():
        m = re.match(r"""\s*version\s*:\s*["']?([^"'#\s]+)""", line)
        if m:
            return m.group(1)
    return None


def version_bumped(base, full):
    """True if this component's version differs from base, or is new there."""
    rel = full.relative_to(ROOT).as_posix()
    manifest = "idf_component.yml" if rel == "." else f"{rel}/idf_component.yml"
    old = git("show", f"{base}:{manifest}")
    if old.returncode != 0 or not old.stdout.strip():
        return True  # new component on this branch: first publish
    cur = read_version((full / "idf_component.yml").read_text())
    return read_version(old.stdout) != cur


def root_scope(components):
    """Pathspec for the root component: its own files, not the sub-components'."""
    others = [
        (ROOT / path).relative_to(ROOT).as_posix()
        for _name, path, _entry in components
        if (ROOT / path).resolve() != ROOT
    ]
    return [".", *(f":(exclude){p}" for p in [*others, *NON_SHIPPING])]


def find_action_step(workflow):
    for job in (workflow.get("jobs") or {}).values():
        for step in job.get("steps") or []:
            if isinstance(step, dict) and str(step.get("uses", "")).startswith(ACTION):
                return step
    return None


def parse_components(components_str):
    """Mirror upload.py: split on [;\\n], then first ':' into name/path."""
    items = []
    for raw in re.split(r"[;\n]", components_str):
        entry = raw.strip()
        if not entry:
            continue
        if ":" in entry:
            name, path = entry.split(":", 1)
            items.append((name.strip(), path.strip(), entry))
        else:
            items.append((None, entry, entry))
    return items


def main():
    import yaml

    if not WORKFLOW.is_file():
        sys.exit(f"Workflow not found: {WORKFLOW}")

    step = find_action_step(yaml.safe_load(WORKFLOW.read_text()))
    if step is None:
        sys.exit(f"No '{ACTION}' step found in {WORKFLOW}")

    with_ = step.get("with") or {}
    namespace = (with_.get("namespace") or "espressif").strip()
    components = parse_components(with_.get("components") or "")
    if not components:
        sys.exit("No components listed in the workflow 'components' input")

    base = base_ref()
    if base is None:
        print("Base ref unavailable; validating all components.", flush=True)
    else:
        print(f"Validating components bumped since {base[:12]}.", flush=True)

    failures = []
    validated = 0
    for name, path, entry in components:
        # Name resolution mirrors upload-components-ci-action v2 exactly: an
        # omitted name defaults to the path basename, except the repo root
        # which must be named explicitly. All other validation is delegated
        # to the compote dry-run below, as the action does.
        full = (ROOT / path).resolve()
        if name is None:
            if full == ROOT:
                failures.append(
                    f"{entry!r}: component in repo root needs an explicit "
                    f'name (e.g. "rmaker_common: .")'
                )
                continue
            name = full.name

        if base is not None and not version_bumped(base, full):
            print(
                f"==> Skipping {namespace}/{name} ({path}): version unchanged",
                flush=True,
            )
            continue

        validated += 1
        print(f"==> Validating {namespace}/{name} ({path})", flush=True)
        result = subprocess.run(
            [
                "compote", "component", "upload", "--dry-run",
                "--namespace", namespace, "--name", name,
                "--project-dir", str(full),
            ],
            capture_output=True,
            text=True,
        )
        # Echo compote's output so CI logs remain self-explanatory.
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if result.returncode != 0:
            # A version that is already published is not a validation error:
            # the component packed and validated fine, it just exists already.
            # The real upload-components-ci-action skips such versions rather
            # than failing, so mirror that here instead of breaking every MR
            # that doesn't bump a version.
            # compote wraps its output at the terminal width, so collapse
            # whitespace before matching on the message.
            combined = re.sub(r"\s+", " ", f"{result.stdout} {result.stderr}")
            if "already on the registry" in combined:
                print(
                    f"    (version already published; skipping like the "
                    f"upload action)",
                    flush=True,
                )
            else:
                failures.append(f"{namespace}/{name} ({path}): dry-run failed")

    if failures:
        print("\nComponent validation FAILED:")
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    if not validated:
        print("\nNo component version bumped; nothing to validate.")
    else:
        print(f"\n{validated} component(s) validated successfully.")


if __name__ == "__main__":
    main()
