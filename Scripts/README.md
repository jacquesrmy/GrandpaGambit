# Gambit Validation Script

`Run-GambitValidation.ps1` is the local validation entrypoint for larger Grandpa Gambit foundation passes.

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1
```

By default it uses Unreal 5.7 at:

```text
C:\Program Files (x86)\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
```

Use a custom editor commandlet path when needed:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1 -UnrealEditorCmdPath "C:\Program Files (x86)\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
```

Useful modes:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1 -SkipBuild
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1 -SkipAutomation
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1 -AuditOnly
```

`-AuditOnly` skips the editor build and automation suite, then runs the data audit, audit-output checks, and `git diff --check`.

The full script runs:

- `GrandpaGambitEditor Win64 Development` build.
- `GambitGameplayDataAudit` with `Docs/Audits/DataAssetsAuditExclusions.json`.
- Non-empty checks for `Saved/Automation/GambitDataAssetsAudit.json` and `.csv`.
- `Automation RunTests GrandpaGambit`.
- `git diff --check`.

Per-run logs and automation reports are written under:

```text
Saved/Automation/GambitValidation/<timestamp>/
```

Use it after broad architecture, gameplay foundation, validation, audit, or debug-tooling passes, and before handing off a change that should be trusted by the next implementation pass.
