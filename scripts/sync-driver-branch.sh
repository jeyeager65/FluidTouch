#!/usr/bin/env bash
set -euo pipefail

REPO_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE="origin"
MAIN_BRANCH="main"
DRIVER_BRANCH="waveshare-driver-stable"
MODE="rebase"
PUSH="false"
NO_STASH="false"

usage() {
  cat <<'EOF'
Usage: sync-driver-branch.sh [options]

Options:
  --repo PATH            Repository path (default: project root)
  --remote NAME          Git remote (default: origin)
  --main BRANCH          Main branch name (default: main)
  --driver BRANCH        Driver branch name (default: waveshare-driver-stable)
  --merge                Merge main into driver branch (default: rebase)
  --rebase               Rebase driver onto main (default)
  --push                 Push updated branches to remote
  --no-stash             Do not auto-stash dirty working tree
  -h, --help             Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      REPO_PATH="$2"; shift 2 ;;
    --remote)
      REMOTE="$2"; shift 2 ;;
    --main)
      MAIN_BRANCH="$2"; shift 2 ;;
    --driver)
      DRIVER_BRANCH="$2"; shift 2 ;;
    --merge)
      MODE="merge"; shift ;;
    --rebase)
      MODE="rebase"; shift ;;
    --push)
      PUSH="true"; shift ;;
    --no-stash)
      NO_STASH="true"; shift ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1 ;;
  esac
done

cd "$REPO_PATH"

git rev-parse --is-inside-work-tree >/dev/null

current_branch="$(git branch --show-current)"
if [[ -z "$current_branch" ]]; then
  echo "Detached HEAD is not supported. Checkout a branch first." >&2
  exit 1
fi

stashed="false"
stash_label=""
restore_stash() {
  if [[ "$stashed" == "true" ]]; then
    echo "Re-applying stashed changes ($stash_label)..."
    git stash pop
  fi
}

restore_branch() {
  git switch "$current_branch" >/dev/null
}

trap 'restore_branch; restore_stash' EXIT

if [[ "$NO_STASH" != "true" ]] && [[ -n "$(git status --porcelain)" ]]; then
  stash_label="auto-sync-$(date +%Y%m%d-%H%M%S)"
  echo "Stashing local changes as '$stash_label'..."
  git stash push -u -m "$stash_label" >/dev/null
  stashed="true"
fi

echo "Fetching $REMOTE..."
git fetch "$REMOTE" --prune

echo "Switching to $MAIN_BRANCH and fast-forwarding from $REMOTE/$MAIN_BRANCH..."
git switch "$MAIN_BRANCH"
git pull --ff-only "$REMOTE" "$MAIN_BRANCH"

echo "Switching to driver branch $DRIVER_BRANCH..."
git switch "$DRIVER_BRANCH"

backup_name="backup/${DRIVER_BRANCH}-$(date +%Y%m%d-%H%M%S)"
echo "Creating safety backup branch: $backup_name"
git branch "$backup_name"

if [[ "$MODE" == "merge" ]]; then
  echo "Merging $MAIN_BRANCH into $DRIVER_BRANCH..."
  git merge --no-ff --no-edit "$MAIN_BRANCH"
else
  echo "Rebasing $DRIVER_BRANCH onto $MAIN_BRANCH..."
  git rebase "$MAIN_BRANCH"
fi

if [[ "$PUSH" == "true" ]]; then
  echo "Pushing $MAIN_BRANCH to $REMOTE..."
  git push "$REMOTE" "$MAIN_BRANCH"

  echo "Pushing $DRIVER_BRANCH to $REMOTE..."
  if [[ "$MODE" == "merge" ]]; then
    git push "$REMOTE" "$DRIVER_BRANCH"
  else
    git push --force-with-lease "$REMOTE" "$DRIVER_BRANCH"
  fi
fi

echo "Sync complete. Driver branch '$DRIVER_BRANCH' now includes latest '$MAIN_BRANCH'."
