[CmdletBinding()]
param(
	[string]$UnrealEditorCmdPath = "C:\Program Files (x86)\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
	[switch]$SkipBuild,
	[switch]$SkipAutomation,
	[switch]$AuditOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$ProjectFile = Join-Path $ProjectRoot "GrandpaGambit.uproject"
$AuditJsonPath = Join-Path $ProjectRoot "Saved\Automation\GambitDataAssetsAudit.json"
$AuditCsvPath = Join-Path $ProjectRoot "Saved\Automation\GambitDataAssetsAudit.csv"
$AuditExclusionsPath = "Docs/Audits/DataAssetsAuditExclusions.json"
$RunStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$ValidationOutputRoot = Join-Path $ProjectRoot "Saved\Automation\GambitValidation"
$ValidationRunDir = Join-Path $ValidationOutputRoot $RunStamp
$BuildLogPath = Join-Path $ValidationRunDir "Build.log"
$AuditLogPath = Join-Path $ValidationRunDir "Audit.log"
$AuditStdoutLogPath = Join-Path $ValidationRunDir "AuditStdout.log"
$AutomationReportDir = Join-Path $ValidationRunDir "AutomationReport"
$AutomationLogPath = Join-Path $ValidationRunDir "Automation.log"
$AutomationStdoutLogPath = Join-Path $ValidationRunDir "AutomationStdout.log"

$EffectiveSkipBuild = $SkipBuild.IsPresent -or $AuditOnly.IsPresent
$EffectiveSkipAutomation = $SkipAutomation.IsPresent -or $AuditOnly.IsPresent

$Results = [ordered]@{
	Build = if ($EffectiveSkipBuild) { "Skipped" } else { "Pending" }
	Audit = "Pending"
	AuditOutputs = "Pending"
	Automation = if ($EffectiveSkipAutomation) { "Skipped" } else { "Pending" }
	DiffCheck = "Pending"
}

$AuditCounts = [ordered]@{
	TotalAssets = $null
	Valid = $null
	Warning = $null
	Error = $null
}

$AutomationCounts = [ordered]@{
	Succeeded = $null
	Warnings = $null
	Failed = $null
	NotRun = $null
	InProcess = $null
}

$Failures = New-Object System.Collections.Generic.List[string]

function Add-Failure {
	param([string]$Message)

	[void]$script:Failures.Add($Message)
}

function Quote-ForDisplay {
	param([string]$Value)

	if ($Value -match '[\s"]') {
		return '"' + ($Value -replace '"', '\"') + '"'
	}

	return $Value
}

function Format-CommandLine {
	param(
		[string]$FilePath,
		[string[]]$Arguments
	)

	$Parts = @($FilePath) + $Arguments
	return (($Parts | ForEach-Object { Quote-ForDisplay $_ }) -join " ")
}

function Invoke-ExternalCommand {
	param(
		[string]$Name,
		[string]$FilePath,
		[string[]]$Arguments,
		[string]$LogPath = ""
	)

	Write-Host ""
	Write-Host "== $Name =="
	Write-Host (Format-CommandLine -FilePath $FilePath -Arguments $Arguments)

	if ([string]::IsNullOrWhiteSpace($LogPath)) {
		& $FilePath @Arguments 2>&1 | ForEach-Object {
			Write-Host $_
		}
	}
	else {
		$LogParent = Split-Path -Parent $LogPath
		if (-not [string]::IsNullOrWhiteSpace($LogParent)) {
			New-Item -ItemType Directory -Path $LogParent -Force | Out-Null
		}
		& $FilePath @Arguments > $LogPath 2>&1
	}

	$ExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }

	if ($ExitCode -eq 0) {
		Write-Host "$Name OK"
	}
	else {
		Write-Host "$Name failed with exit code $ExitCode"
	}

	if (-not [string]::IsNullOrWhiteSpace($LogPath)) {
		Write-Host "Log: $LogPath"
	}

	return $ExitCode
}

function Resolve-UnrealEditorCmd {
	param([string]$Path)

	if (Test-Path -LiteralPath $Path -PathType Leaf) {
		return (Resolve-Path -LiteralPath $Path).Path
	}

	if (Test-Path -LiteralPath $Path -PathType Container) {
		$ProjectInstallCandidate = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
		if (Test-Path -LiteralPath $ProjectInstallCandidate -PathType Leaf) {
			return (Resolve-Path -LiteralPath $ProjectInstallCandidate).Path
		}

		$EngineRootCandidate = Join-Path $Path "Binaries\Win64\UnrealEditor-Cmd.exe"
		if (Test-Path -LiteralPath $EngineRootCandidate -PathType Leaf) {
			return (Resolve-Path -LiteralPath $EngineRootCandidate).Path
		}
	}

	throw "UnrealEditor-Cmd.exe was not found at '$Path'. Pass -UnrealEditorCmdPath with the UE 5.7 UnrealEditor-Cmd.exe path."
}

