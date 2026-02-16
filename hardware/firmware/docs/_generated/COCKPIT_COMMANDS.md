# Cockpit Commands

Generated from tools/dev/cockpit_commands.yaml. Do not edit manually.

| ID | Description | Entrypoint | Args | Runbook | Evidence |
| --- | --- | --- | --- | --- | --- |
| rc | Run RC live gate (build matrix + smoke) | tools/dev/cockpit.sh | rc | docs/QUICKSTART.md |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
| rc-autofix | Run RC live gate with auto-fix helper | tools/dev/cockpit.sh | rc-autofix | docs/QUICKSTART.md |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
| build | Build all firmware targets | tools/dev/cockpit.sh | build | README.md |  |
| bootstrap | Bootstrap local tooling (.venv, pyserial) | tools/dev/cockpit.sh | bootstrap | docs/QUICKSTART.md |  |
| ports | Watch serial ports list | tools/dev/cockpit.sh | ports | docs/QUICKSTART.md |  |
| latest | Print latest artifact directory | tools/dev/cockpit.sh | latest | docs/QUICKSTART.md |  |
| audit | Run full audit (build + rc + driver/test checks) | tools/dev/cockpit.sh | audit | docs/TEST_SCRIPT_COORDINATOR.md |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
| report | Generate sync report | tools/dev/cockpit.sh | report | docs/TEST_SCRIPT_COORDINATOR.md |  |
|  |  |  |  |  |  |
| cleanup | Archive old logs and artifacts | tools/dev/cockpit.sh | cleanup | docs/TEST_SCRIPT_COORDINATOR.md |  |
|  |  |  |  |  |  |
|  |  |  |  |  |  |
| codex-check | Check codex CLI + prompts inventory | tools/dev/cockpit.sh | codex-check | docs/TEST_SCRIPT_COORDINATOR.md |  |
| drivers | Run driver audit for a specific platform | tools/dev/cockpit.sh | drivers<br><platform> | docs/TEST_SCRIPT_COORDINATOR.md |  |
| test | Run tests audit for a specific platform | tools/dev/cockpit.sh | test<br><platform> | docs/TEST_SCRIPT_COORDINATOR.md |  |
| git | Run git commands via cockpit (status, diff, log, branch, show, add, commit, stash, push) | tools/dev/cockpit.sh | git<br><action><br>[args...] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-add | Stage files for commit (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>add<br><pathspec> | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-commit | Commit staged changes (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>commit<br>-m<br><message> | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-stash | Stash working tree changes (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>stash<br>[save|pop|list] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-push | Push commits to remote (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>push<br>[remote]<br>[branch] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| flash | Flash firmware (auto port resolution) | tools/dev/cockpit.sh | flash | docs/QUICKSTART.md |  |
| help | Print cockpit help | tools/dev/cockpit.sh | help | docs/QUICKSTART.md |  |
