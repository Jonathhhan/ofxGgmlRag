param(
	[string]$Configuration = "Release",
	[string]$BuildDir = "",
	[switch]$Clean,
	[switch]$DryRun,
	[switch]$Json,
	[switch]$SummaryOnly
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Get-PowerShellExecutable {
	$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
	if ($pwsh) {
		return $pwsh.Source
	}

	$windowsPowerShell = Get-Command powershell -ErrorAction SilentlyContinue
	if ($windowsPowerShell) {
		return $windowsPowerShell.Source
	}

	throw "Could not find pwsh or powershell."
}

function Test-RuntimeSmokeReady {
	return (Test-Path -LiteralPath (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagTypes.h") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagUtils.cpp") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonRoot "tests\test_main.cpp") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonsRoot "ofxGgmlCore") -PathType Container)
}

function New-DryRunSummary {
	$ready = Test-RuntimeSmokeReady
	return [ordered]@{
		Name = "ofxGgmlRag runtime smoke"
		Root = [string]$addonRoot
		Backend = "request-boundary"
		BuildDir = $BuildDir
		Ready = $ready
		ModelBacked = $false
		IndexBacked = $false
		TestScript = $testScript
		DoctorScript = $doctorScript
		NextCommands = @(
			"scripts\run-rag-runtime-smoke.bat -Json -SummaryOnly",
			"scripts\test-addon.bat",
			"scripts\doctor-rag.bat"
		)
	}
}

function Invoke-SmokeStep {
	param(
		[string]$Name,
		[string[]]$Arguments
	)

	$output = @()
	$exitCode = 0
	try {
		$output = & $powerShell @Arguments 2>&1 | ForEach-Object { "$_" }
		$exitCode = $LASTEXITCODE
	} catch {
		$output += "$_"
		$exitCode = 1
	}

	return [ordered]@{
		Name = $Name
		Passed = ($exitCode -eq 0)
		ExitCode = $exitCode
		Output = $output
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Split-Path -Parent $addonRoot
$testScript = Join-Path $scriptRoot "test-addon.ps1"
$doctorScript = Join-Path $scriptRoot "doctor-rag.ps1"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlRag-runtime-smoke"
}

if ($DryRun) {
	$summary = New-DryRunSummary
	if ($Json) {
		$summary | ConvertTo-Json -Depth 5
		return
	}

	Write-Step "ofxGgmlRag runtime smoke plan"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  BuildDir: $($summary.BuildDir)"
	Write-Host "  Ready: $($summary.Ready)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  IndexBacked: $($summary.IndexBacked)"
	Write-Host "  Test: $($summary.TestScript)"
	Write-Host "  Doctor: $($summary.DoctorScript)"
	Write-Host "  Next: $($summary.NextCommands[0])"
	Write-Step "Dry run complete; no files were changed"
	return
}

$started = Get-Date
$powerShell = Get-PowerShellExecutable
$testArgs = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$testScript,
	"-Configuration",
	$Configuration,
	"-BuildDir",
	$BuildDir
)
if ($Clean) {
	$testArgs += "-Clean"
}
$doctorArgs = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$doctorScript,
	"-Json"
)

$results = @()
$results += Invoke-SmokeStep -Name "request helper tests" -Arguments $testArgs
$results += Invoke-SmokeStep -Name "RAG doctor" -Arguments $doctorArgs

$failed = @($results | Where-Object { -not $_.Passed })
$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$summary = [ordered]@{
	Name = "ofxGgmlRag runtime smoke"
	Passed = ($failed.Count -eq 0)
	Backend = "request-boundary"
	Configuration = $Configuration
	BuildDir = $BuildDir
	ModelBacked = $false
	IndexBacked = $false
	ResultCount = $results.Count
	FailedCount = $failed.Count
	ElapsedMs = $elapsedMs
	Error = $(if ($failed.Count -eq 0) { "" } else { (($failed | ForEach-Object { $_.Output }) -join "`n") })
}

if ($Json) {
	if ($SummaryOnly) {
		$summary | ConvertTo-Json -Depth 5
	} else {
		[ordered]@{
			Summary = $summary
			Results = $results
		} | ConvertTo-Json -Depth 6
	}
} else {
	foreach ($result in $results) {
		Write-Step $result.Name
		foreach ($line in $result.Output) {
			Write-Host $line
		}
	}
	Write-Step "ofxGgmlRag runtime smoke summary"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  IndexBacked: $($summary.IndexBacked)"
	Write-Host "  Passed: $($summary.Passed)"
	Write-Host "  ElapsedMs: $($summary.ElapsedMs)"
}

if ($failed.Count -gt 0) {
	exit 1
}