function Get-EngineRootFromEditorCmd {
	param([string]$EditorCmdPath)

	$Win64Dir = Split-Path -Parent $EditorCmdPath
	$BinariesDir = Split-Path -Parent $Win64Dir
	return Split-Path -Parent $BinariesDir
}

function Test-NonEmptyFile {
	param([string]$Path)

	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
		return $false
	}

	return ((Get-Item -LiteralPath $Path).Length -gt 0)
}

function Read-AuditCounts {
	param([string]$JsonPath)

	$Json = Get-Content -LiteralPath $JsonPath -Raw | ConvertFrom-Json
	$script:AuditCounts.TotalAssets = [int]$Json.summary.totalAssets
	$script:AuditCounts.Valid = [int]$Json.summary.validation.valid
	$script:AuditCounts.Warning = [int]$Json.summary.validation.warning
	$script:AuditCounts.Error = [int]$Json.summary.validation.error
}

function Read-AutomationCounts {
	param([string]$ReportPath)

	$Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
	$script:AutomationCounts.Succeeded = [int]$Report.succeeded
	$script:AutomationCounts.Warnings = [int]$Report.succeededWithWarnings
	$script:AutomationCounts.Failed = [int]$Report.failed
	$script:AutomationCounts.NotRun = [int]$Report.notRun
	$script:AutomationCounts.InProcess = [int]$Report.inProcess
}

function Write-Summary {
	Write-Host ""
	Write-Host "==== Gambit validation summary ===="
	Write-Host "Build:        $($script:Results.Build)"
	Write-Host "Audit:        $($script:Results.Audit)"
	Write-Host "Audit files:  $($script:Results.AuditOutputs)"

	if ($null -ne $script:AuditCounts.TotalAssets) {
		Write-Host "Audit counts: total=$($script:AuditCounts.TotalAssets), valid=$($script:AuditCounts.Valid), warning=$($script:AuditCounts.Warning), error=$($script:AuditCounts.Error)"
	}

	Write-Host "Automation:   $($script:Results.Automation)"

	if ($null -ne $script:AutomationCounts.Succeeded) {
		Write-Host "Test counts:  success=$($script:AutomationCounts.Succeeded), warning=$($script:AutomationCounts.Warnings), failed=$($script:AutomationCounts.Failed), notRun=$($script:AutomationCounts.NotRun), inProcess=$($script:AutomationCounts.InProcess)"
		Write-Host "Test report:  $script:AutomationReportDir"
	}

	Write-Host "Diff check:   $($script:Results.DiffCheck)"
	Write-Host "Run logs:     $script:ValidationRunDir"

	if ($script:Failures.Count -gt 0) {
		Write-Host ""
		Write-Host "Failures:"
		foreach ($Failure in $script:Failures) {
			Write-Host "- $Failure"
		}
	}
}

$ExitCode = 0
$PushedLocation = $false

