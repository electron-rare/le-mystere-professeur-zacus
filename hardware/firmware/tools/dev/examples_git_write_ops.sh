#!/usr/bin/env bash

# Examples: Using cockpit.sh for git write operations via scripts (not agents)
# This saves token credits by executing git commands as shell scripts instead of agents.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_ROOT="$SCRIPT_DIR/../.."

# --- Example 1: Stage files ---
example_git_add() {
  local msg="Example: Stage hardware firmware changes"
  echo
  echo "==== $msg ===="
  echo
  
  # Enable git write + suppress confirmation prompt
  export ZACUS_GIT_ALLOW_WRITE=1
  export ZACUS_GIT_NO_CONFIRM=1
  
  echo "[step] Stage firmware files"
  $SCRIPT_DIR/cockpit.sh git add hardware/firmware/src/
  
  echo "[step] Check status"
  $SCRIPT_DIR/cockpit.sh git status
}

# --- Example 2: Commit with confirmation (interactive) ---
example_git_commit_interactive() {
  local msg="Example: Commit staged changes (with confirmation prompt)"
  echo
  echo "==== $msg ===="
  echo
  
  # Enable git write but keep confirmation enabled (user will be prompted)
  export ZACUS_GIT_ALLOW_WRITE=1
  unset ZACUS_GIT_NO_CONFIRM
  
  echo "[step] Commit with message"
  $SCRIPT_DIR/cockpit.sh git commit -m "Update firmware implementation"
  
  echo "[step] Show commit log (last 5)"
  $SCRIPT_DIR/cockpit.sh git log 5
}

# --- Example 3: Commit without confirmation (CI/CD-friendly) ---
example_git_commit_silent() {
  local msg="Example: Commit silently (no confirmation prompt)"
  echo
  echo "==== $msg ===="
  echo
  
  # Enable git write + suppress confirmation for CI/CD pipelines
  export ZACUS_GIT_ALLOW_WRITE=1
  export ZACUS_GIT_NO_CONFIRM=1
  
  echo "[step] Commit firmware changes"
  $SCRIPT_DIR/cockpit.sh git commit -m "Bump firmware version to v1.2.3"
  
  echo "[step] Show latest commit details"
  $SCRIPT_DIR/cockpit.sh git show
}

# --- Example 4: Stash changes ---
example_git_stash() {
  local msg="Example: Stash uncommitted changes"
  echo
  echo "==== $msg ===="
  echo
  
  export ZACUS_GIT_ALLOW_WRITE=1
  export ZACUS_GIT_NO_CONFIRM=1
  
  echo "[step] Create stash"
  $SCRIPT_DIR/cockpit.sh git stash save "paused work on ui-link"
  
  echo "[step] List stashes"
  $SCRIPT_DIR/cockpit.sh git stash list
}

# --- Example 5: Push to remote ---
example_git_push() {
  local msg="Example: Push commits to remote (⚠️ dangerous, requires explicit confirmation)"
  echo
  echo "==== $msg ===="
  echo
  
  export ZACUS_GIT_ALLOW_WRITE=1
  # Note: Push requires explicit user confirmation (ZACUS_GIT_NO_CONFIRM won't suppress this)
  
  echo "[step] Show local commits not yet pushed"
  $SCRIPT_DIR/cockpit.sh git log -5
  
  echo "[step] Push to origin"
  # This will prompt for confirmation even with ZACUS_GIT_NO_CONFIRM
  # (intentional safety feature for push operations)
  $SCRIPT_DIR/cockpit.sh git push origin story-V2
}

# --- Example 6: In evidence context (initialized via cockpit) ---
example_git_with_evidence() {
  local msg="Example: Git write operations with evidence tracking"
  echo
  echo "==== $msg ===="
  echo
  
  # First, initialize evidence (this would normally be done by a test script)
  # evidence_init is usually called by test runners like run_rc_gate.sh
  
  cd "$FW_ROOT"
  
  export ZACUS_GIT_ALLOW_WRITE=1
  export ZACUS_GIT_NO_CONFIRM=1
  
  # In a real script, evidence would be initialized like:
  #   evidence_init "git_operations" "artifacts/git_ops/$(date +%Y%m%d-%H%M%S)"
  # Then commands are recorded automatically in EVIDENCE_COMMANDS
  
  echo "[step] Stage changes"
  $SCRIPT_DIR/cockpit.sh git add src/
  
  echo "[step] Commit"
  $SCRIPT_DIR/cockpit.sh git commit -m "Update source code"
  
  echo "[step] Status after commit"
  $SCRIPT_DIR/cockpit.sh git status
  
  # All commands are logged to:
  # - EVIDENCE_COMMANDS (plain text log)
  # - EVIDENCE_SUMMARY.md (human-readable)
}

# --- Example 7: Error case - missing ZACUS_GIT_ALLOW_WRITE ---
example_git_error_missing_env() {
  local msg="Example: Error case - missing ZACUS_GIT_ALLOW_WRITE"
  echo
  echo "==== $msg ===="
  echo
  
  # Try without setting ZACUS_GIT_ALLOW_WRITE
  unset ZACUS_GIT_ALLOW_WRITE
  
  echo "[step] Try git add without ZACUS_GIT_ALLOW_WRITE=1"
  if $SCRIPT_DIR/cockpit.sh git add src/ 2>&1 | grep -q "ZACUS_GIT_ALLOW_WRITE"; then
    echo "✓ Correctly rejected - ZACUS_GIT_ALLOW_WRITE required"
  fi
}

# --- Example 8: Scripting use case - automated release tagging ---
example_release_automation() {
  local msg="Example: Automated release - tag and push"
  echo
  echo "==== $msg ===="
  echo
  
  export ZACUS_GIT_ALLOW_WRITE=1
  export ZACUS_GIT_NO_CONFIRM=1
  
  # This would be part of an automated release pipeline
  local version="v1.2.3"
  
  echo "[step] Verify clean working tree"
  $SCRIPT_DIR/cockpit.sh git status
  
  echo "[step] Would tag: $version"
  # In automated context, commands would be logged to EVIDENCE_COMMANDS
  # Real implementation would call: git_cmd tag -a "$version" -m "Release $version"
  
  echo "[step] Would push tag"
  # $SCRIPT_DIR/cockpit.sh git push origin "$version"
}

# --- Usage ---
show_usage() {
  echo "Git Write Operations Examples - Save tokens by using cockpit.sh scripts"
  echo
  echo "Usage: $0 <example_number>"
  echo
  echo "Examples:"
  echo "  1. Stage files (git add)"
  echo "  2. Commit with confirmation prompt (interactive)"
  echo "  3. Commit silently (CI/CD-friendly)"
  echo "  4. Stash changes"
  echo "  5. Push to remote (dangerous, requires confirmation)"
  echo "  6. Git write operations with evidence tracking"
  echo "  7. Error case - missing ZACUS_GIT_ALLOW_WRITE"
  echo "  8. Release automation example"
  echo
  echo "To run: $0 1-8"
}

# --- Main ---
if [[ $# -eq 0 ]]; then
  show_usage
  exit 0
fi

case "$1" in
  1) example_git_add ;;
  2) example_git_commit_interactive ;;
  3) example_git_commit_silent ;;
  4) example_git_stash ;;
  5) example_git_push ;;
  6) example_git_with_evidence ;;
  7) example_git_error_missing_env ;;
  8) example_release_automation ;;
  *)
    echo "Unknown example: $1"
    show_usage
    exit 1
    ;;
esac