try {
	Push-Location $ProjectRoot
	$PushedLocation = $true

	if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) {
		throw "Project file was not found at '$ProjectFile'."
	}

	$ResolvedEditorCmdPath = Resolve-UnrealEditorCmd -Path $UnrealEditorCmdPath
	$EngineRoot = Get-EngineRootFromEditorCmd -EditorCmdPath $ResolvedEditorCmdPath
	$BuildBatPath = Join-Path $EngineRoot "Build\BatchFiles\Build.bat"

	if (-not (Test-Path -LiteralPath $BuildBatPath -PathType Leaf)) {
		throw "Build.bat was not found at '$BuildBatPath'."
	}

	if (-not $EffectiveSkipBuild) {
		$BuildArguments = @(
			"GrandpaGambitEditor",
			"Win64",
			"Development",
			"-Project=$ProjectFile",
			"-WaitMutex",
			"-NoHotReloadFromIDE"
		)

		$BuildExitCode = Invoke-ExternalCommand -Name "Build GrandpaGambitEditor Win64 Development" -FilePath $BuildBatPath -Arguments $BuildArguments -LogPath $BuildLogPath
		if ($BuildExitCode -eq 0) {
			$Results.Build = "OK"
		}
		else {
			$Results.Build = "Failed"
			Add-Failure "Build failed with exit code $BuildExitCode."
		}
	}

	foreach ($OutputPath in @($AuditJsonPath, $AuditCsvPath)) {
		if (Test-Path -LiteralPath $OutputPath -PathType Leaf) {
			Remove-Item -LiteralPath $OutputPath -Force
		}
	}

	$AuditArguments = @(
		$ProjectFile,
		"-run=GambitGameplayDataAudit",
		"-unattended",
		"-nop4",
		"-nosplash",
		"-NullRHI",
		"ContentPath=/Game/GrandpaGambit/Data",
		"JsonPath=Saved/Automation/GambitDataAssetsAudit.json",
		"CsvPath=Saved/Automation/GambitDataAssetsAudit.csv",
		"MatrixExclusionsPath=$AuditExclusionsPath",
		"-FailOnValidationErrors",
		"-abslog=$AuditLogPath"
	)

	$AuditExitCode = Invoke-ExternalCommand -Name "GambitGameplayDataAudit" -FilePath $ResolvedEditorCmdPath -Arguments $AuditArguments -LogPath $AuditStdoutLogPath
	if ($AuditExitCode -eq 0) {
		$Results.Audit = "OK"
	}
	else {
		$Results.Audit = "Failed"
		Add-Failure "Gameplay data audit failed with exit code $AuditExitCode."
	}

	$AuditJsonOk = Test-NonEmptyFile -Path $AuditJsonPath
	$AuditCsvOk = Test-NonEmptyFile -Path $AuditCsvPath
	if ($AuditJsonOk -and $AuditCsvOk) {
		$Results.AuditOutputs = "OK"
	}
	else {
		$Results.AuditOutputs = "Failed"
		Add-Failure "Audit JSON/CSV outputs were not both generated and non-empty."
	}

	if ($AuditJsonOk) {
		try {
			Read-AuditCounts -JsonPath $AuditJsonPath
			if ($AuditCounts.Error -gt 0) {
				$Results.Audit = "Failed"
				Add-Failure "Audit JSON reported $($AuditCounts.Error) validation errors."
			}
		}
		catch {
			$Results.AuditOutputs = "Failed"
			Add-Failure "Audit JSON could not be parsed: $($_.Exception.Message)"
		}
	}

	if (-not $EffectiveSkipAutomation) {
		New-Item -ItemType Directory -Path $ValidationOutputRoot -Force | Out-Null

		$AutomationArguments = @(
			$ProjectFile,
			"-unattended",
			"-nop4",
			"-nosplash",
			"-NullRHI",
			"-DDC-ForceMemoryCache",
			"-ExecCmds=Automation RunTests GrandpaGambit; Quit",
			"-TestExit=Automation Test Queue Empty",
			"-ReportExportPath=$AutomationReportDir",
			"-abslog=$AutomationLogPath",
			"-stdout",
			"-FullStdOutLogOutput"
		)

		$AutomationExitCode = Invoke-ExternalCommand -Name "Automation RunTests GrandpaGambit" -FilePath $ResolvedEditorCmdPath -Arguments $AutomationArguments -LogPath $AutomationStdoutLogPath
		$AutomationReportPath = Join-Path $AutomationReportDir "index.json"

		if (Test-NonEmptyFile -Path $AutomationReportPath) {
			try {
				Read-AutomationCounts -ReportPath $AutomationReportPath
			}
			catch {
				Add-Failure "Automation report could not be parsed: $($_.Exception.Message)"
			}
		}
		else {
			Add-Failure "Automation report was not generated at '$AutomationReportPath'."
		}

		$AutomationCountsAvailable = $null -ne $AutomationCounts.Succeeded
		$AutomationReportOk = $AutomationCountsAvailable `
			-and $AutomationCounts.Succeeded -gt 0 `
			-and $AutomationCounts.Warnings -eq 0 `
			-and $AutomationCounts.Failed -eq 0 `
			-and $AutomationCounts.NotRun -eq 0 `
			-and $AutomationCounts.InProcess -eq 0

		if ($AutomationExitCode -eq 0 -and $AutomationReportOk) {
			$Results.Automation = "OK"
		}
		else {
			$Results.Automation = "Failed"
			if ($AutomationExitCode -ne 0) {
				Add-Failure "Automation command failed with exit code $AutomationExitCode."
			}
			if (-not $AutomationReportOk) {
				Add-Failure "Automation report contains failures, warnings, not-run tests, in-process tests, or zero executed tests."
			}
		}
	}

	$DiffExitCode = Invoke-ExternalCommand -Name "git diff --check" -FilePath "git" -Arguments @("diff", "--check")
	if ($DiffExitCode -eq 0) {
		$Results.DiffCheck = "OK"
	}
	else {
		$Results.DiffCheck = "Failed"
		Add-Failure "git diff --check failed with exit code $DiffExitCode."
	}
}
catch {
	Add-Failure "Script error: $($_.Exception.Message)"
}
finally {
	if ($PushedLocation) {
		Pop-Location
	}
}

if ($Failures.Count -gt 0) {
	$ExitCode = 1
}

Write-Summary
exit $ExitCode
